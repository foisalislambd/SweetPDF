#include "core/pdf/PdfDocument.h"

#include "core/pdf/Geometry.h"
#include "core/pdf/MuContext.h"
#include "core/pdf/MuVersion.h"
#include "core/util/Logging.h"

#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace sweet::core {
namespace {

QString metadataValue(fz_context *ctx, fz_document *document, const char *key)
{
    char buffer[1024] = { 0 };
    int written = 0;
    fz_var(written);
    fz_try(ctx)
        written = fz_lookup_metadata(ctx, document, key, buffer, sizeof(buffer));
    fz_catch(ctx)
        written = -1;

    if (written <= 0)
        return {};
    return QString::fromUtf8(buffer);
}

int resolveOutlinePage(fz_context *ctx, fz_document *document, fz_outline *item)
{
    if (item->page.page >= 0)
        return item->page.page;
    if (!item->uri)
        return -1;

    fz_link_dest dest{};
    fz_try(ctx)
        dest = fz_resolve_link_dest(ctx, document, item->uri);
    fz_catch(ctx)
        return -1;
    return dest.loc.page;
}

QList<OutlineNode> convertOutline(fz_context *ctx, fz_document *document, fz_outline *first)
{
    QList<OutlineNode> nodes;
    for (fz_outline *item = first; item; item = item->next) {
        OutlineNode node;
        node.title = item->title ? QString::fromUtf8(item->title) : QString();
        node.uri = item->uri ? QString::fromUtf8(item->uri) : QString();
        node.openByDefault = item->is_open != 0;
        node.bold = (item->flags & FZ_OUTLINE_FLAG_BOLD) != 0;
        node.italic = (item->flags & FZ_OUTLINE_FLAG_ITALIC) != 0;
        if (item->r != 0 || item->g != 0 || item->b != 0)
            node.color = QColor(int(item->r), int(item->g), int(item->b));
        node.pageIndex = resolveOutlinePage(ctx, document, item);
        node.pageOffset = QPointF(item->x, item->y);
        node.children = convertOutline(ctx, document, item->down);
        nodes.append(std::move(node));
    }
    return nodes;
}

} // namespace

PdfDocument::PdfDocument(fz_document *document, QString path)
    : m_document(document)
    , m_filePath(std::move(path))
{
    fz_context *ctx = MuContext::forThread();
    m_pdf = pdf_specifics(ctx, m_document);

    fz_try(ctx)
        m_pageCount = fz_count_pages(ctx, m_document);
    fz_catch(ctx)
        MuContext::raise(ctx, QStringLiteral("Could not count the pages of the document"));

    m_encrypted = fz_needs_password(ctx, m_document) != 0;
    m_pages.resize(m_pageCount);

    readPageSizes();
    readPermissions();
    readInfo();
    readOutline();
}

PdfDocument::~PdfDocument()
{
    {
        const std::lock_guard<std::mutex> guard(m_pageMutex);
        m_pages.clear();
    }
    if (m_document) {
        fz_drop_document(MuContext::forThread(), m_document);
        m_document = nullptr;
    }
}

std::shared_ptr<PdfDocument> PdfDocument::open(const QString &path, const QString &password)
{
    fz_context *ctx = MuContext::forThread();
    const QByteArray nativePath = QFile::encodeName(path);

    fz_document *document = nullptr;
    fz_try(ctx)
        document = fz_open_document(ctx, nativePath.constData());
    fz_catch(ctx)
        MuContext::raise(ctx, QStringLiteral("Could not open %1").arg(QFileInfo(path).fileName()));

    if (fz_needs_password(ctx, document)) {
        const QByteArray utf8 = password.toUtf8();
        if (!fz_authenticate_password(ctx, document, utf8.constData())) {
            fz_drop_document(ctx, document);
            throw MuError(QStringLiteral("The document is password protected"));
        }
    }

    return adopt(document, path);
}

std::shared_ptr<PdfDocument> PdfDocument::adopt(fz_document *document, QString path,
                                                QByteArray backingBytes)
{
    // Cannot use make_shared: the constructor is private.
    auto wrapped = std::shared_ptr<PdfDocument>(new PdfDocument(document, std::move(path)));
    wrapped->m_backingBytes = std::move(backingBytes);
    return wrapped;
}

