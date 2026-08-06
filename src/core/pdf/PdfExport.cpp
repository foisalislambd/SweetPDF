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

#if defined(SWEETPDF_HAS_OCR)
#  include <tesseract/baseapi.h>
#  include <tesseract/renderer.h>
#endif

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

QStringList tessdataSearchPaths()
{
    QStringList paths;
    const QByteArray fromEnv = qgetenv("TESSDATA_PREFIX");
    if (!fromEnv.isEmpty()) {
        const QString root = QFile::decodeName(fromEnv);
        // TESSDATA_PREFIX is either the tessdata folder itself or its parent.
        paths << root;
        paths << QDir(root).filePath(QStringLiteral("tessdata"));
    }
    paths << QStringLiteral("/usr/share/tesseract-ocr/5/tessdata")
          << QStringLiteral("/usr/share/tesseract-ocr/4.00/tessdata")
          << QStringLiteral("/usr/share/tesseract-ocr/tessdata")
          << QStringLiteral("/usr/share/tessdata")
          << QStringLiteral("/opt/homebrew/share/tessdata")
          << QStringLiteral("/usr/local/share/tessdata");
#ifdef Q_OS_WIN
    paths << QStringLiteral("C:/Program Files/Tesseract-OCR/tessdata")
          << QStringLiteral("C:/Program Files (x86)/Tesseract-OCR/tessdata");
#endif
    return paths;
}

} // namespace

QString defaultTessdataDir()
{
    for (const QString &path : tessdataSearchPaths()) {
        const QDir directory(path);
        if (!directory.exists())
            continue;
        if (!directory.entryList({ QStringLiteral("*.traineddata") }, QDir::Files).isEmpty())
            return path;
    }
    return {};
}

QStringList installedOcrLanguages(const QString &tessdataDir)
{
    const QString root = tessdataDir.isEmpty() ? defaultTessdataDir() : tessdataDir;
    if (root.isEmpty())
        return {};

    QStringList found;
    const QDir directory(root);
    const QStringList files =
        directory.entryList({ QStringLiteral("*.traineddata") }, QDir::Files, QDir::Name);
    for (const QString &file : files) {
        const QString code = QFileInfo(file).completeBaseName();
        if (code != QLatin1String("osd") && !found.contains(code))
            found.append(code);
    }
    return found;
}

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

#if defined(SWEETPDF_HAS_OCR)

QImage pageBitmap(const PdfDocument &document, int pageIndex, int dpi)
{
    auto page = document.page(pageIndex);
    if (!page)
        throw MuError(QStringLiteral("Could not load page %1").arg(pageIndex + 1));

    const qreal scale = dpi / 72.0;
    const QImage image = page->render(scale);
    if (image.isNull())
        throw MuError(QStringLiteral("Could not render page %1 for OCR").arg(pageIndex + 1));
    return image;
}

#endif

} // namespace

void toSearchablePdf(const PdfDocument &document, const QString &path, const QString &language,
                     const QString &tessdataDir, int dpi, const ProgressCallback &progress)
{
#if !defined(SWEETPDF_HAS_OCR)
    Q_UNUSED(document);
    Q_UNUSED(path);
    Q_UNUSED(language);
    Q_UNUSED(tessdataDir);
    Q_UNUSED(dpi);
    Q_UNUSED(progress);
    throw MuError(QStringLiteral(
        "This build of SweetPDF was compiled without Tesseract. Install Tesseract and rebuild "
        "with SWEETPDF_ENABLE_OCR=ON."));
#else
    const int pages = document.pageCount();
    if (pages <= 0)
        throw MuError(QStringLiteral("The document has no pages"));

    const QString dataDir = tessdataDir.isEmpty() ? defaultTessdataDir() : tessdataDir;
    if (dataDir.isEmpty()) {
        throw MuError(QStringLiteral(
            "No Tesseract language data was found. Install a language package such as "
            "tesseract-ocr-eng."));
    }

    const QString lang = language.isEmpty() ? QStringLiteral("eng") : language;
    if (!QFileInfo::exists(QDir(dataDir).filePath(lang + QStringLiteral(".traineddata")))) {
        throw MuError(QStringLiteral("Language data for “%1” is not installed in %2.")
                          .arg(lang, dataDir));
    }

    // TessPDFRenderer writes `<outputbase>.pdf`, so strip a trailing .pdf from
    // the path the user picked.
    QString outputBase = path;
    if (outputBase.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive))
        outputBase.chop(4);

    tesseract::TessBaseAPI api;
    const QByteArray dataDirBytes = QFile::encodeName(dataDir);
    const QByteArray langBytes = lang.toUtf8();
    if (api.Init(dataDirBytes.constData(), langBytes.constData()) != 0) {
        throw MuError(QStringLiteral("Could not start Tesseract with language “%1”.").arg(lang));
    }

    const QByteArray outputBytes = QFile::encodeName(outputBase);
    tesseract::TessPDFRenderer renderer(outputBytes.constData(), api.GetDatapath(), false);
    if (!renderer.BeginDocument("SweetPDF OCR")) {
        api.End();
        throw MuError(QStringLiteral("Could not create the searchable PDF"));
    }

    bool cancelled = false;
    QString failure;
    try {
        for (int index = 0; index < pages; ++index) {
            if (!keepGoing(progress, index, pages)) {
                cancelled = true;
                break;
            }

            // RGB888 is what SetImage expects; a deep copy keeps the buffer alive
            // for the whole Recognize call even if Qt recycles the original.
            QImage image = pageBitmap(document, index, dpi).convertToFormat(QImage::Format_RGB888);
            image = image.copy();
            api.SetImage(image.constBits(), image.width(), image.height(), 3, image.bytesPerLine());
            api.SetSourceResolution(dpi);
            if (api.Recognize(nullptr) != 0) {
                failure = QStringLiteral("OCR failed on page %1").arg(index + 1);
                break;
            }
            if (!renderer.AddImage(&api)) {
                failure = QStringLiteral("Could not write OCR page %1").arg(index + 1);
                break;
            }
        }
    } catch (const std::exception &error) {
        failure = QString::fromUtf8(error.what());
    }

    const bool finished = failure.isEmpty() && !cancelled && renderer.EndDocument();
    api.End();

    const QString written = outputBase + QStringLiteral(".pdf");
    if (cancelled || !failure.isEmpty() || !finished) {
        QFile::remove(written);
        if (cancelled)
            return;
        throw MuError(failure.isEmpty() ? QStringLiteral("Could not finish the searchable PDF")
                                        : failure);
    }

    // Rename into the exact path the dialog asked for when it differed only by
    // case of the extension, or when TessPDFRenderer already used that name.
    if (QFileInfo(written).absoluteFilePath() != QFileInfo(path).absoluteFilePath()
        && QFile::exists(written)) {
        QFile::remove(path);
        if (!QFile::rename(written, path))
            throw MuError(QStringLiteral("Could not write %1").arg(QFileInfo(path).fileName()));
    }
#endif
}

} // namespace sweet::core::exporter
