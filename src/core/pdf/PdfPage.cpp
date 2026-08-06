#include "core/pdf/PdfPage.h"

#include "core/pdf/Geometry.h"
#include "core/pdf/MuContext.h"
#include "core/util/Logging.h"

#include <QTransform>

#include <cmath>

namespace sweet::core {
namespace {

fz_matrix renderMatrix(qreal scale, int rotation)
{
    const float s = static_cast<float>(scale);
    return fz_pre_rotate(fz_scale(s, s), static_cast<float>(rotation));
}

/// Copies a MuPDF pixmap into a QImage that owns its own buffer.
QImage pixmapToImage(fz_context *ctx, fz_pixmap *pixmap)
{
    const int width = fz_pixmap_width(ctx, pixmap);
    const int height = fz_pixmap_height(ctx, pixmap);
    if (width <= 0 || height <= 0)
        return {};

    const int components = fz_pixmap_components(ctx, pixmap);
    const int stride = fz_pixmap_stride(ctx, pixmap);
    const unsigned char *samples = fz_pixmap_samples(ctx, pixmap);

    const QImage::Format format = components == 4 ? QImage::Format_ARGB32_Premultiplied
                                                 : QImage::Format_RGB888;
    return QImage(samples, width, height, stride, format).copy();
}

} // namespace

PdfPage::PdfPage(fz_document *document, int index, std::mutex &parseMutex)
    : m_document(document)
    , m_parseMutex(&parseMutex)
    , m_index(index)
{
    const std::lock_guard<std::mutex> guard(parseMutex);
    fz_context *ctx = MuContext::forThread();

    fz_page *page = nullptr;
    fz_rect bounds = fz_empty_rect;
    // fz_var keeps these readable after a longjmp out of the fz_try block.
    fz_var(page);
    fz_var(bounds);
    fz_try(ctx) {
        page = fz_load_page(ctx, document, index);
        bounds = fz_bound_page(ctx, page);
    }
    fz_catch(ctx)
        MuContext::raise(ctx, QStringLiteral("Could not load page %1").arg(index + 1));

    m_page = page;
    m_bounds = bounds;
    m_sizePoints = sizeOf(bounds);
}

PdfPage::~PdfPage()
{
    const std::lock_guard<std::mutex> guard(*m_parseMutex);
    fz_context *ctx = MuContext::forThread();
    if (m_displayList) {
        fz_drop_display_list(ctx, m_displayList);
        m_displayList = nullptr;
    }
    if (m_page) {
        fz_drop_page(ctx, m_page);
        m_page = nullptr;
    }
}

QSizeF PdfPage::sizePoints(int rotation) const
{
    const int normalised = ((rotation % 360) + 360) % 360;
    if (normalised == 90 || normalised == 270)
        return QSizeF(m_sizePoints.height(), m_sizePoints.width());
    return m_sizePoints;
}

fz_display_list *PdfPage::displayList(fz_context *ctx) const
{
    const std::lock_guard<std::mutex> guard(*m_parseMutex);
    if (m_displayList)
        return m_displayList;

    fz_display_list *list = nullptr;
    fz_device *device = nullptr;
    fz_var(list);
    fz_var(device);
    fz_try(ctx) {
        list = fz_new_display_list(ctx, m_bounds);
        device = fz_new_list_device(ctx, list);
        fz_run_page(ctx, m_page, device, fz_identity, nullptr);
        fz_close_device(ctx, device);
    }
    fz_always(ctx)
        fz_drop_device(ctx, device);
    fz_catch(ctx) {
        fz_drop_display_list(ctx, list);
        MuContext::raise(ctx, QStringLiteral("Could not build the display list for page %1")
                                  .arg(m_index + 1));
    }

    m_displayList = list;
    return m_displayList;
}

QTransform PdfPage::pageToImage(const QSizeF &sizePoints, qreal scale, int rotation)
{
    const fz_matrix ctm = renderMatrix(scale, rotation);
    const fz_rect box = fz_transform_rect(
        fz_make_rect(0, 0, static_cast<float>(sizePoints.width()),
                     static_cast<float>(sizePoints.height())),
        ctm);
    const fz_irect pixels = fz_round_rect(box);
    return toQTransform(fz_concat(
        ctm, fz_translate(static_cast<float>(-pixels.x0), static_cast<float>(-pixels.y0))));
}

QImage PdfPage::render(qreal scale, int rotation) const
{
    if (scale <= 0.0)
        return {};

    fz_context *ctx = MuContext::forThread();
    fz_display_list *list = displayList(ctx);

    const fz_matrix ctm = renderMatrix(scale, rotation);
    const fz_irect bbox = fz_round_rect(fz_transform_rect(m_bounds, ctm));

    fz_pixmap *pixmap = nullptr;
    fz_device *device = nullptr;
    fz_var(pixmap);
    fz_var(device);
    QImage image;
    fz_try(ctx) {
        pixmap = fz_new_pixmap_with_bbox(ctx, fz_device_bgr(ctx), bbox, nullptr, 1);
        fz_clear_pixmap_with_value(ctx, pixmap, 0xff);
        device = fz_new_draw_device(ctx, fz_identity, pixmap);
        fz_run_display_list(ctx, list, device, ctm, fz_infinite_rect, nullptr);
        fz_close_device(ctx, device);
    }
    fz_always(ctx)
        fz_drop_device(ctx, device);
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, pixmap);
        MuContext::raise(ctx, QStringLiteral("Could not render page %1").arg(m_index + 1));
    }

    image = pixmapToImage(ctx, pixmap);
    fz_drop_pixmap(ctx, pixmap);
    return image;
}

QString PdfPage::text() const
{
    fz_context *ctx = MuContext::forThread();
    fz_display_list *list = displayList(ctx);

    fz_stext_options options{};
    fz_stext_page *stext = nullptr;
    fz_try(ctx)
        stext = fz_new_stext_page_from_display_list(ctx, list, &options);
    fz_catch(ctx)
        MuContext::raise(ctx, QStringLiteral("Could not extract text from page %1").arg(m_index + 1));

    QString result;
    for (fz_stext_block *block = stext->first_block; block; block = block->next) {
        if (block->type != FZ_STEXT_BLOCK_TEXT)
            continue;
        for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
            for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
                const char32_t codepoint = static_cast<char32_t>(ch->c);
                result += QString::fromUcs4(&codepoint, 1);
            }
            result += u'\n';
        }
        result += u'\n';
    }

    fz_drop_stext_page(ctx, stext);
    return result;
}

QList<QRectF> PdfPage::search(const QString &needle, int maxHits) const
{
    if (needle.isEmpty() || maxHits <= 0)
        return {};

    fz_context *ctx = MuContext::forThread();
    const QByteArray utf8 = needle.toUtf8();

    QList<fz_quad> quads(maxHits);
    int hits = 0;
    fz_var(hits);
    const std::lock_guard<std::mutex> guard(*m_parseMutex);
    fz_try(ctx)
        hits = fz_search_page(ctx, m_page, utf8.constData(), nullptr, quads.data(), maxHits);
    fz_catch(ctx) {
        qCWarning(lcPdf) << "Search failed on page" << m_index + 1 << ":"
                         << MuContext::lastError(ctx);
        return {};
    }

    QList<QRectF> boxes;
    boxes.reserve(hits);
    for (int i = 0; i < hits; ++i)
        boxes.append(toQRectF(quads.at(i)));
    return boxes;
}

} // namespace sweet::core