std::shared_ptr<PdfDocument> PdfDocument::openFromBytes(QByteArray bytes, QString displayPath)
{
    fz_context *ctx = MuContext::forThread();

    fz_buffer *buffer = nullptr;
    fz_stream *stream = nullptr;
    fz_document *document = nullptr;
    fz_var(buffer);
    fz_var(stream);
    fz_var(document);
    fz_try(ctx) {
        // The buffer borrows the QByteArray, which the document keeps alive.
        buffer = fz_new_buffer_from_shared_data(
            ctx, reinterpret_cast<const unsigned char *>(bytes.constData()), size_t(bytes.size()));
        stream = fz_open_buffer(ctx, buffer);
        document = fz_open_document_with_stream(ctx, "application/pdf", stream);
    }
    fz_always(ctx) {
        fz_drop_stream(ctx, stream);
        fz_drop_buffer(ctx, buffer);
    }
    fz_catch(ctx)
        MuContext::raise(ctx, QStringLiteral("Could not re-open the edited document"));

    return adopt(document, std::move(displayPath), std::move(bytes));
}

std::shared_ptr<PdfDocument> PdfDocument::createEmpty(const QSizeF &pageSizePoints)
{
    fz_context *ctx = MuContext::forThread();

    pdf_document *pdf = nullptr;
    pdf_obj *pageObject = nullptr;
    fz_var(pdf);
    fz_var(pageObject);
    fz_try(ctx) {
        pdf = pdf_create_document(ctx);
        const fz_rect mediabox = fz_make_rect(0, 0, float(pageSizePoints.width()),
                                              float(pageSizePoints.height()));
        pageObject = pdf_add_page(ctx, pdf, mediabox, 0, nullptr, nullptr);
        pdf_insert_page(ctx, pdf, 0, pageObject);
    }
    fz_always(ctx)
        pdf_drop_obj(ctx, pageObject);
    fz_catch(ctx) {
        pdf_drop_document(ctx, pdf);
        MuContext::raise(ctx, QStringLiteral("Could not create an empty document"));
    }

    auto created = adopt(reinterpret_cast<fz_document *>(pdf), QString());
    created->m_edited = true;
    return created;
}

bool PdfDocument::needsPassword(const QString &path)
{
    fz_context *ctx = MuContext::forThread();
    const QByteArray nativePath = QFile::encodeName(path);

    fz_document *document = nullptr;
    fz_try(ctx)
        document = fz_open_document(ctx, nativePath.constData());
    fz_catch(ctx)
        return false;

    const bool needs = fz_needs_password(ctx, document) != 0;
    fz_drop_document(ctx, document);
    return needs;
}

void PdfDocument::readPageSizes()
{
    fz_context *ctx = MuContext::forThread();
    m_pageSizes.reserve(m_pageCount);

    for (int i = 0; i < m_pageCount; ++i) {
        fz_rect bounds = fz_empty_rect;
        fz_page *page = nullptr;
        fz_var(bounds);
        fz_var(page);
        fz_try(ctx) {
            page = fz_load_page(ctx, m_document, i);
            bounds = fz_bound_page(ctx, page);
        }
        fz_always(ctx)
            fz_drop_page(ctx, page);
        fz_catch(ctx)
            bounds = fz_make_rect(0, 0, 612, 792); // US Letter fallback

        const QSizeF size = sizeOf(bounds);
        m_pageSizes.append(size);
        m_maxPageSize.setWidth(qMax(m_maxPageSize.width(), size.width()));
        m_maxPageSize.setHeight(qMax(m_maxPageSize.height(), size.height()));
    }
}

void PdfDocument::readPermissions()
{
    fz_context *ctx = MuContext::forThread();
    m_permissions.print = fz_has_permission(ctx, m_document, FZ_PERMISSION_PRINT) != 0;
    m_permissions.modify = fz_has_permission(ctx, m_document, FZ_PERMISSION_EDIT) != 0;
    m_permissions.copy = fz_has_permission(ctx, m_document, FZ_PERMISSION_COPY) != 0;
    m_permissions.annotate = fz_has_permission(ctx, m_document, FZ_PERMISSION_ANNOTATE) != 0;
    m_permissions.form = fz_has_permission(ctx, m_document, FZ_PERMISSION_FORM) != 0;
    m_permissions.assemble = fz_has_permission(ctx, m_document, FZ_PERMISSION_ASSEMBLE) != 0;
}

