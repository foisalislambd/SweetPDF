#include "ui/viewer/PageItem.h"

#include "core/pdf/PdfDocument.h"
#include "core/session/DocumentSession.h"
#include "ui/theme/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

namespace sweet::ui {
namespace {

constexpr qreal shadowSpread = 9.0; ///< Shadow reach in points.
constexpr qreal cornerRadius = 2.5;
constexpr qreal maxRenderScale = 6.0;

/// Rounds the scale to a coarse ladder so panning and small zoom nudges reuse
/// cached bitmaps instead of re-rendering every page.
qreal quantiseScale(qreal scale)
{
    const qreal clamped = qBound(0.05, scale, maxRenderScale);
    return qRound(clamped * 20.0) / 20.0;
}

} // namespace

PageItem::PageItem(core::DocumentSession *session, int pageIndex)
    : m_session(session)
    , m_pageIndex(pageIndex)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setCacheMode(QGraphicsItem::NoCache);
    refreshGeometry();
}

void PageItem::refreshGeometry()
{
    prepareGeometryChange();

    const auto &document = m_session->document();
    QSizeF size = document ? document->pageSizePoints(m_pageIndex) : QSizeF(612, 792);
    const int rotation = m_session->rotation();
    if (rotation == 90 || rotation == 270)
        size.transpose();

    m_sizePoints = size;
    discardBitmap();
}

void PageItem::discardBitmap()
{
    m_bitmap = QImage();
    m_bitmapScale = 0.0;
    update();
}

QRectF PageItem::pageRect() const
{
    return QRectF(QPointF(0, 0), m_sizePoints);
}

void PageItem::setOverlay(const PageOverlay &overlay)
{
    m_overlay = overlay;
    update();
}

QTransform PageItem::pageToItem() const
{
    // m_sizePoints is the rotated size, so the unrotated page is transposed for
    // the quarter turns.
    const int rotation = ((m_session->rotation() % 360) + 360) % 360;
    const qreal width = (rotation == 90 || rotation == 270) ? m_sizePoints.height()
                                                            : m_sizePoints.width();
    const qreal height = (rotation == 90 || rotation == 270) ? m_sizePoints.width()
                                                             : m_sizePoints.height();

    switch (rotation) {
    case 90:
        return QTransform(0, 1, -1, 0, height, 0);
    case 180:
        return QTransform(-1, 0, 0, -1, width, height);
    case 270:
        return QTransform(0, -1, 1, 0, 0, width);
    default:
        return {};
    }
}

QPointF PageItem::toPagePoint(const QPointF &itemPoint) const
{
    return pageToItem().inverted().map(itemPoint);
}

QRectF PageItem::toPageRect(const QRectF &itemRect) const
{
    return pageToItem().inverted().mapRect(itemRect);
}

QRectF PageItem::boundingRect() const
{
    return pageRect().adjusted(-shadowSpread, -shadowSpread, shadowSpread, shadowSpread + 4);
}

void PageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    const QRectF page = pageRect();
    const Palette &colors = themePalette();
    const bool isCurrent = m_session->currentPage() == m_pageIndex;

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);

    // Soft shadow built from a few translucent rounded rects. Cheaper than a
    // blur effect, and only a handful of pages are ever on screen.
    for (int layer = 6; layer >= 1; --layer) {
        QColor tint = colors.shadow;
        tint.setAlpha(qMax(4, colors.shadow.alpha() / (layer * 2)));
        painter->setBrush(tint);
        const qreal grow = layer * 1.4;
        painter->drawRoundedRect(page.adjusted(-grow, -grow + 1.5, grow, grow + 2.5),
                                 cornerRadius + grow, cornerRadius + grow);
    }

    QPainterPath pagePath;
    pagePath.addRoundedRect(page, cornerRadius, cornerRadius);
    painter->setBrush(Qt::white);
    painter->drawPath(pagePath);

    const qreal devicePixelRatio = widget ? widget->devicePixelRatioF() : 1.0;
    const qreal lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform());
    const qreal wanted = quantiseScale(lod * devicePixelRatio);

    auto *queue = m_session->renderQueue();
    const auto priority = option->exposedRect.intersects(page) ? core::RenderPriority::Visible
                                                              : core::RenderPriority::Prefetch;
    const QImage fresh = queue->request(m_pageIndex, wanted, m_session->rotation(), priority);
    if (!fresh.isNull()) {
        m_bitmap = fresh;
        m_bitmapScale = wanted;
    }

    if (!m_bitmap.isNull()) {
        painter->save();
        painter->setClipPath(pagePath);
        // The bitmap may be at an older scale; stretching it keeps the page
        // readable while the sharper render is still in flight.
        painter->setRenderHint(QPainter::SmoothPixmapTransform,
                               !qFuzzyCompare(m_bitmapScale, wanted));
        painter->drawImage(page, m_bitmap);
        if (m_session->nightMode()) {
            painter->setCompositionMode(QPainter::CompositionMode_Difference);
            painter->fillRect(page, Qt::white);
        }
        painter->restore();
    } else {
        // Placeholder while the first bitmap is on its way: a very light page
        // number, which reads as "loading" without a spinner.
        QFont numberFont = themeFont(TextRole::Display);
        numberFont.setPointSizeF(qMax(12.0, page.height() * 0.06));
        QColor faint = colors.textMuted;
        faint.setAlphaF(0.25f);
        painter->setFont(numberFont);
        painter->setPen(faint);
        painter->drawText(page, Qt::AlignCenter, QString::number(m_pageIndex + 1));
        painter->setPen(Qt::NoPen);
    }

    if (!m_overlay.isEmpty()) {
        painter->save();
        painter->setClipPath(pagePath);
        painter->setTransform(pageToItem(), true);
        paintOverlay(painter, colors);
        painter->restore();
    }

    painter->setBrush(Qt::NoBrush);
    if (isCurrent) {
        QColor ring = colors.accent;
        ring.setAlphaF(0.55f);
        painter->setPen(QPen(ring, 1.4));
    } else {
        painter->setPen(QPen(colors.borderStrong, 0.8));
    }
    painter->drawPath(pagePath);
}

