#include "ui/viewer/PageView.h"

#include "core/pdf/PdfDocument.h"
#include "ui/theme/Theme.h"
#include "ui/viewer/PageItem.h"

#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QWheelEvent>

namespace sweet::ui {
namespace {

constexpr qreal pageGap = 20.0; ///< Vertical space between pages, in points.
constexpr qreal pageMargin = 28.0;
constexpr qreal zoomStep = 1.2;
constexpr int scrollAnimationMs = 220;

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
    // Anchoring is done by hand in applyZoom, which also covers zoom triggered
    // from the ribbon or the status bar, not just the wheel.
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
    // Zoom moves both the transform and the scroll offsets in one step, which
    // smart updates cannot reason about and leave torn. Only a few large items
    // are ever visible, so repainting the whole viewport is cheap enough.
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    setDragMode(QGraphicsView::NoDrag);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    verticalScrollBar()->setSingleStep(28);

    m_scrollAnimation = new QPropertyAnimation(verticalScrollBar(), "value", this);
    m_scrollAnimation->setDuration(scrollAnimationMs);
    m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_session, &core::DocumentSession::documentChanged, this, &PageView::rebuildScene);
    connect(m_session, &core::DocumentSession::rotationChanged, this, [this] {
        for (PageItem *item : m_items)
            item->refreshGeometry();
        layoutPages();
        updateFitZoom();
    });
    connect(m_session, &core::DocumentSession::layoutChanged, this, [this] {
        layoutPages();
        updateFitZoom();
        goToPage(m_session->currentPage());
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
        // Repaint the old and new page so the accent ring follows the reader.
        if (PageItem *previous = itemForPage(m_indicatedPage))
            previous->update();
        if (PageItem *current = itemForPage(pageIndex))
            current->update();
        m_indicatedPage = pageIndex;

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
    connect(&Theme::instance(), &Theme::changed, this, [this] {
        resetCachedContent();
        viewport()->update();
    });

    applyTool();

    // The session may already hold a document: documentChanged fired before this
    // view existed, so build the scene from what is there now.
    if (m_session->isLoaded())
        rebuildScene();
}

void PageView::rebuildScene()
{
    m_scene->clear();
    m_items.clear();
    m_indicatedPage = m_session->currentPage();

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

    m_scene->setSceneRect(QRectF(0, 0, rowWidth + pageMargin * 2, y - pageGap + pageMargin));
}

void PageView::applyZoom()
{
    const QPoint viewPoint = m_zoomAnchorValid ? m_zoomAnchorViewport
                                               : viewport()->rect().center();
    const QPointF scenePoint = m_zoomAnchorValid ? m_zoomAnchorScene : mapToScene(viewPoint);

    const qreal zoom = m_session->zoom();
    setTransform(QTransform::fromScale(zoom, zoom));

    const QPoint after = mapFromScene(scenePoint);
    const QPoint delta = after - viewPoint;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() + delta.y());

    m_zoomAnchorValid = false;
    viewport()->update();
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

    const bool twoUp = m_session->layout() == core::PageLayout::TwoPage
        || m_session->layout() == core::PageLayout::TwoPageContinuous;
    const qreal contentWidth = twoUp ? page.width() * 2 + pageGap : page.width();

    const qreal availableWidth = viewport()->width() - pageMargin * 2;
    const qreal availableHeight = viewport()->height() - pageMargin * 2;
    if (availableWidth <= 0 || availableHeight <= 0)
        return;

    const qreal widthScale = availableWidth / contentWidth;
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

    // The page covering most of the viewport is the one being read.
    const QRectF visible = mapToScene(viewport()->rect()).boundingRect();
    int best = m_session->currentPage();
    qreal bestArea = 0.0;
    for (const PageItem *item : m_items) {
        const QRectF overlap = item->pageRect().translated(item->pos()).intersected(visible);
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

void PageView::animateScrollTo(int value)
{
    QScrollBar *bar = verticalScrollBar();
    const int target = qBound(bar->minimum(), value, bar->maximum());
    if (target == bar->value())
        return;

    m_scrollAnimation->stop();
    m_scrollAnimation->setStartValue(bar->value());
    m_scrollAnimation->setEndValue(target);
    m_scrollAnimation->start();
}

void PageView::goToPage(int pageIndex)
{
    const PageItem *item = itemForPage(pageIndex);
    if (!item)
        return;

    const QPoint viewTarget = mapFromScene(item->pos() - QPointF(0, pageGap));
    animateScrollTo(verticalScrollBar()->value() + viewTarget.y());
}

void PageView::zoomIn()
{
    zoomTo(m_session->zoom() * zoomStep);
}

void PageView::zoomOut()
{
    zoomTo(m_session->zoom() / zoomStep);
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
            m_zoomAnchorViewport = event->position().toPoint();
            m_zoomAnchorScene = mapToScene(m_zoomAnchorViewport);
            m_zoomAnchorValid = true;
            zoomTo(m_session->zoom() * (steps > 0 ? zoomStep : 1.0 / zoomStep));
        }
        event->accept();
        return;
    }

    // A queued scroll animation would fight the wheel; let the wheel win.
    m_scrollAnimation->stop();
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
        // Replayed as a left press so the built-in hand drag takes over.
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
