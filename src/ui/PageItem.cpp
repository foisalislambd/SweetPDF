#include "ui/PageItem.h"

#include "core/DocumentSession.h"
#include "core/PdfDocument.h"
#include "ui/Theme.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace sweet::ui {
namespace {

constexpr qreal shadowSpread = 6.0; ///< Shadow reach in points.
constexpr qreal maxRenderScale = 6.0;

/// Rounds the scale to a coarse ladder so that panning and small zoom nudges
/// reuse cached bitmaps instead of re-rendering every page.
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

QRectF PageItem::boundingRect() const
{
    return pageRect().adjusted(-shadowSpread, -shadowSpread, shadowSpread, shadowSpread + 2);
}

void PageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    const QRectF page = pageRect();
    const Palette &colors = themePalette();

    painter->setRenderHint(QPainter::Antialiasing, true);

    // Soft shadow, built from a few translucent rounded rects. Cheaper than a
    // blur effect and there are only a handful of pages on screen.
    painter->setPen(Qt::NoPen);
    for (int layer = 4; layer >= 1; --layer) {
        QColor tint = colors.shadow;
        tint.setAlpha(qMax(6, colors.shadow.alpha() / (layer + 2)));
        painter->setBrush(tint);
        const qreal grow = layer * 1.5;
        painter->drawRoundedRect(page.adjusted(-grow, -grow + 1, grow, grow + 2), 3, 3);
    }

    painter->setBrush(Qt::white);
    painter->drawRect(page);

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
        // The bitmap may be at an older scale; stretching it keeps the page
        // readable while the sharper render is still in flight.
        const bool needsSmoothing = !qFuzzyCompare(m_bitmapScale, wanted);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, needsSmoothing);
        painter->drawImage(page, m_bitmap);
    } else {
        painter->setPen(QPen(colors.border, 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(page);
        painter->setPen(colors.textMuted);
        painter->drawText(page, Qt::AlignCenter,
                          QObject::tr("Page %1").arg(m_pageIndex + 1));
    }

    painter->setPen(QPen(colors.borderStrong, 0.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(page);
}

} // namespace sweet::ui
