#pragma once

#include "core/session/DocumentSession.h"

#include <QGraphicsView>
#include <QList>
#include <QPointF>

class QGraphicsScene;
class QPropertyAnimation;

namespace sweet::ui {

class PageItem;

/// The scrolling page canvas.
///
/// Pages live in a QGraphicsScene measured in PDF points, so zoom is a view
/// transform and annotations added later become ordinary child items with page
/// coordinates - no second coordinate system to keep in sync.
///
/// Zoom keeps a fixed anchor: the point under the cursor when zooming with the
/// wheel, the centre of the viewport otherwise, so the page never jumps away
/// from what the reader was looking at.
class PageView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit PageView(sweet::core::DocumentSession *session, QWidget *parent = nullptr);

    sweet::core::DocumentSession *session() const noexcept { return m_session; }

    /// Scrolls so `pageIndex` starts at the top of the viewport, animated.
    void goToPage(int pageIndex);

public Q_SLOTS:
    void zoomIn();
    void zoomOut();
    void zoomTo(qreal zoom);
    void fitPage();
    void fitWidth();
    void actualSize();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    void rebuildScene();
    void layoutPages();
    void applyZoom();
    void applyTool();
    void updateFitZoom();
    void updateCurrentPageFromViewport();
    void animateScrollTo(int value);
    PageItem *itemForPage(int pageIndex) const;

    sweet::core::DocumentSession *m_session = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QList<PageItem *> m_items;
    QPropertyAnimation *m_scrollAnimation = nullptr;

    /// Scene point to keep still across the next zoom change, with the viewport
    /// position it should stay at.
    QPointF m_zoomAnchorScene;
    QPoint m_zoomAnchorViewport;
    bool m_zoomAnchorValid = false;

    int m_indicatedPage = -1;    ///< Page currently drawn with the accent ring.
    bool m_syncingCurrentPage = false;
    bool m_temporaryPan = false; ///< Middle button held down.
};

} // namespace sweet::ui
