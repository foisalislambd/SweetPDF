#include "core/pdf/PdfContent.h"

#include "core/pdf/Geometry.h"
#include "core/pdf/MuContext.h"
#include "core/pdf/PdfDocument.h"
#include "core/util/Logging.h"

#include <QDateTime>

#include <mupdf/pdf.h>

namespace sweet::core::content {
namespace {

/// Copies a QImage into a MuPDF image. The caller owns the result and must drop
/// it. MuPDF wants tightly packed RGB or RGBA samples, so anything else is
/// converted first.
fz_image *toFzImage(fz_context *ctx, const QImage &source)
{
    // MuPDF pixmaps hold premultiplied samples, and Qt can convert to exactly
    // that layout, so the copy below is a plain row-by-row memcpy.
    const bool transparent = source.hasAlphaChannel();
    const QImage converted =
        source.convertToFormat(transparent ? QImage::Format_RGBA8888_Premultiplied
                                           : QImage::Format_RGB888);
    if (converted.isNull())
        return nullptr;

    fz_pixmap *pixmap = nullptr;
    fz_image *image = nullptr;
    fz_var(pixmap);
    fz_var(image);
    fz_try(ctx) {
        pixmap = fz_new_pixmap(ctx, fz_device_rgb(ctx), converted.width(), converted.height(),
                               nullptr, transparent ? 1 : 0);
        unsigned char *samples = fz_pixmap_samples(ctx, pixmap);
        const int stride = fz_pixmap_stride(ctx, pixmap);
        const qsizetype rowBytes = qsizetype(converted.width()) * (transparent ? 4 : 3);
        for (int y = 0; y < converted.height(); ++y)
            memcpy(samples + qsizetype(y) * stride, converted.constScanLine(y), size_t(rowBytes));

        image = fz_new_image_from_pixmap(ctx, pixmap, nullptr);
    }
    fz_always(ctx)
        fz_drop_pixmap(ctx, pixmap);
    fz_catch(ctx)
        fz_rethrow(ctx);

    return image;
}

} // namespace

QRectF fitToAspect(const QSizeF &imageSize, const QRectF &rect)
{
    if (imageSize.isEmpty() || rect.isEmpty())
        return rect;

    const QSizeF fitted = imageSize.scaled(rect.size(), Qt::KeepAspectRatio);
    return QRectF(rect.topLeft(), fitted);
}

int addImage(PdfDocument &document, int pageIndex, const QImage &image, const QRectF &rect)
{
    pdf_document *pdf = document.pdfHandle();
    if (!pdf || image.isNull() || rect.isEmpty() || pageIndex < 0
        || pageIndex >= document.pageCount()) {
        return 0;
    }

    fz_context *ctx = MuContext::forThread();
    int objectNumber = 0;

    {
        const std::lock_guard<std::mutex> guard(document.parseMutex());
        pdf_page *page = nullptr;
        fz_image *stamp = nullptr;
        fz_var(page);
        fz_var(stamp);
        fz_var(objectNumber);
        fz_try(ctx) {
            stamp = toFzImage(ctx, image);
            page = pdf_load_page(ctx, pdf, pageIndex);
            pdf_annot *annot = pdf_create_annot(ctx, page, PDF_ANNOT_STAMP);
            pdf_set_annot_rect(ctx, annot, toFzRect(rect));
            pdf_set_annot_stamp_image(ctx, annot, stamp);

            const int64_t now = QDateTime::currentSecsSinceEpoch();
            pdf_set_annot_creation_date(ctx, annot, now);
            pdf_set_annot_modification_date(ctx, annot, now);
            pdf_update_annot(ctx, annot);
            objectNumber = pdf_to_num(ctx, pdf_annot_obj(ctx, annot));
        }
        fz_always(ctx) {
            fz_drop_image(ctx, stamp);
            fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
        }
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not place the image:" << MuContext::lastError(ctx);
            return 0;
        }
    }

    if (objectNumber != 0) {
        if (auto page = document.page(pageIndex))
            page->discardDisplayList();
    }
    return objectNumber;
}

