#include "core/pdf/OfficeExchange.h"

#include "core/pdf/MuContext.h"
#include "core/pdf/PdfAnnotations.h"
#include "core/pdf/PdfDocument.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QPair>
#include <QXmlStreamReader>

#include <algorithm>

#include <mupdf/fitz.h>

namespace sweet::core {
namespace {

quint32 crc32(const QByteArray &data)
{
    quint32 crc = 0xffffffffu;
    for (unsigned char byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

void put16(QByteArray &out, quint16 value)
{
    out.append(char(value & 0xff));
    out.append(char((value >> 8) & 0xff));
}

void put32(QByteArray &out, quint32 value)
{
    out.append(char(value & 0xff));
    out.append(char((value >> 8) & 0xff));
    out.append(char((value >> 16) & 0xff));
    out.append(char((value >> 24) & 0xff));
}

bool inRange(const QByteArray &bytes, int offset, int count)
{
    return offset >= 0 && count >= 0 && offset <= bytes.size() - count;
}

quint16 read16(const QByteArray &bytes, int offset)
{
    if (!inRange(bytes, offset, 2))
        return 0;
    const auto u = reinterpret_cast<const unsigned char *>(bytes.constData());
    return quint16(u[offset]) | quint16(u[offset + 1] << 8);
}

quint32 read32(const QByteArray &bytes, int offset)
{
    if (!inRange(bytes, offset, 4))
        return 0;
    const auto u = reinterpret_cast<const unsigned char *>(bytes.constData());
    return quint32(u[offset]) | (quint32(u[offset + 1]) << 8) | (quint32(u[offset + 2]) << 16)
        | (quint32(u[offset + 3]) << 24);
}

QByteArray storedZip(const QList<QPair<QString, QByteArray>> &files)
{
    QByteArray archive;
    struct Placed {
        QByteArray name;
        quint32 crc = 0;
        quint32 size = 0;
        quint32 offset = 0;
    };
    QList<Placed> placed;
    for (const auto &file : files) {
        Placed entry;
        entry.name = file.first.toUtf8();
        entry.crc = crc32(file.second);
        entry.size = quint32(file.second.size());
        entry.offset = quint32(archive.size());
        archive.append("PK\x03\x04", 4);
        put16(archive, 20);
        put16(archive, 0);
        put16(archive, 0);
        put16(archive, 0);
        put16(archive, 0);
        put32(archive, entry.crc);
        put32(archive, entry.size);
        put32(archive, entry.size);
        put16(archive, quint16(entry.name.size()));
        put16(archive, 0);
        archive.append(entry.name);
        archive.append(file.second);
        placed.append(entry);
    }

    const quint32 directory = quint32(archive.size());
    for (const Placed &entry : placed) {
        archive.append("PK\x01\x02", 4);
        put16(archive, 20);
        put16(archive, 20);
        put16(archive, 0);
        put16(archive, 0);
        put16(archive, 0);
        put16(archive, 0);
        put32(archive, entry.crc);
        put32(archive, entry.size);
        put32(archive, entry.size);
        put16(archive, quint16(entry.name.size()));
        put16(archive, 0);
        put16(archive, 0);
        put16(archive, 0);
        put16(archive, 0);
        put32(archive, 0);
        put32(archive, entry.offset);
        archive.append(entry.name);
    }
    const quint32 directorySize = quint32(archive.size()) - directory;
    archive.append("PK\x05\x06", 4);
    put16(archive, 0);
    put16(archive, 0);
    put16(archive, quint16(placed.size()));
    put16(archive, quint16(placed.size()));
    put32(archive, directorySize);
    put32(archive, directory);
    put16(archive, 0);
    return archive;
}

QByteArray inflateRaw(const unsigned char *data, size_t length, size_t expected)
{
    fz_context *ctx = MuContext::forThread();
    fz_stream *memory = nullptr;
    fz_stream *inflated = nullptr;
    fz_buffer *buffer = nullptr;
    fz_var(memory);
    fz_var(inflated);
    fz_var(buffer);
    QByteArray out;
    fz_try(ctx) {
        memory = fz_open_memory(ctx, data, length);
        // fz_open_flated keeps the chain; this function still owns memory.
        inflated = fz_open_flated(ctx, memory, -15);
        buffer = fz_read_all(ctx, inflated, expected > 0 ? expected : 1024);
        unsigned char *storage = nullptr;
        const size_t count = fz_buffer_storage(ctx, buffer, &storage);
        out = QByteArray(reinterpret_cast<const char *>(storage), int(count));
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, buffer);
        fz_drop_stream(ctx, inflated);
        fz_drop_stream(ctx, memory);
    }
    fz_catch(ctx)
        out.clear();
    return out;
}

QHash<QString, QByteArray> unzip(const QByteArray &package)
{
    QHash<QString, QByteArray> files;
    if (package.size() < 22)
        return files;

    int eocd = -1;
    const int scanFrom = qMax(0, package.size() - (22 + 65535));
    for (int index = package.size() - 22; index >= scanFrom; --index) {
        if (package.at(index) == 'P' && package.at(index + 1) == 'K' && package.at(index + 2) == 5
            && package.at(index + 3) == 6) {
            eocd = index;
            break;
        }
    }
    if (eocd < 0)
        return files;

    const int count = read16(package, eocd + 10);
    int cursor = int(read32(package, eocd + 16));
    for (int index = 0; index < count && cursor + 46 <= package.size(); ++index) {
        if (read32(package, cursor) != 0x02014b50u)
            break;
        const quint16 method = read16(package, cursor + 10);
        const quint32 compressed = read32(package, cursor + 20);
        const quint32 uncompressed = read32(package, cursor + 24);
        const quint16 nameLength = read16(package, cursor + 28);
        const quint16 extraLength = read16(package, cursor + 30);
        const quint16 commentLength = read16(package, cursor + 32);
        const int local = int(read32(package, cursor + 42));
        if (!inRange(package, cursor + 46, nameLength))
            break;
        const QString name = QString::fromUtf8(package.mid(cursor + 46, nameLength));
        const int next = cursor + 46 + nameLength + extraLength + commentLength;
        if (next < cursor)
            break;
        cursor = next;
        if (!inRange(package, local, 30))
            continue;
        const quint16 localName = read16(package, local + 26);
        const quint16 localExtra = read16(package, local + 28);
        const int dataAt = local + 30 + localName + localExtra;
        if (dataAt < 0 || quint32(compressed) > quint32(package.size())
            || dataAt > package.size() - int(compressed))
            continue;
        const QByteArray payload = package.mid(dataAt, int(compressed));
        if (method == 0)
            files.insert(name, payload);
        else if (method == 8)
            files.insert(name, inflateRaw(reinterpret_cast<const unsigned char *>(payload.constData()),
                                          size_t(payload.size()), uncompressed));
    }
    return files;
}

QString xmlEscape(const QString &text)
{
    return text.toHtmlEscaped();
}

QString contentTypes(const QStringList &overrides)
{
    QString xml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>");
    xml += overrides.join(QString());
    xml += QStringLiteral("</Types>");
    return xml;
}

QByteArray rels(const QString &target, const QString &type)
{
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
               "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
               "<Relationship Id=\"rId1\" Type=\"%1\" Target=\"%2\"/>"
               "</Relationships>")
        .arg(type, target)
        .toUtf8();
}

QStringList wrapLines(const QString &paragraph, int width)
{
    QStringList lines;
    QString remaining = paragraph;
    while (remaining.size() > width) {
        int split = remaining.lastIndexOf(QLatin1Char(' '), width);
        if (split <= 0)
            split = width;
        lines.append(remaining.left(split).trimmed());
        remaining = remaining.mid(split).trimmed();
    }
    if (!remaining.isEmpty() || lines.isEmpty())
        lines.append(remaining);
    return lines;
}

QString columnName(int index)
{
    QString name;
    int value = index + 1;
    while (value > 0) {
        const int digit = (value - 1) % 26;
        name.prepend(QChar(QLatin1Char('A') + digit));
        value = (value - 1) / 26;
    }
    return name;
}

} // namespace

namespace office {

QByteArray toDocx(const QString &plainText)
{
    QString body;
    const QStringList paragraphs = plainText.split(QLatin1Char('\n'));
    for (const QString &paragraph : paragraphs) {
        body += QStringLiteral("<w:p><w:r><w:t xml:space=\"preserve\">%1</w:t></w:r></w:p>")
                    .arg(xmlEscape(paragraph));
    }
    const QString document =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                       "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
                       "<w:body>%1<w:sectPr><w:pgSz w:w=\"12240\" w:h=\"15840\"/></w:sectPr></w:body>"
                       "</w:document>")
            .arg(body);
    const QString overrides = QStringLiteral(
        "<Override PartName=\"/word/document.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>");
    return storedZip({
        { QStringLiteral("[Content_Types].xml"), contentTypes({ overrides }).toUtf8() },
        { QStringLiteral("_rels/.rels"),
          rels(QStringLiteral("word/document.xml"),
               QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
                              "officeDocument")) },
        { QStringLiteral("word/document.xml"), document.toUtf8() },
    });
}