void PdfDocument::readInfo()
{
    fz_context *ctx = MuContext::forThread();
    m_info.title = metadataValue(ctx, m_document, FZ_META_INFO_TITLE);
    m_info.author = metadataValue(ctx, m_document, FZ_META_INFO_AUTHOR);
    m_info.subject = metadataValue(ctx, m_document, FZ_META_INFO_SUBJECT);
    m_info.keywords = metadataValue(ctx, m_document, FZ_META_INFO_KEYWORDS);
    m_info.creator = metadataValue(ctx, m_document, FZ_META_INFO_CREATOR);
    m_info.producer = metadataValue(ctx, m_document, FZ_META_INFO_PRODUCER);
    m_info.format = metadataValue(ctx, m_document, FZ_META_FORMAT);
    m_info.creationDate = metadataValue(ctx, m_document, FZ_META_INFO_CREATIONDATE);
    m_info.modificationDate = metadataValue(ctx, m_document, FZ_META_INFO_MODIFICATIONDATE);
    m_info.fileSize = QFileInfo(m_filePath).size();
}

void PdfDocument::readOutline()
{
    fz_context *ctx = MuContext::forThread();

    fz_outline *outline = nullptr;
    fz_try(ctx)
        outline = fz_load_outline(ctx, m_document);
    fz_catch(ctx) {
        qCWarning(lcPdf) << "Could not read the outline:" << MuContext::lastError(ctx);
        return;
    }

    if (!outline)
        return;

    m_outline = convertOutline(ctx, m_document, outline);
    fz_drop_outline(ctx, outline);
}

QSizeF PdfDocument::pageSizePoints(int index) const
{
    if (index < 0 || index >= m_pageSizes.size())
        return {};
    return m_pageSizes.at(index);
}

// --- Editing ---------------------------------------------------------------

namespace {

int permissionBits(const DocumentPermissions &permissions)
{
    int bits = 0;
    if (permissions.print)
        bits |= PDF_PERM_PRINT | PDF_PERM_PRINT_HQ;
    if (permissions.modify)
        bits |= PDF_PERM_MODIFY;
    if (permissions.copy)
        bits |= PDF_PERM_COPY;
    if (permissions.annotate)
        bits |= PDF_PERM_ANNOTATE;
    if (permissions.form)
        bits |= PDF_PERM_FORM;
    if (permissions.assemble)
        bits |= PDF_PERM_ASSEMBLE;
    return bits;
}

pdf_write_options toWriteOptions(const SaveOptions &options)
{
    pdf_write_options write = pdf_default_write_options;
    write.do_incremental = options.incremental ? 1 : 0;
    // MuPDF 1.26 removed linearisation and later versions throw if asked for it.
    write.do_linear = 0;
    write.do_compress = options.compressStreams ? 1 : 0;
    write.do_compress_images = options.compressStreams ? 1 : 0;
    write.do_compress_fonts = options.compressStreams ? 1 : 0;
    write.do_decompress = options.decompressStreams ? 1 : 0;
    write.do_clean = options.cleanContentStreams ? 1 : 0;
    write.do_sanitize = options.cleanContentStreams ? 1 : 0;
    // Garbage collection renumbers objects, which an incremental save cannot do.
    write.do_garbage = (options.garbageCollect && !options.incremental) ? 3 : 0;
    // Object streams and fresh appearance streams belong on a full rewrite.
    // An incremental save must leave existing objects, including appearances, alone.
    write.do_use_objstms = (options.garbageCollect && !options.incremental) ? 1 : 0;
    write.do_appearance = options.incremental ? 0 : 1;

    switch (options.encryption) {
    case SaveOptions::Encryption::Keep:
        write.do_encrypt = PDF_ENCRYPT_KEEP;
        break;
    case SaveOptions::Encryption::None:
        write.do_encrypt = PDF_ENCRYPT_NONE;
        break;
    case SaveOptions::Encryption::Aes128:
        write.do_encrypt = PDF_ENCRYPT_AES_128;
        break;
    case SaveOptions::Encryption::Aes256:
        write.do_encrypt = PDF_ENCRYPT_AES_256;
        break;
    }

    if (options.encryption != SaveOptions::Encryption::Keep
        && options.encryption != SaveOptions::Encryption::None) {
        write.permissions = permissionBits(options.permissions);
        const QByteArray user = options.userPassword.toUtf8();
        const QByteArray owner = options.ownerPassword.toUtf8();
        qstrncpy(write.upwd_utf8, user.constData(), sizeof(write.upwd_utf8));
        qstrncpy(write.opwd_utf8, owner.constData(), sizeof(write.opwd_utf8));
    }

    return write;
}

/// Serialises `pdf` into a QByteArray.
QByteArray writeToBytes(fz_context *ctx, pdf_document *pdf, const pdf_write_options &options)
{
    fz_buffer *buffer = nullptr;
    fz_output *output = nullptr;
    fz_var(buffer);
    fz_var(output);

    QByteArray bytes;
    fz_try(ctx) {
        buffer = fz_new_buffer(ctx, 64 * 1024);
        output = fz_new_output_with_buffer(ctx, buffer);
        pdf_write_document(ctx, pdf, output, &options);
        fz_close_output(ctx, output);

        unsigned char *data = nullptr;
        const size_t size = fz_buffer_storage(ctx, buffer, &data);
        bytes = QByteArray(reinterpret_cast<const char *>(data), qsizetype(size));
    }
    fz_always(ctx) {
        fz_drop_output(ctx, output);
        fz_drop_buffer(ctx, buffer);
    }
    fz_catch(ctx)
        MuContext::raise(ctx, QStringLiteral("Could not write the document"));

    return bytes;
}

} // namespace

