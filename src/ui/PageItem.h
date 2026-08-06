#pragma once

#include <QGraphicsItem>
#include <QImage>

namespace sweet::core {
class DocumentSession;
}

namespace sweet::ui {

/// One page on the canvas.
///
/// Scene coordinates are PDF points, so zooming is just the view transform and
/// nothing has to be laid out again. Each item asks the render queue for a
/// bitmap at the resolution the current transform actually needs, and keeps the
/// previous bitmap on screen until the sharper one arrives.
class PageItem : public QGraphicsItem
{
public:
    PageItem(sweet::core::DocumentSession *session, int pageIndex);

    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    int pageIndex() const noexcept { return m_pageIndex; }

    /// Page area without the drop shadow, in points.
    QRectF pageRect() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    /// Re-reads the page size, e.g. after the view rotation changed.
    void refreshGeometry();

    /// Drops the kept bitmap so the next paint renders from scratch.
    void discardBitmap();

private:
    sweet::core::DocumentSession *m_session = nullptr;
    int m_pageIndex = 0;
    QSizeF m_sizePoints;
    QImage m_bitmap;      ///< Last bitmap received, possibly at a stale scale.
    qreal m_bitmapScale = 0.0;
};

} // namespace sweet::ui
