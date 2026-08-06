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

} // namespace sweet::ui