QByteArray toXlsx(const QList<QStringList> &rows)
{
    QString sheetData;
    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        sheetData += QStringLiteral("<row r=\"%1\">").arg(rowIndex + 1);
        const QStringList cells = rows.at(rowIndex);
        for (int column = 0; column < cells.size(); ++column) {
            const QString ref = columnName(column) + QString::number(rowIndex + 1);
            sheetData += QStringLiteral("<c r=\"%1\" t=\"inlineStr\"><is><t>%2</t></is></c>")
                             .arg(ref, xmlEscape(cells.at(column)));
        }
        sheetData += QStringLiteral("</row>");
    }
    const QString sheet = QStringLiteral(
                              "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                              "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                              "<sheetData>%1</sheetData></worksheet>")
                              .arg(sheetData);
    const QString workbook = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets><sheet name=\"Sheet1\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>");
    const QString overrides = QStringLiteral(
        "<Override PartName=\"/xl/workbook.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>");
    return storedZip({
        { QStringLiteral("[Content_Types].xml"), contentTypes({ overrides }).toUtf8() },
        { QStringLiteral("_rels/.rels"),
          rels(QStringLiteral("xl/workbook.xml"),
               QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
                              "officeDocument")) },
        { QStringLiteral("xl/workbook.xml"), workbook.toUtf8() },
        { QStringLiteral("xl/_rels/workbook.xml.rels"),
          rels(QStringLiteral("worksheets/sheet1.xml"),
               QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
                              "worksheet")) },
        { QStringLiteral("xl/worksheets/sheet1.xml"), sheet.toUtf8() },
    });
}