void PdfDocument::ensureStructure() const
{
    if (m_structureChecked || !m_pdf)
        return;
    m_structureChecked = true;

    fz_context *ctx = MuContext::forThread();
    int repaired = 0;
    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        fz_var(repaired);
        fz_try(ctx)
        {
#if SWEETPDF_MUPDF_1_28
            repaired = pdf_check_document(ctx, m_pdf);
#else
            pdf_check_document(ctx, m_pdf);
#endif
        }
        fz_catch(ctx)
        {
            m_structureWarning = MuContext::lastError(ctx);
            qCWarning(lcPdf) << "Structure check failed:" << m_structureWarning;
            return;
        }
    }

    m_structureRepaired = repaired != 0;
    if (m_structureRepaired) {
        qCWarning(lcPdf) << "MuPDF repaired the PDF structure before editing";
        discardPageCaches(-1);
    }
}

pdf_document *PdfDocument::requirePdf() const
{
    ensureStructure();
    if (!m_pdf) {
        throw MuError(QStringLiteral("This file format can be viewed but not edited. "
                                     "Save it as a PDF first."));
    }
    return m_pdf;
}

void PdfDocument::markEdited()
{
    m_edited = true;
}

bool PdfDocument::hasUnsavedChanges() const
{
    if (m_edited)
        return true;
    if (!m_pdf)
        return false;
    return pdf_has_unsaved_changes(MuContext::forThread(), m_pdf) != 0;
}

bool PdfDocument::canSaveIncrementally() const
{
    if (!m_pdf || m_filePath.isEmpty() || !m_backingBytes.isEmpty())
        return false;
    if (m_encrypted)
        return false;

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(m_parseMutex);
    int ok = 0;
    fz_var(ok);
    fz_try(ctx)
        ok = pdf_can_be_saved_incrementally(ctx, m_pdf);
    fz_catch(ctx)
        ok = 0;
    return ok != 0;
}

QByteArray PdfDocument::toBytes(const SaveOptions &options) const
{
    pdf_document *pdf = requirePdf();
    const std::lock_guard<std::mutex> guard(m_parseMutex);

    // An incremental write only makes sense against a file; in memory there is
    // nothing to append to.
    SaveOptions inMemory = options;
    inMemory.incremental = false;
    return writeToBytes(MuContext::forThread(), pdf, toWriteOptions(inMemory));
}

void PdfDocument::saveToFile(const QString &path, const SaveOptions &options) const
{
    pdf_document *pdf = requirePdf();
    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(m_parseMutex);

    SaveOptions effective = options;
    if (effective.incremental) {
        if (path != m_filePath || !m_backingBytes.isEmpty() || m_encrypted) {
            effective.incremental = false;
        } else {
            int ok = 0;
            fz_var(ok);
            fz_try(ctx)
                ok = pdf_can_be_saved_incrementally(ctx, pdf);
            fz_catch(ctx)
                ok = 0;
            if (ok == 0)
                effective.incremental = false;
        }
    }

    const pdf_write_options write = toWriteOptions(effective);
    const QByteArray nativePath = QFile::encodeName(path);

    fz_try(ctx)
        pdf_save_document(ctx, pdf, nativePath.constData(), &write);
    fz_catch(ctx)
        MuContext::raise(ctx, QStringLiteral("Could not save %1").arg(QFileInfo(path).fileName()));
}

void PdfDocument::setInfo(const DocumentInfo &info)
{
    requirePdf();
    fz_context *ctx = MuContext::forThread();
    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        auto setField = [&](const char *key, const QString &value) {
            const QByteArray utf8 = value.toUtf8();
            fz_set_metadata(ctx, m_document, key, utf8.constData());
        };
        fz_try(ctx) {
            setField(FZ_META_INFO_TITLE, info.title);
            setField(FZ_META_INFO_AUTHOR, info.author);
            setField(FZ_META_INFO_SUBJECT, info.subject);
            setField(FZ_META_INFO_KEYWORDS, info.keywords);
            setField(FZ_META_INFO_CREATOR, info.creator);
            setField(FZ_META_INFO_PRODUCER, info.producer);
            setField(FZ_META_INFO_CREATIONDATE, info.creationDate);
            setField(FZ_META_INFO_MODIFICATIONDATE, info.modificationDate);
        }
        fz_catch(ctx)
            MuContext::raise(ctx, QStringLiteral("Could not update document metadata"));
    }
    markEdited();
    readInfo();
}

