#include "ui/viewer/PageView.h"

#include "core/pdf/PdfDocument.h"
#include "ui/theme/Theme.h"
#include "ui/viewer/PageItem.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QEvent>
#include <QFrame>
#include <QMessageBox>
#include <QGraphicsScene>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

namespace sweet::ui {
namespace {

constexpr qreal pageGap = 20.0; ///< Vertical space between pages, in points.
constexpr qreal pageMargin = 28.0;
constexpr qreal zoomStep = 1.2;
constexpr int scrollAnimationMs = 220;

/// Which markup type a tool produces, or Other for tools that are not markup.
core::AnnotationType markupTypeFor(core::Tool tool)
{
    switch (tool) {
    case core::Tool::Highlight:
        return core::AnnotationType::Highlight;
    case core::Tool::Underline:
        return core::AnnotationType::Underline;
    case core::Tool::Strikeout:
        return core::AnnotationType::StrikeOut;
    case core::Tool::Squiggly:
        return core::AnnotationType::Squiggly;
    default:
        return core::AnnotationType::Other;
    }
}

bool isRectShapeTool(core::Tool tool)
{
    return tool == core::Tool::Shape || tool == core::Tool::Ellipse || tool == core::Tool::Redact;
}

bool isLineTool(core::Tool tool)
{
    return tool == core::Tool::Line || tool == core::Tool::Arrow;
}

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
    connect(m_session, &core::DocumentSession::annotationsChanged, this, [this](int pageIndex) {
        // The annotation the overlay was framing may be gone, and its box may
        // have moved, so re-read it from the document.
        if (m_selectedAnnotation != 0
            && (pageIndex < 0 || pageIndex == m_selectedAnnotationPage)) {
            const int page = m_selectedAnnotationPage;
            const int object = m_selectedAnnotation;
            m_selectedAnnotation = 0;
            m_selectedAnnotationPage = -1;
            revealAnnotation(page, object);
        }
        refreshOverlay(pageIndex);
    });
    connect(m_session, &core::DocumentSession::markupStyleChanged, this,
            [this] { refreshOverlay(m_drawPage); });
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

    // Selection and drawing refer to pages that are about to be replaced.
    m_selectionPage = -1;
    m_selection = {};
    m_selectingText = false;
    m_drawing = false;
    m_drawPage = -1;
    m_strokes.clear();
    m_currentStroke.clear();
    m_pendingShape = QRectF();
    m_lineEnd = QPointF();
    m_selectedAnnotation = 0;
    m_selectedAnnotationPage = -1;
    m_matches.clear();
    m_activeMatchPage = -1;
    Q_EMIT selectionChanged(false);

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
    // Anything half-drawn belongs to the tool that is being left behind.
    if (m_drawing || !m_strokes.isEmpty() || !m_pendingShape.isNull()) {
        const int page = m_drawPage;
        m_drawing = false;
        m_drawPage = -1;
        m_strokes.clear();
        m_currentStroke.clear();
        m_pendingShape = QRectF();
        m_lineEnd = QPointF();
        refreshOverlay(page);
    }
    m_selectingText = false;

    // The rubber band is never used: dragging on a page selects text, and the
    // hand tool or the middle button pans.
    setDragMode(QGraphicsView::NoDrag);

