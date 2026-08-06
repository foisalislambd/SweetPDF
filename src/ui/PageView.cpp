#include "ui/PageView.h"

#include "core/PdfDocument.h"
#include "ui/PageItem.h"
#include "ui/Theme.h"

#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

namespace sweet::ui {
namespace {

constexpr qreal pageGap = 18.0;   ///< Vertical space between pages, in points.
constexpr qreal pageMargin = 24.0;
constexpr qreal zoomStep = 1.25;

} // namespace

PageView::PageView(core::DocumentSession *session, QWidget *parent)
    : QGraphicsView(parent)
    , m_session(session)
    , m_scene(new QGraphicsScene(this))
{
    setObjectName(QStringLiteral("PageCanvas"));
    setScene(m_scene);
    setFrameShape(QFrame::NoFrame);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    setDragMode(QGraphicsView::NoDrag);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    verticalScrollBar()->setSingleStep(24);

    connect(m_session, &core::DocumentSession::documentChanged, this, &PageView::rebuildScene);
    connect(m_session, &core::DocumentSession::rotationChanged, this, [this] {
        for (PageItem *item : m_items)
            item->refreshGeometry();
        layoutPages();
        updateFitZoom();
    });
    connect(m_session, &core::DocumentSession::zoomChanged, this, &PageView::applyZoom);
    connect(m_session, &core::DocumentSession::zoomModeChanged, this,
            [this] { updateFitZoom(); });
    connect(m_session, &core::DocumentSession::toolChanged, this, &PageView::applyTool);
    connect(m_session, &core::DocumentSession::pageInvalidated, this, [this](int pageIndex) {
        if (pageIndex < 0) {
            for (PageItem *item : m_items)
                item->discardBitmap();
        } else if (PageItem *item = itemForPage(pageIndex)) {
            item->discardBitmap();
        }
    });
    connect(m_session, &core::DocumentSession::currentPageChanged, this, [this](int pageIndex) {
        if (!m_syncingCurrentPage)
            goToPage(pageIndex);
    });
    connect(m_session->renderQueue(), &core::RenderQueue::rendered, this,
            [this](const core::RenderKey &key, const QImage &) {
                if (PageItem *item = itemForPage(key.pageIndex))
                    item->update();
            });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &PageView::updateCurrentPageFromViewport);

    applyTool();
}

void PageView::rebuildScene()
{
    m_scene->clear();
    m_items.clear();

    const auto &document = m_session->document();
    if (!document)
        return;

    m_items.reserve(document->pageCount());
    for (int index = 0; index < document->pageCount(); ++index) {
        auto *item = new PageItem(m_session, index);
        m_scene->addItem(item);
        m_items.append(item);
    }

    layoutPages();
    updateFitZoom();
    horizontalScrollBar()->setValue(horizontalScrollBar()->minimum());
    verticalScrollBar()->setValue(verticalScrollBar()->minimum());
}

void PageView::layoutPages()
{
    if (m_items.isEmpty())
        return;

    const bool twoUp = m_session->layout() == core::PageLayout::TwoPage
        || m_session->layout() == core::PageLayout::TwoPageContinuous;

    qreal widest = 0.0;
    for (PageItem *item : m_items)
        widest = qMax(widest, item->pageRect().width());
    const qreal rowWidth = twoUp ? widest * 2 + pageGap : widest;

    qreal y = pageMargin;
    for (int index = 0; index < m_items.size();) {
        qreal rowHeight = 0.0;
        const int inRow = twoUp ? qMin(2, m_items.size() - index) : 1;

        qreal x = pageMargin;
        if (inRow == 1)
            x += (rowWidth - m_items.at(index)->pageRect().width()) / 2.0;

        for (int column = 0; column < inRow; ++column) {
            PageItem *item = m_items.at(index + column);
            item->setPos(x, y);
            x += item->pageRect().width() + pageGap;
            rowHeight = qMax(rowHeight, item->pageRect().height());
        }

        y += rowHeight + pageGap;
        index += inRow;
    }

    const QRectF content(0, 0, rowWidth + pageMargin * 2, y - pageGap + pageMargin);
    m_scene->setSceneRect(content);
}

void PageView::applyZoom()
{
    const qreal zoom = m_session->zoom();
    setTransform(QTransform::fromScale(zoom, zoom));
}

void PageView::applyTool()
{
    switch (m_session->tool()) {
    case core::Tool::Hand:
        setDragMode(QGraphicsView::ScrollHandDrag);
        viewport()->setCursor(Qt::OpenHandCursor);
        break;
    case core::Tool::Select:
        setDragMode(QGraphicsView::RubberBandDrag);
        viewport()->setCursor(Qt::ArrowCursor);
        break;
    case core::Tool::AddText:
    case core::Tool::EditText:
        setDragMode(QGraphicsView::NoDrag);
        viewport()->setCursor(Qt::IBeamCursor);
        break;
    case core::Tool::Draw:
    case core::Tool::Highlight:
    case core::Tool::Underline:
    case core::Tool::Strikeout:
    case core::Tool::Redact:
        setDragMode(QGraphicsView::NoDrag);
        viewport()->setCursor(Qt::CrossCursor);
        break;
    default:
        setDragMode(QGraphicsView::NoDrag);
        viewport()->setCursor(Qt::ArrowCursor);
        break;
    }
}