void PageItem::paintOverlay(QPainter *painter, const Palette &colors)
{
    // Selected text: a translucent wash in the accent colour, the way every
    // text editor does it, so it reads as selection and not as a highlight
    // annotation the user might mistake for content.
    if (!m_overlay.selection.isEmpty()) {
        QColor wash = colors.accent;
        wash.setAlphaF(0.28f);
        painter->setPen(Qt::NoPen);
        painter->setBrush(wash);
        for (const QRectF &box : m_overlay.selection)
            painter->drawRect(box.adjusted(-0.4, -0.4, 0.4, 0.4));
    }

    // Search hits: outlined so they stay visible over dark or coloured content,
    // with the current hit filled.
    if (!m_overlay.matches.isEmpty()) {
        QColor found = colors.markupYellow;
        found.setAlphaF(0.38f);
        painter->setBrush(found);
        painter->setPen(QPen(colors.markupYellow.darker(140), 0.7));
        for (const QRectF &box : m_overlay.matches)
            painter->drawRoundedRect(box.adjusted(-0.8, -0.8, 0.8, 0.8), 1.5, 1.5);
    }
    if (!m_overlay.activeMatch.isNull()) {
        QColor current = colors.accent;
        current.setAlphaF(0.42f);
        painter->setBrush(current);
        painter->setPen(QPen(colors.accent, 1.1));
        painter->drawRoundedRect(m_overlay.activeMatch.adjusted(-1, -1, 1, 1), 1.5, 1.5);
    }

    // The annotation the user has picked: a dashed frame with corner handles, so
    // it is clearly a selection and not part of the page.
    if (!m_overlay.selectedAnnotation.isNull()) {
        const QRectF frame = m_overlay.selectedAnnotation.adjusted(-2, -2, 2, 2);
        painter->setBrush(Qt::NoBrush);
        QPen dashed(colors.accent, 1.0, Qt::DashLine);
        dashed.setDashPattern({ 4, 3 });
        painter->setPen(dashed);
        painter->drawRect(frame);

        painter->setPen(QPen(colors.accent, 0.8));
        painter->setBrush(colors.surface);
        constexpr qreal handle = 3.0;
        for (const QPointF &corner : { frame.topLeft(), frame.topRight(), frame.bottomLeft(),
                                       frame.bottomRight() }) {
            painter->drawRect(QRectF(corner.x() - handle, corner.y() - handle, handle * 2,
                                     handle * 2));
        }
    }

    // Freehand in progress, drawn with the same colour and width the committed
    // ink annotation will have.
    if (!m_overlay.strokes.isEmpty()) {
        QPen inkPen(m_overlay.toolColor.isValid() ? m_overlay.toolColor : colors.accent,
                    m_overlay.toolWidth);
        inkPen.setCapStyle(Qt::RoundCap);
        inkPen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(inkPen);
        painter->setBrush(Qt::NoBrush);
        for (const QPolygonF &stroke : m_overlay.strokes) {
            if (stroke.size() == 1)
                painter->drawPoint(stroke.first());
            else
                painter->drawPolyline(stroke);
        }
    }

    if (!m_overlay.pendingShape.isNull()) {
        const QColor tool = m_overlay.toolColor.isValid() ? m_overlay.toolColor : colors.accent;
        switch (m_overlay.pendingShapeKind) {
        case PageOverlay::PendingShape::Redaction: {
            painter->setPen(QPen(Qt::black, 1.0));
            QColor fill = Qt::black;
            fill.setAlphaF(0.72f);
            painter->setBrush(fill);
            painter->drawRect(m_overlay.pendingShape);
            break;
        }
        case PageOverlay::PendingShape::Ellipse:
            painter->setPen(QPen(tool, m_overlay.toolWidth));
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(m_overlay.pendingShape);
            break;
        case PageOverlay::PendingShape::Rectangle:
            painter->setPen(QPen(tool, m_overlay.toolWidth));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(m_overlay.pendingShape);
            break;
        case PageOverlay::PendingShape::Line: {
            painter->setPen(QPen(tool, m_overlay.toolWidth, Qt::SolidLine, Qt::RoundCap));
            painter->setBrush(Qt::NoBrush);
            painter->drawLine(m_overlay.pendingLineStart, m_overlay.pendingLineEnd);
            break;
        }
        case PageOverlay::PendingShape::None:
            break;
        }
    }
}

} // namespace sweet::ui