    switch (m_session->tool()) {
    case core::Tool::Hand:
        setDragMode(QGraphicsView::ScrollHandDrag);
        viewport()->setCursor(Qt::OpenHandCursor);
        break;
    case core::Tool::Select:
    case core::Tool::Highlight:
    case core::Tool::Underline:
    case core::Tool::Strikeout:
    case core::Tool::Squiggly:
    case core::Tool::EditText:
        viewport()->setCursor(Qt::IBeamCursor);
        break;
    case core::Tool::Draw:
    case core::Tool::Shape:
    case core::Tool::Ellipse:
    case core::Tool::Line:
    case core::Tool::Arrow:
    case core::Tool::Redact:
        viewport()->setCursor(Qt::CrossCursor);
        break;
    case core::Tool::Note:
    case core::Tool::AddText:
    case core::Tool::Stamp:
        viewport()->setCursor(Qt::PointingHandCursor);
        break;
    default:
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

// --- Page hit testing ------------------------------------------------------

PageItem *PageView::pageAt(const QPoint &viewportPos, QPointF *pagePoint) const
{
    const QPointF scenePoint = mapToScene(viewportPos);
    for (PageItem *item : m_items) {
        const QPointF itemPoint = item->mapFromScene(scenePoint);
        if (!item->pageRect().contains(itemPoint))
            continue;
        if (pagePoint)
            *pagePoint = item->toPagePoint(itemPoint);
        return item;
    }
    return nullptr;
}

std::shared_ptr<const core::PageText> PageView::textOf(int pageIndex) const
{
    const auto &document = m_session->document();
    if (!document)
        return nullptr;
    if (auto page = document->page(pageIndex))
        return page->textLayer();
    return nullptr;
}

// --- Overlays --------------------------------------------------------------

void PageView::refreshOverlay(int pageIndex)
{
    if (pageIndex < 0) {
        for (const PageItem *item : m_items)
            refreshOverlay(item->pageIndex());
        return;
    }

    PageItem *item = itemForPage(pageIndex);
    if (!item)
        return;

    PageOverlay overlay;
    overlay.toolColor = m_session->markupColor();

    if (pageIndex == m_selectionPage && !m_selection.isEmpty()) {
        if (auto text = textOf(pageIndex))
            overlay.selection = text->boxes(m_selection);
    }

    if (const auto hits = m_matches.constFind(pageIndex); hits != m_matches.cend()) {
        if (auto text = textOf(pageIndex)) {
            for (const core::TextRange &range : *hits) {
                const bool isActive = pageIndex == m_activeMatchPage
                    && range.from == m_activeMatch.from && range.to == m_activeMatch.to;
                const QList<QRectF> boxes = text->boxes(range);
                if (isActive && !boxes.isEmpty())
                    overlay.activeMatch = boxes.first();
                else
                    overlay.matches.append(boxes);
            }
        }
    }

    if (pageIndex == m_drawPage) {
        overlay.strokes = m_strokes;
        if (!m_currentStroke.isEmpty())
            overlay.strokes.append(m_currentStroke);
        overlay.toolWidth = 2.0;

        if (!m_pendingShape.isNull()
            || (m_drawing && isLineTool(m_session->tool()))) {
            overlay.pendingShape = m_pendingShape;
            switch (m_placement != Placement::None ? core::Tool::Shape : m_session->tool()) {
            case core::Tool::Ellipse:
                overlay.pendingShapeKind = PageOverlay::PendingShape::Ellipse;
                break;
            case core::Tool::Redact:
                overlay.pendingShapeKind = PageOverlay::PendingShape::Redaction;
                break;
            case core::Tool::Line:
            case core::Tool::Arrow:
                overlay.pendingShapeKind = PageOverlay::PendingShape::Line;
                overlay.pendingLineStart = m_shapeOrigin;
                overlay.pendingLineEnd = m_lineEnd;
                overlay.toolWidth = m_session->markupBorderWidth();
                break;
            default:
                overlay.pendingShapeKind = PageOverlay::PendingShape::Rectangle;
                break;
            }
            if (isRectShapeTool(m_session->tool())
                || (m_placement == Placement::None && isLineTool(m_session->tool())))
                overlay.toolWidth = m_session->markupBorderWidth();
        }
    }

    if (pageIndex == m_selectedAnnotationPage && m_selectedAnnotation != 0)
        overlay.selectedAnnotation = m_selectedAnnotationRect;

    item->setOverlay(overlay);
}

// --- Text selection --------------------------------------------------------

bool PageView::hasSelection() const
{
    return m_selectionPage >= 0 && !m_selection.isEmpty();
}

QString PageView::selectedText() const
{
    if (!hasSelection())
        return {};
    if (auto text = textOf(m_selectionPage))
        return text->textIn(m_selection);
    return {};
}

void PageView::setSelection(int pageIndex, const core::TextRange &range)
{
    const bool had = hasSelection();
    const int previousPage = m_selectionPage;

    m_selectionPage = pageIndex;
    m_selection = range;

    if (previousPage >= 0 && previousPage != pageIndex)
        refreshOverlay(previousPage);
    refreshOverlay(pageIndex);

    if (had != hasSelection())
        Q_EMIT selectionChanged(hasSelection());
}

void PageView::clearSelection()
{
    if (!hasSelection())
        return;
    setSelection(-1, {});
}

void PageView::copySelection()
{
    const QString text = selectedText();
    if (text.isEmpty())
        return;

    const auto &document = m_session->document();
    if (document && !document->permissions().copy) {
        Q_EMIT statusMessage(tr("This document does not allow copying text."));
        return;
    }

    QGuiApplication::clipboard()->setText(text);
    Q_EMIT statusMessage(tr("%n character(s) copied.", nullptr, int(text.size())));
}

void PageView::selectAllOnCurrentPage()
{
    const int pageIndex = m_session->currentPage();
    if (auto text = textOf(pageIndex); text && !text->isEmpty())
        setSelection(pageIndex, { 0, text->length() });
}

bool PageView::markUpSelection(core::AnnotationType type)
{
    if (!hasSelection())
        return false;

    auto text = textOf(m_selectionPage);
    if (!text)
        return false;

    core::AnnotationSpec spec;
    spec.type = type;
    spec.pageIndex = m_selectionPage;
    spec.quads = text->boxes(m_selection);
    spec.color = m_session->markupColor();
    // Underlines and strikeouts are lines, not washes, so full opacity reads
    // better than the translucency a highlight wants.
    spec.opacity = type == core::AnnotationType::Highlight ? m_session->markupOpacity() : 1.0;
    spec.author = m_session->author();
    if (spec.quads.isEmpty())
        return false;

    if (type == core::AnnotationType::Redact) {
        spec.color = Qt::black;
        spec.opacity = 1.0;
    }

    const bool added = m_session->addAnnotation(spec);
    if (added)
        clearSelection();
    return added;
}

// --- Search ----------------------------------------------------------------

void PageView::showMatches(const QHash<int, QList<core::TextRange>> &matches)
{
    m_matches = matches;
    refreshOverlay();
}

void PageView::clearMatches()
{
    if (m_matches.isEmpty() && m_activeMatchPage < 0)
        return;
    m_matches.clear();
    m_activeMatchPage = -1;
    m_activeMatch = {};
    refreshOverlay();
}

void PageView::focusMatch(int pageIndex, const core::TextRange &range)
{
    const int previous = m_activeMatchPage;
    m_activeMatchPage = pageIndex;
    m_activeMatch = range;

    if (previous >= 0 && previous != pageIndex)
        refreshOverlay(previous);
    refreshOverlay(pageIndex);

    if (auto text = textOf(pageIndex)) {
        const QList<QRectF> boxes = text->boxes(range);
        if (!boxes.isEmpty())
            scrollRectIntoView(pageIndex, boxes.first());
    }
}

void PageView::scrollRectIntoView(int pageIndex, const QRectF &pageRect)
{
    PageItem *item = itemForPage(pageIndex);
    if (!item)
        return;

    const QRectF inItem = item->pageToItem().mapRect(pageRect);
    const QRectF inScene = item->mapRectToScene(inItem);
    const QRect inViewport = mapFromScene(inScene).boundingRect();
    const QRect visible = viewport()->rect();

    // Leave a comfortable margin so a hit near the edge is not glued to it.
    constexpr int margin = 80;
    if (inViewport.top() >= margin && inViewport.bottom() <= visible.height() - margin)
        return;

    const int wanted = inViewport.center().y() - visible.height() / 2;
    animateScrollTo(verticalScrollBar()->value() + wanted);

    if (inViewport.left() < 0 || inViewport.right() > visible.width()) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + inViewport.center().x()
                                       - visible.width() / 2);
    }
}

// --- Annotations -----------------------------------------------------------

core::Annotation PageView::annotationAt(int pageIndex, const QPointF &pagePoint) const
{
    const QList<core::Annotation> found = m_session->annotationsOnPage(pageIndex);
    // Later annotations are painted on top, so search backwards.
    for (auto it = found.crbegin(); it != found.crend(); ++it) {
        if (!it->editable)
            continue;
        if (it->rect.adjusted(-2, -2, 2, 2).contains(pagePoint))
            return *it;
    }
    return {};
}

void PageView::revealAnnotation(int pageIndex, int objectNumber)
{
    const QList<core::Annotation> found = m_session->annotationsOnPage(pageIndex);
    for (const core::Annotation &annotation : found) {
        if (annotation.objectNumber != objectNumber)
            continue;

        const int previous = m_selectedAnnotationPage;
        m_selectedAnnotationPage = pageIndex;
        m_selectedAnnotation = objectNumber;
        m_selectedAnnotationRect = annotation.rect;
        if (previous >= 0 && previous != pageIndex)
            refreshOverlay(previous);
        refreshOverlay(pageIndex);

        m_session->setSelectedAnnotation(pageIndex, objectNumber);
        scrollRectIntoView(pageIndex, annotation.rect);
        return;
    }
}

bool PageView::deleteSelectedAnnotation()
{
    if (m_selectedAnnotation == 0)
        return false;

    const QList<core::Annotation> found = m_session->annotationsOnPage(m_selectedAnnotationPage);
    for (const core::Annotation &annotation : found) {
        if (annotation.objectNumber != m_selectedAnnotation)
            continue;

        const int page = m_selectedAnnotationPage;
        m_selectedAnnotation = 0;
        m_selectedAnnotationPage = -1;
        m_selectedAnnotationRect = QRectF();
        m_session->setSelectedAnnotation(-1, 0);
        const bool removed = m_session->removeAnnotation(annotation);
        refreshOverlay(page);
        return removed;
    }
    return false;
}

void PageView::editSelectedAnnotation()
{
    if (m_selectedAnnotation == 0)
        return;

    const QList<core::Annotation> found = m_session->annotationsOnPage(m_selectedAnnotationPage);
    for (const core::Annotation &annotation : found) {
        if (annotation.objectNumber != m_selectedAnnotation)
            continue;

        bool accepted = false;
        const QString text = QInputDialog::getMultiLineText(
            this, tr("Edit note"), tr("Note text:"), annotation.contents, &accepted);
        if (accepted)
            m_session->modifyAnnotation(annotation, annotation.color, annotation.opacity, text);
        return;
    }
}

void PageView::createNoteAt(int pageIndex, const QPointF &pagePoint)
{
    bool accepted = false;
    const QString text = QInputDialog::getMultiLineText(this, tr("New note"), tr("Note text:"),
                                                        QString(), &accepted);
    if (!accepted)
        return;

    core::AnnotationSpec spec;
    spec.type = core::AnnotationType::Note;
    spec.pageIndex = pageIndex;
    spec.rect = QRectF(pagePoint, QSizeF(20, 20));
    spec.color = m_session->markupColor();
    spec.contents = text;
    spec.author = m_session->author();
    m_session->addAnnotation(spec);
}

void PageView::createTextBoxAt(int pageIndex, const QPointF &pagePoint)
{
    bool accepted = false;
    const QString text = QInputDialog::getMultiLineText(this, tr("Add text"), tr("Text:"),
                                                        QString(), &accepted);
    if (!accepted || text.isEmpty())
        return;

    // Sized from the text so a short label does not get a huge box.
    const int longestLine = std::max_element(
                                text.split(QLatin1Char('\n')).cbegin(),
                                text.split(QLatin1Char('\n')).cend(),
                                [](const QString &a, const QString &b) {
                                    return a.size() < b.size();
                                })
                                ->size();
    const qreal width = qBound(80.0, longestLine * 6.2, 420.0);
    const qreal height = qMax(24.0, text.count(QLatin1Char('\n')) * 14.0 + 22.0);

    core::AnnotationSpec spec;
    spec.type = core::AnnotationType::FreeText;
    spec.pageIndex = pageIndex;
    spec.rect = QRectF(pagePoint, QSizeF(width, height));
    spec.color = QColor(0x14, 0x18, 0x25);
    spec.contents = text;
    spec.author = m_session->author();
    spec.fontSize = 11.0;
    m_session->addAnnotation(spec);
}

void PageView::createStampAt(int pageIndex, const QPointF &pagePoint)
{
    constexpr qreal width = 140.0;
    constexpr qreal height = 40.0;
    core::AnnotationSpec spec;
    spec.type = core::AnnotationType::Stamp;
    spec.pageIndex = pageIndex;
    spec.rect = QRectF(pagePoint.x() - width / 2.0, pagePoint.y() - height / 2.0, width, height);
    spec.iconName = m_session->stampIconName();
    spec.color = m_session->markupColor();
    spec.author = m_session->author();
    m_session->addAnnotation(spec);
}

void PageView::commitPendingAnnotation()
{
    const core::Tool tool = m_session->tool();

    if (tool == core::Tool::Draw && !m_strokes.isEmpty()) {
        core::AnnotationSpec spec;
        spec.type = core::AnnotationType::Ink;
        spec.pageIndex = m_drawPage;
        spec.strokes = m_strokes;
        spec.color = m_session->markupColor();
        spec.borderWidth = 2.0;
        spec.author = m_session->author();
        m_session->addAnnotation(spec);
    } else if (isLineTool(tool)) {
        const QPointF delta = m_lineEnd - m_shapeOrigin;
        if (QPointF::dotProduct(delta, delta) > 4.0) {
            core::AnnotationSpec spec;
            spec.type = core::AnnotationType::Line;
            spec.pageIndex = m_drawPage;
            spec.lineStart = m_shapeOrigin;
            spec.lineEnd = m_lineEnd;
            spec.hasLineEndpoints = true;
            spec.lineStartStyle = core::LineEnding::None;
            spec.lineEndStyle =
                tool == core::Tool::Arrow ? m_session->lineEndStyle() : core::LineEnding::None;
            spec.color = m_session->markupColor();
            spec.opacity = m_session->markupOpacity();
            spec.borderWidth = m_session->markupBorderWidth();
            spec.author = m_session->author();
            m_session->addAnnotation(spec);
        }
    } else if (isRectShapeTool(tool) && m_pendingShape.width() > 2 && m_pendingShape.height() > 2) {
        core::AnnotationSpec spec;
        spec.pageIndex = m_drawPage;
        spec.rect = m_pendingShape;
        spec.author = m_session->author();
        spec.opacity = m_session->markupOpacity();
        spec.borderWidth = m_session->markupBorderWidth();
        switch (tool) {
        case core::Tool::Ellipse:
            spec.type = core::AnnotationType::Circle;
            spec.color = m_session->markupColor();
            break;
        case core::Tool::Redact:
            spec.type = core::AnnotationType::Redact;
            spec.color = Qt::black;
            spec.opacity = 1.0;
            break;
        default:
            spec.type = core::AnnotationType::Square;
            spec.color = m_session->markupColor();
            break;
        }
        m_session->addAnnotation(spec);
    }

    const int page = m_drawPage;
    m_strokes.clear();
    m_currentStroke.clear();
    m_pendingShape = QRectF();
    m_lineEnd = QPointF();
    m_drawing = false;
    m_drawPage = -1;
    refreshOverlay(page);
}

bool PageView::followLinkAt(int pageIndex, const QPointF &pagePoint)
{
    if (!m_session->isLoaded())
        return false;

    const QList<core::content::Link> links =
        core::content::linksOnPage(*m_session->document(), pageIndex);
    for (const core::content::Link &link : links) {
        if (!link.rect.contains(pagePoint))
            continue;

        const int target = core::content::pageForUri(*m_session->document(), link.uri);
        if (target >= 0) {
            m_session->setCurrentPage(target);
            goToPage(target);
            Q_EMIT statusMessage(tr("Jumped to page %1.").arg(target + 1));
            return true;
        }

        // An address that leads outside the document is handed to the desktop,
        // and only after the reader agrees: a PDF should not be able to open
        // anything it likes behind your back.
        const QUrl url(link.uri);
        if (!url.isValid() || url.scheme().isEmpty())
            return false;

        const auto choice = QMessageBox::question(
            this, tr("Open link"),
            tr("This link leads outside the document:\n\n%1\n\nOpen it in your browser?")
                .arg(link.uri),
            QMessageBox::Cancel | QMessageBox::Open, QMessageBox::Open);
        if (choice == QMessageBox::Open)
            QDesktopServices::openUrl(url);
        return true;
    }
    return false;
}

QList<QRectF> PageView::selectionBoxes() const
{
    if (!hasSelection())
        return {};
    if (auto text = textOf(m_selectionPage))
        return text->boxes(m_selection);
    return {};
}

QRect PageView::selectionViewportRect() const
{
    const QList<QRectF> boxes = selectionBoxes();
    PageItem *item = itemForPage(m_selectionPage);
    if (!item || boxes.isEmpty())
        return {};

    QRectF area = boxes.first();
    for (const QRectF &box : boxes)
        area = area.united(box);

    const QRectF inItem = item->pageToItem().mapRect(area);
    const QRectF inScene = item->mapRectToScene(inItem);
    return mapFromScene(inScene).boundingRect().adjusted(-4, -3, 8, 4);
}

void PageView::startInlineTextEdit()
{
    // A second edit request must keep the previous replacement, not throw it away.
    if (m_inlineEdit)
        commitInlineTextEdit();
    if (!hasSelection())
        return;

    const QRect box = selectionViewportRect();
    if (box.isEmpty())
        return;

    m_inlineEdit = new QPlainTextEdit(viewport());
    m_inlineEdit->setObjectName(QStringLiteral("InlineTextEdit"));
    m_inlineEdit->setPlainText(selectedText());
    m_inlineEdit->selectAll();
    m_inlineEdit->setTabChangesFocus(true);
    m_inlineEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_inlineEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_inlineEdit->setFrameShape(QFrame::NoFrame);
    m_inlineEdit->setAttribute(Qt::WA_StyledBackground, true);
    m_inlineEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit#InlineTextEdit {"
        "  background: #FFFBE6; color: #141825; border: 2px solid #2F86FF;"
        "  border-radius: 4px; padding: 2px 4px;"
        "}"));

    QFont font = m_inlineEdit->font();
    font.setPointSizeF(qBound(9.0, box.height() * 0.55, 22.0));
    m_inlineEdit->setFont(font);

    const int height = qMax(box.height() + 6, int(font.pointSizeF() * 2.2) + 10);
    m_inlineEdit->setGeometry(box.x(), box.y(), qMax(box.width(), 120), height);
    m_inlineEdit->show();
    m_inlineEdit->setFocus(Qt::MouseFocusReason);

    connect(m_inlineEdit, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_inlineEdit)
            return;
        const QSize hint = m_inlineEdit->document()->size().toSize();
        m_inlineEdit->resize(qMax(m_inlineEdit->width(), hint.width() + 16),
                             qMax(m_inlineEdit->height(), hint.height() + 10));
    });

    m_inlineEdit->installEventFilter(this);
}

