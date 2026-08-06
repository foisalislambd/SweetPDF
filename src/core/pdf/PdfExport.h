#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace sweet::core {

class PdfDocument;

/// Turning a document into something that is not a PDF, and back.
///
/// These run synchronously and can take a while on a long document, so callers
/// on the GUI thread should report progress through the `ProgressCallback`, which
/// returns false to cancel.
namespace exporter {

using ProgressCallback = std::function<bool(int pageIndex, int pageCount)>;

/// Renders each page in `indices` to `directory` as `baseName-001.png` (or .jpg).
/// `dpi` is the render resolution. Returns the files written.
QStringList pagesToImages(const PdfDocument &document, const QList<int> &indices,
                          const QString &directory, const QString &baseName,
                          const QString &format, int dpi,
                          const ProgressCallback &progress = {});

/// Plain text of the whole document, pages separated by form feeds.
QString documentText(const PdfDocument &document, const ProgressCallback &progress = {});

/// Writes `documentText` to a UTF-8 file.
void textToFile(const PdfDocument &document, const QString &path,
                const ProgressCallback &progress = {});

/// A self-contained HTML file with each page as a positioned block of text.
void toHtml(const PdfDocument &document, const QString &path,
            const ProgressCallback &progress = {});

/// Runs OCR over every page and writes a new PDF that keeps the page image and
/// adds an invisible text layer, so the result is searchable and selectable.
///
/// `language` is a Tesseract language code such as "eng"; `tessdataDir` may be
/// empty to use the system location. Throws MuError when the MuPDF build has no
/// OCR support or the language data is missing.
void toSearchablePdf(const PdfDocument &document, const QString &path, const QString &language,
                     const QString &tessdataDir, int dpi,
                     const ProgressCallback &progress = {});

} // namespace exporter
} // namespace sweet::core