int PdfDocument::pageRotation(int index) const
{
    if (!m_pdf || index < 0 || index >= m_pageCount)
        return 0;

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(m_parseMutex);

    int rotation = 0;
    fz_var(rotation);
    fz_try(ctx) {
        pdf_obj *pageObject = pdf_lookup_page_obj(ctx, m_pdf, index);
        rotation = pdf_dict_get_inheritable_int(ctx, pageObject, PDF_NAME(Rotate));
    }
    fz_catch(ctx)
        rotation = 0;
    return ((rotation % 360) + 360) % 360;
}

void PdfDocument::setPageRotation(int index, int degrees)
{
    pdf_document *pdf = requirePdf();
    if (index < 0 || index >= m_pageCount)
        return;

    fz_context *ctx = MuContext::forThread();
    const int normalised = ((degrees % 360) + 360) % 360;
    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        fz_try(ctx) {
            pdf_obj *pageObject = pdf_lookup_page_obj(ctx, pdf, index);
            pdf_dict_put_int(ctx, pageObject, PDF_NAME(Rotate), normalised);
        }
        fz_catch(ctx)
            MuContext::raise(ctx, QStringLiteral("Could not rotate page %1").arg(index + 1));
    }
    markEdited();

    {
        const std::lock_guard<std::mutex> guard(m_pageMutex);
        if (index < m_pages.size()) {
            if (auto cached = m_pages.at(index).lock())
                cached->refreshBounds();
        }
    }
    updateCachedPageSize(index);
}

void PdfDocument::deletePages(QList<int> indices)
{
    pdf_document *pdf = requirePdf();
    if (indices.isEmpty())
        return;

    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    if (indices.size() >= m_pageCount)
        throw MuError(QStringLiteral("A document must keep at least one page"));

    fz_context *ctx = MuContext::forThread();
    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        fz_try(ctx) {
            // Back to front, so each removal leaves the earlier indices valid.
            for (auto it = indices.crbegin(); it != indices.crend(); ++it) {
                if (*it >= 0 && *it < m_pageCount)
                    pdf_delete_page(ctx, pdf, *it);
            }
        }
        fz_catch(ctx)
            MuContext::raise(ctx, QStringLiteral("Could not delete the selected pages"));
    }
    markEdited();
}

void PdfDocument::insertBlankPage(int at, const QSizeF &sizePoints)
{
    pdf_document *pdf = requirePdf();
    fz_context *ctx = MuContext::forThread();
    const int position = qBound(0, at, m_pageCount);

    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        pdf_obj *pageObject = nullptr;
        fz_var(pageObject);
        fz_try(ctx) {
            const fz_rect mediabox = fz_make_rect(0, 0, float(sizePoints.width()),
                                                  float(sizePoints.height()));
            pageObject = pdf_add_page(ctx, pdf, mediabox, 0, nullptr, nullptr);
            pdf_insert_page(ctx, pdf, position, pageObject);
        }
        fz_always(ctx)
            pdf_drop_obj(ctx, pageObject);
        fz_catch(ctx)
            MuContext::raise(ctx, QStringLiteral("Could not insert a page"));
    }
    markEdited();
}

void PdfDocument::movePage(int from, int to)
{
    pdf_document *pdf = requirePdf();
    if (from < 0 || from >= m_pageCount || from == to)
        return;

    fz_context *ctx = MuContext::forThread();
    const int target = qBound(0, to, m_pageCount - 1);

    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        pdf_obj *pageObject = nullptr;
        fz_var(pageObject);
        fz_try(ctx) {
            // Keep a reference: deleting the page drops the tree's own.
            pageObject = pdf_keep_obj(ctx, pdf_lookup_page_obj(ctx, pdf, from));
            pdf_delete_page(ctx, pdf, from);
            pdf_insert_page(ctx, pdf, target, pageObject);
        }
        fz_always(ctx)
            pdf_drop_obj(ctx, pageObject);
        fz_catch(ctx)
            MuContext::raise(ctx, QStringLiteral("Could not move page %1").arg(from + 1));
    }
    markEdited();
}