QByteArray toPptx(const QStringList &slideText)
{
    QStringList slides = slideText;
    if (slides.isEmpty())
        slides.append(QString());

    QStringList overrides;
    QString presentationRels;
    QString slideIds;
    QList<QPair<QString, QByteArray>> files;
    for (int index = 0; index < slides.size(); ++index) {
        const int number = index + 1;
        const QString part = QStringLiteral("/ppt/slides/slide%1.xml").arg(number);
        overrides.append(
            QStringLiteral("<Override PartName=\"%1\" "
                           "ContentType=\"application/vnd.openxmlformats-officedocument."
                           "presentationml.slide+xml\"/>")
                .arg(part));
        QString paragraphs;
        const QStringList lines = slides.at(index).split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            paragraphs += QStringLiteral("<a:p><a:r><a:rPr lang=\"en-US\" sz=\"2400\"/>"
                                         "<a:t>%1</a:t></a:r></a:p>")
                              .arg(xmlEscape(line));
        }
        const QString slide = QStringLiteral(
                                  "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                                  "<p:sld xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
                                  "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">"
                                  "<p:cSld><p:spTree>"
                                  "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>"
                                  "<p:grpSpPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/>"
                                  "<a:chOff x=\"0\" y=\"0\"/><a:chExt cx=\"0\" cy=\"0\"/></a:xfrm></p:grpSpPr>"
                                  "<p:sp><p:nvSpPr><p:cNvPr id=\"2\" name=\"Text\"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr>"
                                  "<p:spPr><a:xfrm><a:off x=\"457200\" y=\"457200\"/>"
                                  "<a:ext cx=\"8229600\" cy=\"5943600\"/></a:xfrm>"
                                  "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></p:spPr>"
                                  "<p:txBody><a:bodyPr/><a:lstStyle/>%1</p:txBody></p:sp>"
                                  "</p:spTree></p:cSld></p:sld>")
                                  .arg(paragraphs);
        files.append({ QStringLiteral("ppt/slides/slide%1.xml").arg(number), slide.toUtf8() });
        presentationRels += QStringLiteral(
                                "<Relationship Id=\"rId%1\" "
                                "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
                                "relationships/slide\" Target=\"slides/slide%2.xml\"/>")
                                .arg(number)
                                .arg(number);
        slideIds += QStringLiteral("<p:sldId id=\"%1\" r:id=\"rId%2\"/>").arg(256 + number).arg(number);
    }
    overrides.prepend(QStringLiteral(
        "<Override PartName=\"/ppt/presentation.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml\"/>"));
    const QString presentation = QStringLiteral(
                                     "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                                     "<p:presentation xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
                                     "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">"
                                     "<p:sldIdLst>%1</p:sldIdLst>"
                                     "<p:sldSz cx=\"9144000\" cy=\"6858000\"/>"
                                     "<p:notesSz cx=\"6858000\" cy=\"9144000\"/>"
                                     "</p:presentation>")
                                     .arg(slideIds);
    const QByteArray presentationRelationships = QStringLiteral(
                                                     "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                                                     "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                                                     "%1</Relationships>")
                                                     .arg(presentationRels)
                                                     .toUtf8();
    files.prepend({ QStringLiteral("ppt/_rels/presentation.xml.rels"), presentationRelationships });
    files.prepend({ QStringLiteral("ppt/presentation.xml"), presentation.toUtf8() });
    files.prepend({ QStringLiteral("_rels/.rels"),
                    rels(QStringLiteral("ppt/presentation.xml"),
                         QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/"
                                        "relationships/officeDocument")) });
    files.prepend({ QStringLiteral("[Content_Types].xml"), contentTypes(overrides).toUtf8() });
    return storedZip(files);
}

