#include "core/pdf/PdfDocument.h"

#include "core/pdf/MuContext.h"
#include "core/util/Logging.h"

#include <QDateTime>

#include <cstring>

namespace sweet::core {
namespace {

/// Walks a PDF name tree, appending embedded file-spec objects. The pointers
/// are borrowed from the document and only valid while the parse lock is held.
void collectEmbedded(fz_context *ctx, pdf_obj *node, QList<pdf_obj *> &out)
{
    if (!node)
        return;

    pdf_obj *names = pdf_dict_get(ctx, node, PDF_NAME(Names));
    if (pdf_is_array(ctx, names)) {
        const int count = pdf_array_len(ctx, names);
        for (int index = 0; index + 1 < count; index += 2) {
            pdf_obj *spec = pdf_array_get(ctx, names, index + 1);
            if (pdf_is_embedded_file(ctx, spec))
                out.append(spec);
        }
    }

    pdf_obj *kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
    if (pdf_is_array(ctx, kids)) {
        const int count = pdf_array_len(ctx, kids);
        for (int index = 0; index < count; ++index)
            collectEmbedded(ctx, pdf_array_get(ctx, kids, index), out);
    }
}

pdf_obj *embeddedFilesRoot(fz_context *ctx, pdf_document *pdf)
{
    pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, pdf), PDF_NAME(Root));
    pdf_obj *names = pdf_dict_get(ctx, root, PDF_NAME(Names));
    return pdf_dict_get(ctx, names, PDF_NAME(EmbeddedFiles));
}

EmbeddedFile describeFile(fz_context *ctx, pdf_obj *spec)
{
    EmbeddedFile file;
    pdf_filespec_params params{};
    pdf_get_filespec_params(ctx, spec, &params);
    if (params.filename)
        file.name = QString::fromUtf8(params.filename);
    if (params.mimetype)
        file.mimeType = QString::fromUtf8(params.mimetype);
    if (params.size >= 0)
        file.size = params.size;
    return file;
}

EmbeddedFile describeNamed(fz_context *ctx, pdf_obj *spec, int index)
{
    EmbeddedFile file;
    fz_try(ctx)
        file = describeFile(ctx, spec);
    fz_catch(ctx)
        file = {};
    file.nameTreeIndex = index;
    return file;
}

void appendPageAttachments(fz_context *ctx, pdf_document *pdf, int pageIndex,
                           QList<EmbeddedFile> &files)
{
    pdf_page *page = nullptr;
    fz_var(page);
    fz_try(ctx) {
        page = pdf_load_page(ctx, pdf, pageIndex);
        for (pdf_annot *annot = pdf_first_annot(ctx, page); annot;
             annot = pdf_next_annot(ctx, annot)) {
            if (pdf_annot_type(ctx, annot) != PDF_ANNOT_FILE_ATTACHMENT)
                continue;
            if (!pdf_annot_has_filespec(ctx, annot))
                continue;
            pdf_obj *spec = pdf_annot_filespec(ctx, annot);
            if (!pdf_is_embedded_file(ctx, spec))
                continue;
            EmbeddedFile file = describeFile(ctx, spec);
            file.pageIndex = pageIndex;
            file.objectNumber = pdf_to_num(ctx, pdf_annot_obj(ctx, annot));
            files.append(std::move(file));
        }
    }
    fz_always(ctx)
        fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
    fz_catch(ctx)
        qCWarning(lcPdf) << "Could not list attachments on page" << pageIndex + 1 << ":"
                         << MuContext::lastError(ctx);
}

} // namespace

QList<EmbeddedFile> PdfDocument::embeddedFiles() const
{
    if (!m_pdf)
        return {};

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(m_parseMutex);

    QList<pdf_obj *> specs;
    fz_try(ctx)
        collectEmbedded(ctx, embeddedFilesRoot(ctx, m_pdf), specs);
    fz_catch(ctx)
    {
        qCWarning(lcPdf) << "Could not list embedded files:" << MuContext::lastError(ctx);
        specs.clear();
    }

    QList<EmbeddedFile> files;
    files.reserve(specs.size());
    for (int index = 0; index < specs.size(); ++index)
        files.append(describeNamed(ctx, specs.at(index), index));

    // File-attachment annotations are a second place PDFs keep embedded files.
    // They are not in the name tree, so a panel that only reads that tree hides them.
    for (int pageIndex = 0; pageIndex < m_pageCount; ++pageIndex)
        appendPageAttachments(ctx, m_pdf, pageIndex, files);
    return files;
}

