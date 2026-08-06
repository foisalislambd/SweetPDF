#pragma once

#include "core/pdf/PageText.h"
#include "core/session/DocumentSession.h"

#include <QGraphicsView>
#include <QHash>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QPolygonF>

class QGraphicsScene;
class QPlainTextEdit;
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

    // --- Text selection ---------------------------------------------------

    bool hasSelection() const;
    QString selectedText() const;
    int selectionPage() const noexcept { return m_selectionPage; }

    /// Turns the current selection into a markup annotation. This is how the
    /// ribbon's Highlight button works when text is already selected.
    bool markUpSelection(sweet::core::AnnotationType type);

    // --- Search -----------------------------------------------------------

    /// Hits to draw, keyed by page index.
    void showMatches(const QHash<int, QList<sweet::core::TextRange>> &matches);
    void clearMatches();

    /// Highlights one hit and scrolls it into view.
    void focusMatch(int pageIndex, const sweet::core::TextRange &range);

    // --- Annotations ------------------------------------------------------

    /// Scrolls to an annotation and selects it.
    void revealAnnotation(int pageIndex, int objectNumber);

    /// Deletes whatever annotation is selected. Returns false when none is.
    bool deleteSelectedAnnotation();

    /// Opens the note editor for the selected annotation.
    void editSelectedAnnotation();

    /// Boxes covering the current text selection, in page points. Empty when
    /// nothing is selected. Used by commands that need the geometry rather than
    /// the text, such as linking a phrase.
    QList<QRectF> selectionBoxes() const;

    // --- Placing pictures and links ---------------------------------------
    //
    // Both are placed by dragging a box on the page, the way Acrobat does it: the
    // command arms the view, the next drag decides where the thing lands. A click
    // without a drag drops a picture at its natural size and cancels a link,
    // which has no natural size.

    void beginImagePlacement(const QImage &image, const QString &commandText);
    void beginLinkPlacement(const QString &uri);
    void cancelPlacement();
    bool isPlacing() const noexcept { return m_placement != Placement::None; }

public Q_SLOTS:
    void copySelection();
    void selectAllOnCurrentPage();
    void clearSelection();
    void zoomIn();
    void zoomOut();
    void zoomTo(qreal zoom);
    void fitPage();
    void fitWidth();
    void actualSize();

Q_SIGNALS:
    void selectionChanged(bool hasSelection);
    /// A note or text box was double-clicked, or a new one needs its text.
    void annotationSelected(int pageIndex, int objectNumber);
    void statusMessage(const QString &message);

    /// Text was selected with the Edit text tool. The view opens an inline editor
    /// itself; this signal remains for listeners that want to know.
    void textEditRequested();

    /// The inline editor committed a replacement for the current selection.
    void textReplaceRequested(const QString &replacement);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
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

    /// The page under a viewport position, with the point converted to unrotated
    /// page points. Returns nullptr when the position is on the canvas background.
    PageItem *pageAt(const QPoint &viewportPos, QPointF *pagePoint = nullptr) const;

    std::shared_ptr<const sweet::core::PageText> textOf(int pageIndex) const;

    /// Rebuilds the overlay of one page, or of every page when `pageIndex` is -1.
    void refreshOverlay(int pageIndex = -1);

    void setSelection(int pageIndex, const sweet::core::TextRange &range);
    void scrollRectIntoView(int pageIndex, const QRectF &pageRect);

    /// Turns whatever the current tool was drawing into an annotation.
    void commitPendingAnnotation();

    /// Turns the dragged box into the picture or link that was armed.
    void commitPlacement();

    /// Follows a link under `pagePoint`, if there is one. Returns true when a link
    /// was there, whether or not the reader chose to follow it.
    bool followLinkAt(int pageIndex, const QPointF &pagePoint);
    void createNoteAt(int pageIndex, const QPointF &pagePoint);
    void createTextBoxAt(int pageIndex, const QPointF &pagePoint);

    void startInlineTextEdit();
    void commitInlineTextEdit();
    void cancelInlineTextEdit();
    QRect selectionViewportRect() const;

    /// The annotation under a page point, topmost first, or an object number of 0.
    sweet::core::Annotation annotationAt(int pageIndex, const QPointF &pagePoint) const;

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

    /// Text selection. Kept as a page plus a character range rather than as
    /// rectangles, so it survives zooming and can be turned into either text or
    /// annotation quads on demand.
    int m_selectionPage = -1;
    sweet::core::TextRange m_selection;
    int m_selectionAnchor = -1;
    bool m_selectingText = false;
    QPoint m_textPressPos;

    QPlainTextEdit *m_inlineEdit = nullptr;

    /// Search hits, by page, plus the one that is currently focused.
    QHash<int, QList<sweet::core::TextRange>> m_matches;
    int m_activeMatchPage = -1;
    sweet::core::TextRange m_activeMatch;

    /// Freehand and shape drawing in progress.
    int m_drawPage = -1;
    QList<QPolygonF> m_strokes;
    QPolygonF m_currentStroke;
    QPointF m_shapeOrigin;
    QRectF m_pendingShape;
    bool m_drawing = false;

    /// A picture or link waiting for the box the user is about to drag.
    enum class Placement { None, Image, Link };
    Placement m_placement = Placement::None;
    QImage m_placementImage;
    QString m_placementText;
    QString m_placementUri;

    int m_selectedAnnotationPage = -1;
    int m_selectedAnnotation = 0;
    QRectF m_selectedAnnotationRect;
};

} // namespace sweet::ui
