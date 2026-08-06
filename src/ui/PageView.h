#pragma once

#include "core/DocumentSession.h"

#include <QGraphicsView>
#include <QList>

class QGraphicsScene;

namespace sweet::ui {

class PageItem;

/// The scrolling page canvas.
///
/// Pages live in a QGraphicsScene measured in PDF points, so zoom is a view
/// transform and annotations added later become ordinary child items with page
/// coordinates - no second coordinate system to keep in sync.
class PageView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit PageView(sweet::core::DocumentSession *session, QWidget *parent = nullptr);

    sweet::core::DocumentSession *session() const noexcept { return m_session; }

    /// Scrolls so `pageIndex` starts at the top of the viewport.
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
    PageItem *itemForPage(int pageIndex) const;

    sweet::core::DocumentSession *m_session = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QList<PageItem *> m_items;
    bool m_syncingCurrentPage = false;
    bool m_temporaryPan = false; ///< Space bar or middle button held down.
};

} // namespace sweet::ui
