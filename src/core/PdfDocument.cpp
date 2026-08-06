#include "core/PdfDocument.h"

#include "core/Geometry.h"
#include "core/Logging.h"
#include "core/MuContext.h"

#include <QFile>
#include <QFileInfo>

namespace sweet::core {
namespace {

QString metadataValue(fz_context *ctx, fz_document *document, const char *key)
{
    char buffer[1024] = { 0 };
    int written = 0;
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

    // Cannot use make_shared: the constructor is private.
    return std::shared_ptr<PdfDocument>(new PdfDocument(document, path));
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
    m_info.subject = metadataValue(ctx, m_document, "info:Subject");
    m_info.keywords = metadataValue(ctx, m_document, "info:Keywords");
    m_info.creator = metadataValue(ctx, m_document, "info:Creator");
    m_info.producer = metadataValue(ctx, m_document, "info:Producer");
    m_info.format = metadataValue(ctx, m_document, FZ_META_FORMAT);
    m_info.creationDate = metadataValue(ctx, m_document, "info:CreationDate");
    m_info.modificationDate = metadataValue(ctx, m_document, "info:ModDate");
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

std::shared_ptr<PdfPage> PdfDocument::page(int index) const
{
    if (index < 0 || index >= m_pageCount)
        return nullptr;

    const std::lock_guard<std::mutex> guard(m_pageMutex);
    if (auto cached = m_pages.at(index).lock())
        return cached;

    auto loaded = std::make_shared<PdfPage>(m_document, index);
    m_pages[index] = loaded;
    return loaded;
}

} // namespace sweet::core
