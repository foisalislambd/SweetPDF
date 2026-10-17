#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace sweet::core {

class PdfDocument;

/// Office files SweetPDF can build or read without an external converter.
/// Word, Excel and PowerPoint packages are ZIP archives of XML. Export writes
/// the text; import reads the text back into a PDF. Layout, styles and pictures
/// from the office file are not kept.
namespace office {

QByteArray toDocx(const QString &plainText);
QByteArray toXlsx(const QList<QStringList> &rows);
QByteArray toPptx(const QStringList &slideText);

QStringList paragraphsFromDocx(const QByteArray &package);
QList<QStringList> rowsFromXlsx(const QByteArray &package);
QStringList slidesFromPptx(const QByteArray &package);

/// One FreeText line per paragraph, paginated onto A4.
std::shared_ptr<PdfDocument> pdfFromParagraphs(const QStringList &paragraphs);

} // namespace office

/// The same operation applied to many PDFs, written into one folder.
namespace batch {

enum class Job { Compress, Grayscale, Strip };

struct Item {
    QString input;
    QString output;
    QString error;
    bool ok = false;
};

QList<Item> run(const QStringList &files, const QString &directory, Job job,
                const std::function<bool(int done, int total)> &progress = {});

} // namespace batch
} // namespace sweet::core