void PdfDocument::reorderPages(const QList<int> &newOrder)
{
    requirePdf();
    if (newOrder.size() != m_pageCount)
        throw MuError(QStringLiteral("The new page order does not cover every page"));

    // `current` tracks where each original page sits now, so each page is moved
    // at most once and no index goes stale.
    QList<int> current(m_pageCount);
    for (int index = 0; index < m_pageCount; ++index)
        current[index] = index;

    for (int target = 0; target < m_pageCount; ++target) {
        const int wanted = newOrder.at(target);
        const int at = int(current.indexOf(wanted));
        if (at < 0 || at == target)
            continue;
        movePage(at, target);
        current.move(at, target);
    }
}

QByteArray PdfDocument::extractPages(const QList<int> &indices) const
{
    pdf_document *source = requirePdf();
    if (indices.isEmpty())
        throw MuError(QStringLiteral("No pages were selected"));

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(m_parseMutex);

    pdf_document *destination = nullptr;
    pdf_graft_map *map = nullptr;
    fz_var(destination);
    fz_var(map);

    QByteArray bytes;
    fz_try(ctx) {
        destination = pdf_create_document(ctx);
        map = pdf_new_graft_map(ctx, destination);
        for (const int index : indices) {
            if (index >= 0 && index < m_pageCount)
                pdf_graft_mapped_page(ctx, map, -1, source, index);
        }
        bytes = writeToBytes(ctx, destination, pdf_default_write_options);
    }
    fz_always(ctx) {
        pdf_drop_graft_map(ctx, map);
        pdf_drop_document(ctx, destination);
    }
    fz_catch(ctx)
        MuContext::raise(ctx, QStringLiteral("Could not extract the selected pages"));

    return bytes;
}

void PdfDocument::duplicatePage(int index)
{
    if (index < 0 || index >= m_pageCount)
        return;

    // Round-tripping through a one-page document avoids grafting a document into
    // itself, which MuPDF's graft map is not built for.
    const QByteArray single = extractPages({ index });
    pdf_document *destination = requirePdf();
    fz_context *ctx = MuContext::forThread();

    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        fz_buffer *buffer = nullptr;
        fz_stream *stream = nullptr;
        pdf_document *source = nullptr;
        pdf_graft_map *map = nullptr;
        fz_var(buffer);
        fz_var(stream);
        fz_var(source);
        fz_var(map);
        fz_try(ctx) {
            buffer = fz_new_buffer_from_copied_data(
                ctx, reinterpret_cast<const unsigned char *>(single.constData()),
                size_t(single.size()));
            stream = fz_open_buffer(ctx, buffer);
            source = pdf_open_document_with_stream(ctx, stream);
            map = pdf_new_graft_map(ctx, destination);
            pdf_graft_mapped_page(ctx, map, index + 1, source, 0);
        }
        fz_always(ctx) {
            pdf_drop_graft_map(ctx, map);
            pdf_drop_document(ctx, source);
            fz_drop_stream(ctx, stream);
            fz_drop_buffer(ctx, buffer);
        }
        fz_catch(ctx)
            MuContext::raise(ctx, QStringLiteral("Could not duplicate page %1").arg(index + 1));
    }
    markEdited();
}

void PdfDocument::appendPagesFrom(const QString &path, const QString &password)
{
    pdf_document *destination = requirePdf();
    fz_context *ctx = MuContext::forThread();
    const QByteArray nativePath = QFile::encodeName(path);

    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        fz_document *opened = nullptr;
        pdf_graft_map *map = nullptr;
        fz_var(opened);
        fz_var(map);
        fz_try(ctx) {
            opened = fz_open_document(ctx, nativePath.constData());
            if (fz_needs_password(ctx, opened)) {
                const QByteArray utf8 = password.toUtf8();
                if (!fz_authenticate_password(ctx, opened, utf8.constData()))
                    fz_throw(ctx, FZ_ERROR_GENERIC, "wrong password");
            }

            pdf_document *source = pdf_specifics(ctx, opened);
            if (!source)
                fz_throw(ctx, FZ_ERROR_GENERIC, "not a PDF");

            map = pdf_new_graft_map(ctx, destination);
            const int count = pdf_count_pages(ctx, source);
            for (int index = 0; index < count; ++index)
                pdf_graft_mapped_page(ctx, map, -1, source, index);
        }
        fz_always(ctx) {
            pdf_drop_graft_map(ctx, map);
            fz_drop_document(ctx, opened);
        }
        fz_catch(ctx)
            MuContext::raise(ctx, QStringLiteral("Could not merge %1")
                                      .arg(QFileInfo(path).fileName()));
    }
    markEdited();
}