void PageView::commitInlineTextEdit()
{
    if (!m_inlineEdit)
        return;

    const QString replacement = m_inlineEdit->toPlainText();
    const int pageIndex = m_selectionPage;
    const QList<QRectF> boxes = selectionBoxes();
    // Detach first so a nested focus change cannot re-enter cancel/commit.
    QPlainTextEdit *editor = m_inlineEdit;
    m_inlineEdit = nullptr;
    editor->deleteLater();

    if (pageIndex < 0 || boxes.isEmpty())
        return;
    Q_EMIT textReplaceRequested(replacement, pageIndex, boxes);
}

void PageView::cancelInlineTextEdit()
{
    if (!m_inlineEdit)
        return;
    QPlainTextEdit *editor = m_inlineEdit;
    m_inlineEdit = nullptr;
    editor->deleteLater();
}

bool PageView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_inlineEdit) {
        if (event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) {
                cancelInlineTextEdit();
                return true;
            }
            if (key->key() == Qt::Key_Return
                && !(key->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
                commitInlineTextEdit();
                return true;
            }
        }
        if (event->type() == QEvent::FocusOut) {
            // Defer so a click that commits elsewhere does not fight this.
            QTimer::singleShot(0, this, [this] {
                if (m_inlineEdit && !m_inlineEdit->hasFocus())
                    commitInlineTextEdit();
            });
        }
    }
    return QGraphicsView::eventFilter(watched, event);
}