QByteArray PdfDocument::embeddedFileContents(int index) const
{
    if (!m_pdf || index < 0)
        return {};

    const QList<EmbeddedFile> listed = embeddedFiles();
    if (index >= listed.size())
        return {};
    const EmbeddedFile file = listed.at(index);

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(m_parseMutex);

    fz_buffer *buffer = nullptr;
    pdf_page *page = nullptr;
    unsigned char *copy = nullptr;
    size_t length = 0;
    fz_var(buffer);
    fz_var(page);
    fz_var(copy);
    fz_var(length);
    fz_try(ctx)
    {
        pdf_obj *spec = nullptr;
        if (file.nameTreeIndex >= 0) {
            QList<pdf_obj *> specs;
            collectEmbedded(ctx, embeddedFilesRoot(ctx, m_pdf), specs);
            if (file.nameTreeIndex < specs.size())
                spec = specs.at(file.nameTreeIndex);
        } else if (file.pageIndex >= 0) {
            page = pdf_load_page(ctx, m_pdf, file.pageIndex);
            for (pdf_annot *annot = pdf_first_annot(ctx, page); annot;
                 annot = pdf_next_annot(ctx, annot)) {
                if (pdf_to_num(ctx, pdf_annot_obj(ctx, annot)) != file.objectNumber)
                    continue;
                if (pdf_annot_has_filespec(ctx, annot))
                    spec = pdf_annot_filespec(ctx, annot);
                break;
            }
        }
        if (spec && pdf_is_embedded_file(ctx, spec))
            buffer = pdf_load_embedded_file_contents(ctx, spec);
        unsigned char *data = nullptr;
        if (buffer)
            length = fz_buffer_storage(ctx, buffer, &data);
        if (data && length > 0) {
            copy = static_cast<unsigned char *>(fz_malloc(ctx, length));
            memcpy(copy, data, length);
        }
    }
    fz_always(ctx) {
        fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
        fz_drop_buffer(ctx, buffer);
    }
    fz_catch(ctx)
    {
        qCWarning(lcPdf) << "Could not read embedded file" << index << ":"
                         << MuContext::lastError(ctx);
        return {};
    }

    QByteArray bytes;
    if (copy) {
        bytes = QByteArray(reinterpret_cast<const char *>(copy), int(length));
        fz_free(ctx, copy);
    }
    return bytes;
}

bool PdfDocument::addEmbeddedFile(const QString &name, const QByteArray &data, const QString &mimeType)
{
    if (!m_pdf || name.trimmed().isEmpty() || data.isEmpty())
        return false;

    ensureStructure();
    fz_context *ctx = MuContext::forThread();
    const QByteArray fileName = name.trimmed().toUtf8();
    const QByteArray mime = (mimeType.trimmed().isEmpty() ? QStringLiteral("application/octet-stream")
                                                          : mimeType.trimmed())
                                .toUtf8();
    const int64_t now = QDateTime::currentSecsSinceEpoch();
    bool added = false;

    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        fz_buffer *buffer = nullptr;
        fz_var(buffer);
        fz_var(added);
        fz_try(ctx) {
            buffer = fz_new_buffer_from_copied_data(
                ctx, reinterpret_cast<const unsigned char *>(data.constData()), size_t(data.size()));
            // The checksum lets a reader notice a truncated attachment. The
            // returned filespec is not yet in the name tree, so a panel that
            // only walks EmbeddedFiles would not see it.
            pdf_obj *spec = pdf_add_embedded_file(ctx, m_pdf, fileName.constData(), mime.constData(),
                                                  buffer, now, now, 1);
            pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, m_pdf), PDF_NAME(Root));
            pdf_obj *names = pdf_dict_get(ctx, root, PDF_NAME(Names));
            if (!names) {
                names = pdf_new_dict(ctx, m_pdf, 1);
                pdf_dict_put_drop(ctx, root, PDF_NAME(Names), names);
            }
            pdf_obj *tree = pdf_dict_get(ctx, names, PDF_NAME(EmbeddedFiles));
            if (!tree) {
                tree = pdf_new_dict(ctx, m_pdf, 1);
                pdf_dict_put_drop(ctx, names, PDF_NAME(EmbeddedFiles), tree);
            }
            pdf_obj *leaf = tree;
            while (pdf_is_array(ctx, pdf_dict_get(ctx, leaf, PDF_NAME(Kids)))) {
                pdf_obj *kids = pdf_dict_get(ctx, leaf, PDF_NAME(Kids));
                const int count = pdf_array_len(ctx, kids);
                if (count <= 0)
                    break;
                leaf = pdf_array_get(ctx, kids, count - 1);
            }
            pdf_obj *entries = pdf_dict_get(ctx, leaf, PDF_NAME(Names));
            if (!entries) {
                entries = pdf_new_array(ctx, m_pdf, 2);
                pdf_dict_put_drop(ctx, leaf, PDF_NAME(Names), entries);
            }
            int at = pdf_array_len(ctx, entries);
            for (int index = 0; index + 1 < at; index += 2) {
                const char *existing = pdf_to_text_string(ctx, pdf_array_get(ctx, entries, index));
                if (std::strcmp(fileName.constData(), existing ? existing : "") < 0) {
                    at = index;
                    break;
                }
            }
            pdf_array_insert_drop(ctx, entries, pdf_new_text_string(ctx, fileName.constData()), at);
            pdf_array_insert(ctx, entries, spec, at + 1);
            pdf_drop_obj(ctx, spec);
            added = true;
        }
        fz_always(ctx)
            fz_drop_buffer(ctx, buffer);
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not embed" << name << ":" << MuContext::lastError(ctx);
            return false;
        }
    }

    if (!added)
        return false;
    markEdited();
    return true;
}

} // namespace sweet::core
