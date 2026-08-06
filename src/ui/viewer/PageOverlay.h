#pragma once

#include <QColor>
#include <QList>
#include <QPolygonF>
#include <QRectF>

namespace sweet::ui {

/// Everything drawn on top of a page bitmap that is not part of the PDF yet.
///
/// Committed annotations come out of MuPDF inside the page bitmap, so they are
/// not in here. This is only the live, uncommitted layer: what the user has
/// selected, what search found, and the shape or stroke currently being drawn.
/// Coordinates are unrotated page points, the same space the document uses, so
/// the same overlay is correct at any zoom or view rotation.
struct PageOverlay
{
    QList<QRectF> selection;  ///< Selected text, one box per line.
    QList<QRectF> matches;    ///< Search hits on this page.
    QRectF activeMatch;       ///< The hit the view is currently sitting on.
    QRectF selectedAnnotation; ///< Handles are drawn around this.

    QList<QPolygonF> strokes; ///< Freehand stroke in progress.
    QRectF pendingShape;      ///< Rectangle, ellipse or redaction being dragged.

    enum class PendingShape { None, Rectangle, Ellipse, Redaction };
    PendingShape pendingShapeKind = PendingShape::None;

    QColor toolColor;    ///< Colour of the pending stroke or shape.
    qreal toolWidth = 2.0;

    bool isEmpty() const
    {
        return selection.isEmpty() && matches.isEmpty() && activeMatch.isNull()
            && selectedAnnotation.isNull() && strokes.isEmpty() && pendingShape.isNull();
    }
};

} // namespace sweet::ui
