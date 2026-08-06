#include "core/DocumentSession.h"

#include "core/Logging.h"
#include "core/MuContext.h"
#include "core/UndoCommand.h"

#include <QFileInfo>

namespace sweet::core {

void DocumentCommand::notifyPageChanged(int pageIndex)
{
    if (!m_session)
        return;
    m_session->setModified(true);
    m_session->invalidatePage(pageIndex);
}

DocumentSession::DocumentSession(QObject *parent)
    : QObject(parent)
    , m_renderQueue(new RenderQueue(this))
    , m_undoStack(new QUndoStack(this))
{
    connect(m_undoStack, &QUndoStack::cleanChanged, this,
            [this](bool clean) { setModified(!clean); });
}

DocumentSession::~DocumentSession() = default;

bool DocumentSession::load(const QString &path, const QString &password, QString *errorMessage)
{
    try {
        auto document = PdfDocument::open(path, password);
        m_document = std::move(document);
    } catch (const MuError &failure) {
        if (errorMessage)
            *errorMessage = failure.details();
        return false;
    } catch (const std::exception &failure) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(failure.what());
        return false;
    }

    m_filePath = path;
    m_currentPage = 0;
    m_rotation = 0;
    m_modified = false;
    m_undoStack->clear();
    m_renderQueue->setDocument(m_document);

    qCInfo(lcPdf) << "Opened" << path << "with" << m_document->pageCount() << "pages";

    Q_EMIT documentChanged();
    Q_EMIT currentPageChanged(m_currentPage);
    Q_EMIT modifiedChanged(false);
    return true;
}

QString DocumentSession::filePath() const
{
    return m_filePath;
}

QString DocumentSession::displayName() const
{
    if (m_filePath.isEmpty())
        return tr("Untitled");
    return QFileInfo(m_filePath).fileName();
}

int DocumentSession::pageCount() const
{
    return m_document ? m_document->pageCount() : 0;
}

void DocumentSession::setCurrentPage(int pageIndex)
{
    const int clamped = qBound(0, pageIndex, qMax(0, pageCount() - 1));
    if (clamped == m_currentPage)
        return;
    m_currentPage = clamped;
    Q_EMIT currentPageChanged(m_currentPage);
}

void DocumentSession::setZoom(qreal zoom)
{
    const qreal clamped = qBound(minZoom, zoom, maxZoom);
    if (qFuzzyCompare(clamped, m_zoom))
        return;
    m_zoom = clamped;
    Q_EMIT zoomChanged(m_zoom);
}

void DocumentSession::setZoomMode(ZoomMode mode)
{
    if (mode == m_zoomMode)
        return;
    m_zoomMode = mode;
    Q_EMIT zoomModeChanged(m_zoomMode);
}

void DocumentSession::setRotation(int degrees)
{
    const int normalised = ((degrees % 360) + 360) % 360;
    if (normalised == m_rotation)
        return;
    m_rotation = normalised;
    m_renderQueue->discardQueued();
    m_renderQueue->cache().clear();
    Q_EMIT rotationChanged(m_rotation);
}

void DocumentSession::rotateBy(int degrees)
{
    setRotation(m_rotation + degrees);
}

void DocumentSession::setTool(Tool tool)
{
    if (tool == m_tool)
        return;
    m_tool = tool;
    Q_EMIT toolChanged(m_tool);
}

void DocumentSession::setLayout(PageLayout layout)
{
    if (layout == m_layout)
        return;
    m_layout = layout;
    Q_EMIT layoutChanged(m_layout);
}

void DocumentSession::setModified(bool modified)
{
    if (modified == m_modified)
        return;
    m_modified = modified;
    Q_EMIT modifiedChanged(m_modified);
}

void DocumentSession::invalidatePage(int pageIndex)
{
    if (pageIndex < 0)
        m_renderQueue->cache().clear();
    else
        m_renderQueue->cache().invalidatePage(pageIndex);
    Q_EMIT pageInvalidated(pageIndex);
}

} // namespace sweet::core
