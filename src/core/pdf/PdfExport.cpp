#include "core/pdf/PdfExport.h"

#include "core/pdf/MuContext.h"
#include "core/pdf/PdfDocument.h"
#include "core/util/Logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QTextStream>

#include <mupdf/pdf.h>

namespace sweet::core::exporter {
namespace {

/// Calls `progress` and reports whether the caller wants to keep going.
bool keepGoing(const ProgressCallback &progress, int pageIndex, int pageCount)
{
    return !progress || progress(pageIndex, pageCount);
}

QString escapeHtml(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    escaped.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    escaped.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    return escaped;
}

void writeAll(const QString &path, const QString &contents)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        throw MuError(QStringLiteral("Could not write %1").arg(QFileInfo(path).fileName()));

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << contents;
    stream.flush();

    if (!file.commit())
        throw MuError(QStringLiteral("Could not write %1").arg(QFileInfo(path).fileName()));
}

} // namespace

QStringList pagesToImages(const PdfDocument &document, const QList<int> &indices,
                          const QString &directory, const QString &baseName,
                          const QString &format, int dpi, const ProgressCallback &progress)
{
    QStringList written;
    const QDir target(directory);
    if (!target.exists())
        throw MuError(QStringLiteral("The folder %1 does not exist").arg(directory));

    const qreal scale = qBound(0.1, dpi / 72.0, 12.0);
    const bool jpeg = format.compare(QLatin1String("jpg"), Qt::CaseInsensitive) == 0
        || format.compare(QLatin1String("jpeg"), Qt::CaseInsensitive) == 0;
    const int quality = jpeg ? 92 : -1;

    int done = 0;
    for (const int index : indices) {
        if (!keepGoing(progress, done, int(indices.size())))
            break;

        auto page = document.page(index);
        if (!page)
            continue;

        const QImage image = page->render(scale);
        if (image.isNull())
            continue;

        const QString name = QStringLiteral("%1-%2.%3")
                                 .arg(baseName)
                                 .arg(index + 1, 3, 10, QLatin1Char('0'))
                                 .arg(jpeg ? QStringLiteral("jpg") : format.toLower());
        const QString path = target.filePath(name);
        if (!image.save(path, nullptr, quality))
            throw MuError(QStringLiteral("Could not write %1").arg(name));

        written.append(path);
        ++done;
    }

    return written;
}

QString documentText(const PdfDocument &document, const ProgressCallback &progress)
{
    QString collected;
    const int pages = document.pageCount();
    for (int index = 0; index < pages; ++index) {
        if (!keepGoing(progress, index, pages))
            break;
        if (auto page = document.page(index))
            collected += page->text();
        if (index + 1 < pages)
            collected += QChar(0x000C); // Form feed: the usual page separator.
    }
    return collected;
}

void textToFile(const PdfDocument &document, const QString &path,
                const ProgressCallback &progress)
{
    writeAll(path, documentText(document, progress));
}

void toHtml(const PdfDocument &document, const QString &path, const ProgressCallback &progress)
{
    const QString title = document.info().title.isEmpty()
        ? QFileInfo(document.filePath()).completeBaseName()
        : document.info().title;

    QString html = QStringLiteral(
                       "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n"
                       "<title>%1</title>\n<style>\n"
                       "body{margin:0;background:#e8ebf1;font:14px/1.5 system-ui,sans-serif;}\n"
                       ".page{background:#fff;max-width:52em;margin:24px auto;padding:48px;\n"
                       "  box-shadow:0 2px 12px rgba(11,20,43,.14);border-radius:3px;\n"
                       "  white-space:pre-wrap;}\n"
                       ".page-number{color:#6a7486;font-size:12px;text-align:right;}\n"
                       "</style></head><body>\n")
                       .arg(escapeHtml(title));

    const int pages = document.pageCount();
    for (int index = 0; index < pages; ++index) {
        if (!keepGoing(progress, index, pages))
            break;

        QString text;
        if (auto page = document.page(index))
            text = page->text();

        html += QStringLiteral("<section class=\"page\"><div class=\"page-number\">%1</div>%2"
                               "</section>\n")
                    .arg(index + 1)
                    .arg(escapeHtml(text));
    }

    html += QStringLiteral("</body></html>\n");
    writeAll(path, html);
}

namespace {

/// Rasterise one page into an open writer. This lives in its own function so the
/// caller's loop counters never share a stack frame with MuPDF's setjmp.
void writeOcrPage(fz_context *ctx, fz_document_writer *writer, PdfPage &page, int index)
{
    const QSizeF size = page.sizePoints();
    fz_device *device = nullptr;
    fz_var(device);
    fz_try(ctx) {
        const fz_rect box = fz_make_rect(0, 0, float(size.width()), float(size.height()));
        device = fz_begin_page(ctx, writer, box);
        fz_run_page(ctx, page.handle(), device, fz_identity, nullptr);
        fz_end_page(ctx, writer);
    }
    fz_catch(ctx) {
        fz_drop_document_writer(ctx, writer);
        MuContext::raise(ctx, QStringLiteral("OCR failed on page %1").arg(index + 1));
    }
}

/// Flush and release the writer. A cancelled run is dropped without closing so
/// MuPDF does not bother finalising a file we are about to delete.
void finishOcrWriter(fz_context *ctx, fz_document_writer *writer, bool cancelled)
{
    fz_try(ctx) {
        if (!cancelled)
            fz_close_document_writer(ctx, writer);
    }
    fz_always(ctx)
        fz_drop_document_writer(ctx, writer);
    fz_catch(ctx)
        MuContext::raise(ctx, QStringLiteral("Could not finish the OCR document"));
}

} // namespace

void toSearchablePdf(const PdfDocument &document, const QString &path, const QString &language,
                     const QString &tessdataDir, int dpi, const ProgressCallback &progress)
{
    fz_context *ctx = MuContext::forThread();
    const int pages = document.pageCount();
    if (pages <= 0)
        throw MuError(QStringLiteral("The document has no pages"));

    // MuPDF's PDF/OCR writer rasterises each page, runs Tesseract over it and
    // stores the recognised words as invisible text on top of the image. That is
    // exactly what a searchable scan is, so there is no need to build the text
    // layer by hand.
    QStringList options;
    options << QStringLiteral("compression=flate")
            << QStringLiteral("resolution=%1").arg(qBound(72, dpi, 600));
    if (!language.isEmpty())
        options << QStringLiteral("ocr-language=%1").arg(language);
    if (!tessdataDir.isEmpty())
        options << QStringLiteral("ocr-datadir=%1").arg(tessdataDir);

    const QByteArray nativePath = QFile::encodeName(path);
    const QByteArray optionString = options.join(QLatin1Char(',')).toUtf8();

    fz_document_writer *writer = nullptr;
    fz_var(writer);
    fz_try(ctx)
        writer = fz_new_pdfocr_writer(ctx, nativePath.constData(), optionString.constData());
    fz_catch(ctx)
        MuContext::raise(ctx, QStringLiteral("This build of MuPDF cannot run OCR"));

    bool cancelled = false;
    for (int index = 0; index < pages; ++index) {
        if (!keepGoing(progress, index, pages)) {
            cancelled = true;
            break;
        }

        if (auto page = document.page(index)) {
            const std::lock_guard<std::mutex> guard(document.parseMutex());
            writeOcrPage(ctx, writer, *page, index);
        }
    }

    finishOcrWriter(ctx, writer, cancelled);

    if (cancelled)
        QFile::remove(path);
}

} // namespace sweet::core::exporter