QStringList paragraphsFromDocx(const QByteArray &package)
{
    const QByteArray xml = unzip(package).value(QStringLiteral("word/document.xml"));
    QStringList paragraphs;
    QXmlStreamReader reader(xml);
    QString current;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() && !reader.isEndElement())
            continue;
        if (reader.name() == QLatin1String("p") && reader.isEndElement()) {
            paragraphs.append(current);
            current.clear();
        } else if (reader.name() == QLatin1String("t") && reader.isStartElement()) {
            current += reader.readElementText(QXmlStreamReader::IncludeChildElements);
        }
    }
    if (!current.isEmpty())
        paragraphs.append(current);
    return paragraphs;
}

QList<QStringList> rowsFromXlsx(const QByteArray &package)
{
    const QHash<QString, QByteArray> files = unzip(package);
    QStringList shared;
    QXmlStreamReader strings(files.value(QStringLiteral("xl/sharedStrings.xml")));
    QString current;
    bool inItem = false;
    while (!strings.atEnd()) {
        strings.readNext();
        if (strings.isStartElement() && strings.name() == QLatin1String("si")) {
            inItem = true;
            current.clear();
        } else if (strings.isEndElement() && strings.name() == QLatin1String("si")) {
            shared.append(current);
            inItem = false;
        } else if (inItem && strings.isStartElement() && strings.name() == QLatin1String("t")) {
            current += strings.readElementText(QXmlStreamReader::IncludeChildElements);
        }
    }

    QList<QStringList> rows;
    QXmlStreamReader sheet(files.value(QStringLiteral("xl/worksheets/sheet1.xml")));
    QStringList row;
    QString cellType;
    int column = 0;
    const auto place = [&row](int at, const QString &value) {
        while (row.size() < at)
            row.append(QString());
        if (row.size() == at)
            row.append(value);
        else if (at >= 0 && at < row.size())
            row[at] = value;
    };
    while (!sheet.atEnd()) {
        sheet.readNext();
        if (sheet.isStartElement() && sheet.name() == QLatin1String("row")) {
            row.clear();
            column = 0;
        } else if (sheet.isEndElement() && sheet.name() == QLatin1String("row")) {
            rows.append(row);
        } else if (sheet.isStartElement() && sheet.name() == QLatin1String("c")) {
            cellType = sheet.attributes().value(QLatin1String("t")).toString();
            const QString ref = sheet.attributes().value(QLatin1String("r")).toString();
            int decoded = 0;
            bool sawLetter = false;
            for (const QChar character : ref) {
                if (!character.isLetter())
                    break;
                sawLetter = true;
                decoded = decoded * 26 + (character.toUpper().unicode() - u'A' + 1);
            }
            column = sawLetter ? decoded - 1 : row.size();
        } else if (sheet.isStartElement() && sheet.name() == QLatin1String("v")) {
            const QString value = sheet.readElementText();
            if (cellType == QLatin1String("s")) {
                const int index = value.toInt();
                place(column, index >= 0 && index < shared.size() ? shared.at(index) : value);
            } else {
                place(column, value);
            }
        } else if (sheet.isStartElement() && sheet.name() == QLatin1String("t")
                   && cellType == QLatin1String("inlineStr")) {
            place(column, sheet.readElementText(QXmlStreamReader::IncludeChildElements));
        }
    }
    return rows;
}