bool PdfDocument::applyRedactions(int pageIndex, const RedactOptions &options)
{
    pdf_document *pdf = requirePdf();
    if (pageIndex < 0 || pageIndex >= m_pageCount)
        return false;

    fz_context *ctx = MuContext::forThread();
    bool changed = false;
    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        pdf_page *page = nullptr;
        fz_var(page);
        fz_var(changed);
        fz_try(ctx) {
            page = pdf_load_page(ctx, pdf, pageIndex);
            pdf_redact_options redact{};
            redact.black_boxes = options.blackBoxes ? 1 : 0;
            switch (options.images) {
            case RedactOptions::ImageMethod::None:
                redact.image_method = PDF_REDACT_IMAGE_NONE;
                break;
            case RedactOptions::ImageMethod::Remove:
                redact.image_method = PDF_REDACT_IMAGE_REMOVE;
                break;
            case RedactOptions::ImageMethod::Pixels:
                redact.image_method = PDF_REDACT_IMAGE_PIXELS;
                break;
            }
            switch (options.lineArt) {
            case RedactOptions::LineArt::None:
                redact.line_art = PDF_REDACT_LINE_ART_NONE;
                break;
            case RedactOptions::LineArt::RemoveIfCovered:
                redact.line_art = PDF_REDACT_LINE_ART_REMOVE_IF_COVERED;
                break;
            case RedactOptions::LineArt::RemoveIfTouched:
                redact.line_art = PDF_REDACT_LINE_ART_REMOVE_IF_TOUCHED;
                break;
            }
            redact.text = PDF_REDACT_TEXT_REMOVE;
            changed = pdf_redact_page(ctx, pdf, page, &redact) != 0;
        }
        fz_always(ctx)
            fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
        fz_catch(ctx)
            MuContext::raise(ctx, QStringLiteral("Could not apply redactions on page %1")
                                      .arg(pageIndex + 1));
    }

    if (changed) {
        markEdited();
        discardPageCaches(pageIndex);
    }
    return changed;
}

void PdfDocument::discardPageCaches(int pageIndex) const
{
    const std::lock_guard<std::mutex> guard(m_pageMutex);
    auto discard = [](const std::weak_ptr<PdfPage> &weak) {
        if (auto page = weak.lock())
            page->discardDisplayList();
    };
    if (pageIndex < 0) {
        for (const auto &weak : m_pages)
            discard(weak);
        return;
    }
    if (pageIndex < m_pages.size())
        discard(m_pages.at(pageIndex));
}

void PdfDocument::updateCachedPageSize(int index)
{
    if (index < 0 || index >= m_pageCount)
        return;

    QSizeF size;
    {
        const std::lock_guard<std::mutex> guard(m_pageMutex);
        if (index < m_pages.size()) {
            if (auto cached = m_pages.at(index).lock())
                size = cached->sizePoints();
        }
    }
    if (!size.isValid() || size.isEmpty()) {
        fz_context *ctx = MuContext::forThread();
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        fz_page *page = nullptr;
        fz_rect bounds = fz_empty_rect;
        fz_var(page);
        fz_var(bounds);
        fz_try(ctx) {
            page = fz_load_page(ctx, m_document, index);
            bounds = fz_bound_page(ctx, page);
        }
        fz_always(ctx)
            fz_drop_page(ctx, page);
        fz_catch(ctx)
            bounds = fz_make_rect(0, 0, 612, 792);
        size = sizeOf(bounds);
    }

    if (index < m_pageSizes.size())
        m_pageSizes[index] = size;
    else
        m_pageSizes.append(size);

    m_maxPageSize = QSizeF();
    for (const QSizeF &pageSize : m_pageSizes) {
        m_maxPageSize.setWidth(qMax(m_maxPageSize.width(), pageSize.width()));
        m_maxPageSize.setHeight(qMax(m_maxPageSize.height(), pageSize.height()));
    }
}

namespace {

/// Moves `iter` onto the outline item addressed by `path`. The iterator starts
/// on the first top-level item when the document has one.
bool seekOutline(fz_context *ctx, fz_outline_iterator *iter, const QList<int> &path)
{
    if (path.isEmpty() || !fz_outline_iterator_item(ctx, iter))
        return false;

    for (int depth = 0; depth < path.size(); ++depth) {
        if (depth > 0 && fz_outline_iterator_down(ctx, iter) != 0)
            return false;
        for (int step = 0; step < path.at(depth); ++step) {
            if (fz_outline_iterator_next(ctx, iter) != 0)
                return false;
        }
    }
    return fz_outline_iterator_item(ctx, iter) != nullptr;
}

} // namespace