void PageView::beginImagePlacement(const QImage &image, const QString &commandText)
{
    if (image.isNull())
        return;

    m_placement = Placement::Image;
    m_placementImage = image;
    m_placementText = commandText;
    clearSelection();
    viewport()->setCursor(Qt::CrossCursor);
    Q_EMIT statusMessage(tr("Drag a box where the picture should go, or click once to place it "
                            "at its own size."));
}

void PageView::beginLinkPlacement(const QString &uri)
{
    if (uri.isEmpty())
        return;

    m_placement = Placement::Link;
    m_placementUri = uri;
    viewport()->setCursor(Qt::CrossCursor);
    Q_EMIT statusMessage(tr("Drag a box over the area the link should cover."));
}

void PageView::cancelPlacement()
{
    if (m_placement == Placement::None)
        return;

    m_placement = Placement::None;
    m_placementImage = QImage();
    m_placementUri.clear();
    m_placementText.clear();

    const int page = m_drawPage;
    m_drawing = false;
    m_drawPage = -1;
    m_pendingShape = QRectF();
    refreshOverlay(page);
    applyTool();
}

void PageView::commitPlacement()
{
    const Placement kind = m_placement;
    const int pageIndex = m_drawPage;
    const QRectF box = m_pendingShape.normalized();
    const QImage image = m_placementImage;
    const QString uri = m_placementUri;
    const QString commandText = m_placementText;

    cancelPlacement();
    if (kind == Placement::None || pageIndex < 0)
        return;

    if (kind == Placement::Image) {
        QRectF target = box;
        if (target.width() < 4 || target.height() < 4) {
            // A click rather than a drag: place at the picture's own size, taking
            // its pixels as 96 dpi, which is what screen captures and scans use.
            const QSizeF natural(image.width() * 72.0 / 96.0, image.height() * 72.0 / 96.0);
            target = QRectF(box.topLeft(), natural);
        } else {
            target = core::content::fitToAspect(QSizeF(image.size()), target);
        }

        if (m_session->addImage(pageIndex, image, target, commandText))
            Q_EMIT statusMessage(tr("Placed on page %1.").arg(pageIndex + 1));
        else
            Q_EMIT statusMessage(tr("The picture could not be placed."));
        return;
    }

    if (box.width() < 4 || box.height() < 4) {
        Q_EMIT statusMessage(tr("A link needs a box: drag over the area it should cover."));
        return;
    }
    if (m_session->addLink(pageIndex, box, uri))
        Q_EMIT statusMessage(tr("Link added on page %1.").arg(pageIndex + 1));
    else
        Q_EMIT statusMessage(tr("The link could not be added."));
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

    if (event->button() == Qt::LeftButton) {
        QPointF pagePoint;
        PageItem *item = pageAt(event->position().toPoint(), &pagePoint);
        const core::Tool tool = m_session->tool();

        if (item) {
            const int pageIndex = item->pageIndex();

            // An armed picture or link takes the drag before any tool does.
            if (m_placement != Placement::None) {
                m_drawPage = pageIndex;
                m_drawing = true;
                m_shapeOrigin = pagePoint;
                m_pendingShape = QRectF(pagePoint, pagePoint);
                refreshOverlay(pageIndex);
                event->accept();
                return;
            }

            // Following a link comes before everything else, the way it does in a
            // reader: that is what a pointer over a link is for.
            if (tool == core::Tool::Select || tool == core::Tool::Hand) {
                if (followLinkAt(pageIndex, pagePoint)) {
                    event->accept();
                    return;
                }
            }

            // Picking an existing annotation always wins over starting something
            // new, otherwise a note could never be selected again.
            if (tool == core::Tool::Select) {
                const core::Annotation hit = annotationAt(pageIndex, pagePoint);
                if (hit.objectNumber != 0) {
                    clearSelection();
                    revealAnnotation(pageIndex, hit.objectNumber);
                    Q_EMIT annotationSelected(pageIndex, hit.objectNumber);
                    event->accept();
                    return;
                }
                if (m_selectedAnnotation != 0) {
                    const int previous = m_selectedAnnotationPage;
                    m_selectedAnnotation = 0;
                    m_selectedAnnotationPage = -1;
                    m_session->setSelectedAnnotation(-1, 0);
                    refreshOverlay(previous);
                }
            }

            switch (tool) {
            case core::Tool::EditText: {
                // Commit first: cancel-on-press was throwing away typed text when
                // the user clicked away to finish editing.
                if (m_inlineEdit) {
                    commitInlineTextEdit();
                    event->accept();
                    return;
                }
                if (auto text = textOf(pageIndex)) {
                    bool hitText = false;
                    m_selectionAnchor = text->indexAt(pagePoint, &hitText);
                    m_selectingText = true;
                    m_textPressPos = event->position().toPoint();
                    setSelection(pageIndex, { m_selectionAnchor, m_selectionAnchor });
                }
                event->accept();
                return;
            }
            case core::Tool::Select:
            case core::Tool::Highlight:
            case core::Tool::Underline:
            case core::Tool::Strikeout:
            case core::Tool::Squiggly: {
                if (auto text = textOf(pageIndex)) {
                    m_selectionAnchor = text->indexAt(pagePoint);
                    m_selectingText = true;
                    setSelection(pageIndex, { m_selectionAnchor, m_selectionAnchor });
                }
                event->accept();
                return;
            }
            case core::Tool::Draw:
                m_drawPage = pageIndex;
                m_drawing = true;
                m_currentStroke = QPolygonF({ pagePoint });
                refreshOverlay(pageIndex);
                event->accept();
                return;
            case core::Tool::Shape:
            case core::Tool::Ellipse:
            case core::Tool::Redact:
                m_drawPage = pageIndex;
                m_drawing = true;
                m_shapeOrigin = pagePoint;
                m_pendingShape = QRectF(pagePoint, pagePoint);
                refreshOverlay(pageIndex);
                event->accept();
                return;
            case core::Tool::Line:
            case core::Tool::Arrow:
                m_drawPage = pageIndex;
                m_drawing = true;
                m_shapeOrigin = pagePoint;
                m_lineEnd = pagePoint;
                m_pendingShape = QRectF();
                refreshOverlay(pageIndex);
                event->accept();
                return;
            case core::Tool::Note:
                createNoteAt(pageIndex, pagePoint);
                m_session->setTool(core::Tool::Select);
                event->accept();
                return;
            case core::Tool::Stamp:
                createStampAt(pageIndex, pagePoint);
                m_session->setTool(core::Tool::Select);
                event->accept();
                return;
            case core::Tool::AddText:
                createTextBoxAt(pageIndex, pagePoint);
                m_session->setTool(core::Tool::Select);
                event->accept();
                return;
            default:
                break;
            }
        } else if (tool == core::Tool::Select) {
            clearSelection();
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void PageView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selectingText && (event->buttons() & Qt::LeftButton)) {
        QPointF pagePoint;
        // Clamped to the page the selection started on: dragging past the edge
        // should extend to the end of that page, not jump to the next one.
        PageItem *item = pageAt(event->position().toPoint(), &pagePoint);
        if (!item || item->pageIndex() != m_selectionPage) {
            if (PageItem *anchorItem = itemForPage(m_selectionPage))
                pagePoint = anchorItem->toPagePoint(anchorItem->mapFromScene(
                    mapToScene(event->position().toPoint())));
        }

        if (auto text = textOf(m_selectionPage)) {
            const int index = text->indexAt(pagePoint);
            setSelection(m_selectionPage, { qMin(m_selectionAnchor, index),
                                            qMax(m_selectionAnchor, index) });
        }
        event->accept();
        return;
    }

    if (m_drawing && (event->buttons() & Qt::LeftButton)) {
        QPointF pagePoint;
        PageItem *item = pageAt(event->position().toPoint(), &pagePoint);
        if (item && item->pageIndex() == m_drawPage) {
            if (m_placement != Placement::None)
                m_pendingShape = QRectF(m_shapeOrigin, pagePoint).normalized();
            else if (m_session->tool() == core::Tool::Draw)
                m_currentStroke.append(pagePoint);
            else if (isLineTool(m_session->tool()))
                m_lineEnd = pagePoint;
            else
                m_pendingShape = QRectF(m_shapeOrigin, pagePoint).normalized();
            refreshOverlay(m_drawPage);
        }
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void PageView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QPointF pagePoint;
    PageItem *item = pageAt(event->position().toPoint(), &pagePoint);
    if (item && event->button() == Qt::LeftButton) {
        const core::Annotation hit = annotationAt(item->pageIndex(), pagePoint);
        if (hit.objectNumber != 0
            && (hit.type == core::AnnotationType::Note
                || hit.type == core::AnnotationType::FreeText || !hit.contents.isEmpty())) {
            revealAnnotation(item->pageIndex(), hit.objectNumber);
            editSelectedAnnotation();
            event->accept();
            return;
        }

        // Double-click selects a word, triple-click the line, as everywhere else.
        if (auto text = textOf(item->pageIndex()); text && !text->isEmpty()) {
            const int index = text->indexAt(pagePoint);
            setSelection(item->pageIndex(), text->wordAt(index));
            m_selectionAnchor = index;
            event->accept();
            return;
        }
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}

void PageView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_selectingText && event->button() == Qt::LeftButton) {
        m_selectingText = false;

        // A markup tool turns the drag straight into an annotation, so the reader
        // does not have to select and then press a button.
        const core::AnnotationType markup = markupTypeFor(m_session->tool());
        if (markup != core::AnnotationType::Other && hasSelection()) {
            markUpSelection(markup);
        } else if (m_session->tool() == core::Tool::EditText) {
            // A click (no real drag) takes the whole line under the pointer -
            // that is what people mean by "click this text to edit it", especially
            // on forms and invoices. A drag keeps the exact range they painted.
            const QPoint delta = event->position().toPoint() - m_textPressPos;
            const bool wasClick = delta.manhattanLength() < 5;
            if (wasClick) {
                QPointF pagePoint;
                PageItem *item = pageAt(event->position().toPoint(), &pagePoint);
                if (item) {
                    if (auto text = textOf(item->pageIndex())) {
                        bool hitText = false;
                        const int index = text->indexAt(pagePoint, &hitText);
                        if (hitText)
                            setSelection(item->pageIndex(), text->lineAt(index));
                        else
                            clearSelection();
                    }
                }
            }
            if (hasSelection()) {
                Q_EMIT selectionChanged(true);
                Q_EMIT textEditRequested();
                startInlineTextEdit();
            } else {
                Q_EMIT statusMessage(tr("Click a line of text to edit it."));
            }
        } else if (hasSelection()) {
            Q_EMIT selectionChanged(true);
        }

        event->accept();
        return;
    }

    if (m_drawing && event->button() == Qt::LeftButton) {
        if (m_placement != Placement::None) {
            commitPlacement();
            event->accept();
            return;
        }
        if (m_session->tool() == core::Tool::Draw && !m_currentStroke.isEmpty()) {
            m_strokes.append(m_currentStroke);
            m_currentStroke.clear();
            // Ink stays open for more strokes until the pointer leaves the page or
            // the tool changes, which is how a pen is expected to behave. One
            // stroke per release is committed immediately to keep undo granular.
            commitPendingAnnotation();
            event->accept();
            return;
        }
        commitPendingAnnotation();
        event->accept();
        return;
    }

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

void PageView::contextMenuEvent(QContextMenuEvent *event)
{
    QPointF pagePoint;
    PageItem *item = pageAt(event->pos(), &pagePoint);
    if (!item) {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    const int pageIndex = item->pageIndex();
    const core::Annotation hit = annotationAt(pageIndex, pagePoint);

    QMenu menu(this);
    if (hit.objectNumber != 0) {
        revealAnnotation(pageIndex, hit.objectNumber);
        menu.addAction(icon(QStringLiteral("comment")), tr("Edit note…"), this,
                       &PageView::editSelectedAnnotation);
        menu.addAction(icon(QStringLiteral("trash")), tr("Delete %1")
                                                          .arg(core::annotations::displayName(
                                                                   hit.type)
                                                                   .toLower()),
                       this, [this] { deleteSelectedAnnotation(); });
        menu.addSeparator();
    }

    const QList<core::content::Link> links = m_session->isLoaded()
        ? core::content::linksOnPage(*m_session->document(), pageIndex)
        : QList<core::content::Link>();
    for (const core::content::Link &link : links) {
        if (!link.rect.contains(pagePoint))
            continue;
        menu.addAction(icon(QStringLiteral("link")), tr("Follow link"), this,
                       [this, pageIndex, pagePoint] { followLinkAt(pageIndex, pagePoint); });
        menu.addAction(icon(QStringLiteral("trash")), tr("Remove link"), this,
                       [this, pageIndex, pagePoint] {
                           QString error;
                           m_session->applyStructuralEdit(
                               tr("Remove link"),
                               [pageIndex, pagePoint](core::PdfDocument &document) {
                                   core::content::removeLinkAt(document, pageIndex, pagePoint);
                               },
                               &error);
                           if (!error.isEmpty())
                               Q_EMIT statusMessage(error);
                       });
        menu.addSeparator();
        break;
    }

    if (hasSelection()) {
        menu.addAction(icon(QStringLiteral("copy")), tr("Copy"), this, &PageView::copySelection);
        menu.addSeparator();
        menu.addAction(icon(QStringLiteral("highlight")), tr("Highlight"), this,
                       [this] { markUpSelection(core::AnnotationType::Highlight); });
        menu.addAction(icon(QStringLiteral("underline")), tr("Underline"), this,
                       [this] { markUpSelection(core::AnnotationType::Underline); });
        menu.addAction(icon(QStringLiteral("strikeout")), tr("Strikeout"), this,
                       [this] { markUpSelection(core::AnnotationType::StrikeOut); });
        menu.addAction(icon(QStringLiteral("squiggly")), tr("Squiggly"), this,
                       [this] { markUpSelection(core::AnnotationType::Squiggly); });
        menu.addAction(icon(QStringLiteral("redact")), tr("Mark for redaction"), this,
                       [this] { markUpSelection(core::AnnotationType::Redact); });
        menu.addSeparator();
    }

    menu.addAction(icon(QStringLiteral("text-select")), tr("Select all on this page"), this,
                   &PageView::selectAllOnCurrentPage);
    menu.exec(event->globalPos());
    event->accept();
}

void PageView::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy)) {
        copySelection();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        selectAllOnCurrentPage();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (isPlacing()) {
            cancelPlacement();
            Q_EMIT statusMessage(tr("Cancelled."));
            event->accept();
            return;
        }
        clearSelection();
        clearMatches();
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
        && deleteSelectedAnnotation()) {
        event->accept();
        return;
    }

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