QStringList slidesFromPptx(const QByteArray &package)
{
    const QHash<QString, QByteArray> files = unzip(package);
    QStringList names;
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        if (it.key().startsWith(QLatin1String("ppt/slides/slide")) && it.key().endsWith(QLatin1String(".xml")))
            names.append(it.key());
    }
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        const auto number = [](const QString &path) {
            const int slash = path.lastIndexOf(QLatin1Char('/'));
            const QString file = path.mid(slash + 1);
            int value = 0;
            for (const QChar character : file) {
                if (character.isDigit())
                    value = value * 10 + character.digitValue();
            }
            return value;
        };
        return number(a) < number(b);
    });
    QStringList slides;
    for (const QString &name : names) {
        QXmlStreamReader reader(files.value(name));
        QStringList lines;
        QString line;
        while (!reader.atEnd()) {
            reader.readNext();
            if (reader.isStartElement() && reader.name() == QLatin1String("t"))
                line += reader.readElementText(QXmlStreamReader::IncludeChildElements);
            else if (reader.isEndElement() && reader.name() == QLatin1String("p")) {
                if (!line.isEmpty())
                    lines.append(line);
                line.clear();
            }
        }
        slides.append(lines.join(QLatin1Char('\n')));
    }
    return slides;
}

std::shared_ptr<PdfDocument> pdfFromParagraphs(const QStringList &paragraphs)
{
    const QSizeF size(595.28, 841.89);
    auto document = PdfDocument::createEmpty(size);
    QStringList lines;
    for (const QString &paragraph : paragraphs) {
        const QStringList broken = paragraph.split(QLatin1Char('\n'));
        for (const QString &piece : broken) {
            if (piece.isEmpty())
                lines.append(QString());
            else
                lines.append(wrapLines(piece, 90));
        }
    }
    if (lines.isEmpty())
        return document;

    constexpr qreal font = 11.0;
    constexpr qreal step = 16.0;
    qreal y = 54.0;
    int page = 0;
    for (const QString &line : lines) {
        if (y > size.height() - 54.0) {
            ++page;
            document->insertBlankPage(page, size);
            y = 54.0;
        }
        if (!line.isEmpty()) {
            AnnotationSpec spec;
            spec.type = AnnotationType::FreeText;
            spec.pageIndex = page;
            spec.rect = QRectF(54, y, size.width() - 108, font + 6);
            spec.contents = line;
            spec.fontSize = font;
            spec.color = QColor(Qt::black);
            spec.borderWidth = 0;
            spec.opacity = 1;
            annotations::add(*document, spec);
        }
        y += step;
    }
    return document;
}

} // namespace office

namespace batch {

QList<Item> run(const QStringList &files, const QString &directory, Job job,
                const std::function<bool(int, int)> &progress)
{
    QList<Item> results;
    QDir().mkpath(directory);
    for (int index = 0; index < files.size(); ++index) {
        if (progress && !progress(index, files.size()))
            break;
        Item item;
        item.input = files.at(index);
        const QFileInfo info(item.input);
        QString output = QDir(directory).filePath(info.fileName());
        if (QFileInfo(output).absoluteFilePath() == info.absoluteFilePath())
            output = QDir(directory).filePath(info.completeBaseName() + QStringLiteral("-edited.pdf"));
        item.output = output;
        try {
            auto document = PdfDocument::open(item.input);
            if (job == Job::Grayscale)
                document->convertToGrayscale();
            else if (job == Job::Strip)
                document->stripHiddenData();
            SaveOptions options;
            options.incremental = false;
            options.compressStreams = true;
            options.garbageCollect = true;
            options.cleanContentStreams = job == Job::Compress;
            document->saveToFile(output, options);
            item.ok = true;
        } catch (const std::exception &failure) {
            item.error = QString::fromUtf8(failure.what());
        }
        results.append(item);
    }
    if (progress)
        progress(files.size(), files.size());
    return results;
}

} // namespace batch
} // namespace sweet::core