bool PdfDocument::addBookmark(const QString &title, int pageIndex)
{
    if (!m_pdf || title.isEmpty() || pageIndex < 0 || pageIndex >= m_pageCount)
        return false;

    ensureStructure();
    fz_context *ctx = MuContext::forThread();
    const QByteArray titleUtf8 = title.toUtf8();
    bool added = false;

    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        fz_outline_iterator *iter = nullptr;
        char *uri = nullptr;
        fz_var(iter);
        fz_var(uri);
        fz_var(added);
        fz_try(ctx) {
            const fz_link_dest dest = fz_make_link_dest_xyz(0, pageIndex, 0, 0, 0);
            uri = fz_format_link_uri(ctx, m_document, dest);
            iter = fz_new_outline_iterator(ctx, m_document);
            if (fz_outline_iterator_item(ctx, iter)) {
                while (fz_outline_iterator_next(ctx, iter) == 0) {
                }
            }
            fz_outline_item item{};
            item.title = const_cast<char *>(titleUtf8.constData());
            item.uri = uri;
            fz_outline_iterator_insert(ctx, iter, &item);
            added = true;
        }
        fz_always(ctx) {
            fz_drop_outline_iterator(ctx, iter);
            fz_free(ctx, uri);
        }
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not add a bookmark:" << MuContext::lastError(ctx);
            return false;
        }
    }

    if (!added)
        return false;
    markEdited();
    readOutline();
    return true;
}

bool PdfDocument::renameBookmark(const QList<int> &path, const QString &title)
{
    if (!m_pdf || path.isEmpty() || title.isEmpty())
        return false;

    ensureStructure();
    fz_context *ctx = MuContext::forThread();
    const QByteArray titleUtf8 = title.toUtf8();
    bool renamed = false;

    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        fz_outline_iterator *iter = nullptr;
        fz_var(iter);
        fz_var(renamed);
        fz_try(ctx) {
            iter = fz_new_outline_iterator(ctx, m_document);
            if (!seekOutline(ctx, iter, path))
                fz_throw(ctx, FZ_ERROR_ARGUMENT, "bookmark not found");
            fz_outline_item *current = fz_outline_iterator_item(ctx, iter);
            fz_outline_item item = *current;
            item.title = const_cast<char *>(titleUtf8.constData());
            fz_outline_iterator_update(ctx, iter, &item);
            renamed = true;
        }
        fz_always(ctx)
            fz_drop_outline_iterator(ctx, iter);
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not rename a bookmark:" << MuContext::lastError(ctx);
            return false;
        }
    }

    if (!renamed)
        return false;
    markEdited();
    readOutline();
    return true;
}

bool PdfDocument::removeBookmark(const QList<int> &path)
{
    if (!m_pdf || path.isEmpty())
        return false;

    ensureStructure();
    fz_context *ctx = MuContext::forThread();
    bool removed = false;

    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        fz_outline_iterator *iter = nullptr;
        fz_var(iter);
        fz_var(removed);
        fz_try(ctx) {
            iter = fz_new_outline_iterator(ctx, m_document);
            if (!seekOutline(ctx, iter, path))
                fz_throw(ctx, FZ_ERROR_ARGUMENT, "bookmark not found");
            fz_outline_iterator_delete(ctx, iter);
            removed = true;
        }
        fz_always(ctx)
            fz_drop_outline_iterator(ctx, iter);
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not remove a bookmark:" << MuContext::lastError(ctx);
            return false;
        }
    }

    if (!removed)
        return false;
    markEdited();
    readOutline();
    return true;
}

void PdfDocument::refreshStructure()
{
    fz_context *ctx = MuContext::forThread();

    {
        const std::lock_guard<std::mutex> guard(m_pageMutex);
        m_pages.clear();
    }

    fz_try(ctx)
        m_pageCount = fz_count_pages(ctx, m_document);
    fz_catch(ctx)
        m_pageCount = 0;

    m_pages.resize(m_pageCount);
    m_pageSizes.clear();
    m_maxPageSize = QSizeF();
    m_outline.clear();

    readPageSizes();
    readInfo();
    readOutline();
}

std::shared_ptr<PdfPage> PdfDocument::page(int index) const
{
    if (index < 0 || index >= m_pageCount)
        return nullptr;

    const std::lock_guard<std::mutex> guard(m_pageMutex);
    if (auto cached = m_pages.at(index).lock())
        return cached;

    auto loaded = std::make_shared<PdfPage>(m_document, index, m_parseMutex);
    m_pages[index] = loaded;
    return loaded;
}

} // namespace sweet::core