void PageView::updateFitZoom()
{
    if (m_items.isEmpty())
        return;

    const core::ZoomMode mode = m_session->zoomMode();
    if (mode == core::ZoomMode::Manual)
        return;

    const PageItem *reference = itemForPage(m_session->currentPage());
    if (!reference)
        reference = m_items.first();
    const QRectF page = reference->pageRect();
    if (page.isEmpty())
        return;

    const qreal availableWidth = viewport()->width() - pageMargin * 2;
    const qreal availableHeight = viewport()->height() - pageMargin * 2;
    if (availableWidth <= 0 || availableHeight <= 0)
        return;

    const qreal widthScale = availableWidth / page.width();
    const qreal zoom = mode == core::ZoomMode::FitWidth
        ? widthScale
        : qMin(widthScale, availableHeight / page.height());

    m_session->setZoom(zoom);
    applyZoom();
}

void PageView::updateCurrentPageFromViewport()
{
    if (m_items.isEmpty())
        return;

    // The page that covers the most of the viewport is the one the user reads.
    const QRectF visible = mapToScene(viewport()->rect()).boundingRect();
    int best = m_session->currentPage();
    qreal bestArea = 0.0;
    for (const PageItem *item : m_items) {
        const QRectF pageInScene = item->pageRect().translated(item->pos());
        const QRectF overlap = pageInScene.intersected(visible);
        const qreal area = overlap.width() * overlap.height();
        if (area > bestArea) {
            bestArea = area;
            best = item->pageIndex();
        }
    }

    if (best == m_session->currentPage())
        return;

    m_syncingCurrentPage = true;
    m_session->setCurrentPage(best);
    m_syncingCurrentPage = false;
}

PageItem *PageView::itemForPage(int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= m_items.size())
        return nullptr;
    return m_items.at(pageIndex);
}

void PageView::goToPage(int pageIndex)
{
    const PageItem *item = itemForPage(pageIndex);
    if (!item)
        return;

    const QPointF target = item->pos() - QPointF(0, pageGap);
    const QPoint viewTarget = mapFromScene(target);
    verticalScrollBar()->setValue(verticalScrollBar()->value() + viewTarget.y());
}

void PageView::zoomIn()
{
    m_session->setZoomMode(core::ZoomMode::Manual);
    m_session->setZoom(m_session->zoom() * zoomStep);
}

void PageView::zoomOut()
{
    m_session->setZoomMode(core::ZoomMode::Manual);
    m_session->setZoom(m_session->zoom() / zoomStep);
}

void PageView::zoomTo(qreal zoom)
{
    m_session->setZoomMode(core::ZoomMode::Manual);
    m_session->setZoom(zoom);
}

void PageView::fitPage()
{
    m_session->setZoomMode(core::ZoomMode::FitPage);
    updateFitZoom();
}

void PageView::fitWidth()
{
    m_session->setZoomMode(core::ZoomMode::FitWidth);
    updateFitZoom();
}

void PageView::actualSize()
{
    zoomTo(1.0);
}

void PageView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const int steps = event->angleDelta().y();
        if (steps != 0) {
            const qreal factor = steps > 0 ? zoomStep : 1.0 / zoomStep;
            m_session->setZoomMode(core::ZoomMode::Manual);
            m_session->setZoom(m_session->zoom() * factor);
        }
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void PageView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    updateFitZoom();
}

void PageView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton && dragMode() != QGraphicsView::ScrollHandDrag) {
        m_temporaryPan = true;
        setDragMode(QGraphicsView::ScrollHandDrag);
        // Replay as a left press so the built-in hand drag takes over.
        QMouseEvent synthetic(QEvent::MouseButtonPress, event->position(),
                             event->globalPosition(), Qt::LeftButton, Qt::LeftButton,
                             event->modifiers());
        QGraphicsView::mousePressEvent(&synthetic);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void PageView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_temporaryPan && event->button() == Qt::MiddleButton) {
        QMouseEvent synthetic(QEvent::MouseButtonRelease, event->position(),
                             event->globalPosition(), Qt::LeftButton, Qt::NoButton,
                             event->modifiers());
        QGraphicsView::mouseReleaseEvent(&synthetic);
        m_temporaryPan = false;
        applyTool();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void PageView::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_PageDown:
        m_session->setCurrentPage(m_session->currentPage() + 1);
        event->accept();
        return;
    case Qt::Key_PageUp:
        m_session->setCurrentPage(m_session->currentPage() - 1);
        event->accept();
        return;
    case Qt::Key_Home:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            m_session->setCurrentPage(0);
            event->accept();
            return;
        }
        break;
    case Qt::Key_End:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            m_session->setCurrentPage(m_session->pageCount() - 1);
            event->accept();
            return;
        }
        break;
    default:
        break;
    }
    QGraphicsView::keyPressEvent(event);
}

void PageView::drawBackground(QPainter *painter, const QRectF &rect)
{
    // Qualified: QWidget::palette() would shadow the theme helper here.
    painter->fillRect(rect, Theme::instance().palette().canvas);
}

} // namespace sweet::ui