bool addLink(PdfDocument &document, int pageIndex, const QRectF &rect, const QString &uri)
{
    pdf_document *pdf = document.pdfHandle();
    if (!pdf || rect.isEmpty() || uri.isEmpty() || pageIndex < 0
        || pageIndex >= document.pageCount()) {
        return false;
    }

    fz_context *ctx = MuContext::forThread();
    const QByteArray target = uri.toUtf8();

    {
        const std::lock_guard<std::mutex> guard(document.parseMutex());
        pdf_page *page = nullptr;
        fz_link *link = nullptr;
        fz_var(page);
        fz_var(link);
        fz_try(ctx) {
            page = pdf_load_page(ctx, pdf, pageIndex);
            link = pdf_create_link(ctx, page, toFzRect(rect), target.constData());
        }
        fz_always(ctx) {
            fz_drop_link(ctx, link);
            fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
        }
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not create the link:" << MuContext::lastError(ctx);
            return false;
        }
    }

    if (auto page = document.page(pageIndex))
        page->discardDisplayList();
    return true;
}

QList<Link> linksOnPage(const PdfDocument &document, int pageIndex)
{
    QList<Link> links;
    if (pageIndex < 0 || pageIndex >= document.pageCount())
        return links;

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(document.parseMutex());

    fz_page *page = nullptr;
    fz_link *first = nullptr;
    fz_var(page);
    fz_var(first);
    fz_try(ctx) {
        page = fz_load_page(ctx, document.handle(), pageIndex);
        first = fz_load_links(ctx, page);
        for (fz_link *link = first; link; link = link->next)
            links.append({ toQRectF(link->rect), QString::fromUtf8(link->uri ? link->uri : "") });
    }
    fz_always(ctx) {
        fz_drop_link(ctx, first);
        fz_drop_page(ctx, page);
    }
    fz_catch(ctx) {
        qCWarning(lcPdf) << "Could not read the links on page" << pageIndex + 1 << ":"
                         << MuContext::lastError(ctx);
        return {};
    }

    return links;
}

bool removeLinkAt(PdfDocument &document, int pageIndex, const QPointF &at)
{
    pdf_document *pdf = document.pdfHandle();
    if (!pdf || pageIndex < 0 || pageIndex >= document.pageCount())
        return false;

    fz_context *ctx = MuContext::forThread();
    bool removed = false;

    {
        const std::lock_guard<std::mutex> guard(document.parseMutex());
        pdf_page *page = nullptr;
        fz_link *first = nullptr;
        fz_var(page);
        fz_var(first);
        fz_var(removed);
        fz_try(ctx) {
            page = pdf_load_page(ctx, pdf, pageIndex);
            first = fz_load_links(ctx, reinterpret_cast<fz_page *>(page));
            for (fz_link *link = first; link; link = link->next) {
                if (!toQRectF(link->rect).contains(at))
                    continue;
                pdf_delete_link(ctx, page, link);
                removed = true;
                break;
            }
        }
        fz_always(ctx) {
            fz_drop_link(ctx, first);
            fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
        }
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not remove the link:" << MuContext::lastError(ctx);
            return false;
        }
    }

    if (removed) {
        if (auto page = document.page(pageIndex))
            page->discardDisplayList();
    }
    return removed;
}

QString internalUri(const PdfDocument &document, int pageIndex)
{
    if (!document.isPdf() || pageIndex < 0 || pageIndex >= document.pageCount())
        return {};

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(document.parseMutex());

    char *uri = nullptr;
    fz_var(uri);
    fz_try(ctx) {
        const fz_link_dest dest = fz_make_link_dest_xyz(0, pageIndex, 0, 0, 0);
        uri = fz_format_link_uri(ctx, document.handle(), dest);
    }
    fz_catch(ctx) {
        qCWarning(lcPdf) << "Could not build a link target:" << MuContext::lastError(ctx);
        return {};
    }

    const QString formatted = QString::fromUtf8(uri ? uri : "");
    fz_free(ctx, uri);
    return formatted;
}

int pageForUri(const PdfDocument &document, const QString &uri)
{
    if (uri.isEmpty() || uri.contains(QStringLiteral("://")))
        return -1;

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(document.parseMutex());
    const QByteArray target = uri.toUtf8();

    int pageIndex = -1;
    fz_var(pageIndex);
    fz_try(ctx) {
        const fz_link_dest dest = fz_resolve_link_dest(ctx, document.handle(), target.constData());
        pageIndex = dest.loc.page;
    }
    fz_catch(ctx)
        return -1;

    return pageIndex >= 0 && pageIndex < document.pageCount() ? pageIndex : -1;
}

} // namespace sweet::core::content
