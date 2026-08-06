#pragma once

#include <QColor>
#include <QDateTime>
#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>

namespace sweet::core {

class PdfDocument;

/// The annotation types SweetPDF can create or recognise. Anything else in a
/// document is still listed, as `Other`, and left untouched.
enum class AnnotationType {
    Highlight,
    Underline,
    StrikeOut,
    Squiggly,
    Ink,      ///< Freehand drawing.
    Note,     ///< Sticky note ("Text" in the PDF spec).
    FreeText, ///< Text typed directly onto the page.
    Square,
    Circle,
    Line,
    Redact, ///< Marked for removal; content goes when redactions are applied.
    Stamp,
    Link,
    Widget, ///< Form field.
    Other,
};

/// Line-cap styles for Line annotations. Values match MuPDF's pdf_line_ending.
enum class LineEnding {
    None = 0,
    Square,
    Circle,
    Diamond,
    OpenArrow,
    ClosedArrow,
    Butt,
    ROpenArrow,
    RClosedArrow,
    Slash,
};

/// An annotation as the UI sees it. `objectNumber` is the PDF object number,
/// which stays stable while the document is open, so it is what commands and
/// panels refer to rather than a position in a list.
struct Annotation
{
    int pageIndex = -1;
    int objectNumber = 0;
    AnnotationType type = AnnotationType::Other;
    QRectF rect;              ///< Bounding box in unrotated page points.
    QList<QRectF> quads;      ///< Text markup boxes, one per line.
    QList<QPolygonF> strokes; ///< Ink strokes, in page points.
    QPointF lineStart;        ///< Line annotation start, in page points.
    QPointF lineEnd;          ///< Line annotation end, in page points.
    LineEnding lineStartStyle = LineEnding::None;
    LineEnding lineEndStyle = LineEnding::None;
    QColor color;
    qreal opacity = 1.0;
    qreal borderWidth = 1.5;
    QString contents;        ///< Note body, or the text of a FreeText.
    QString author;
    QString iconName;        ///< Stamp name (Draft, Approved, …).
    QDateTime modified;
    bool editable = true;    ///< False for form fields and unknown types.
};

/// What to create. Only the fields relevant to `type` are read.
struct AnnotationSpec
{
    AnnotationType type = AnnotationType::Highlight;
    int pageIndex = 0;
    QRectF rect;                    ///< Square, Circle, Note, FreeText, Redact, Stamp.
    QList<QRectF> quads;            ///< Text markup.
    QList<QPolygonF> strokes;       ///< Ink, in page points.
    QPointF lineStart;              ///< Line endpoints (when type is Line).
    QPointF lineEnd;
    bool hasLineEndpoints = false;  ///< True when lineStart/lineEnd are set.
    LineEnding lineStartStyle = LineEnding::None;
    LineEnding lineEndStyle = LineEnding::None;
    QColor color = QColor(0xFF, 0xDD, 0x4A);
    QColor interiorColor;           ///< Fill for Square and Circle; invalid means none.
    qreal opacity = 1.0;
    qreal borderWidth = 1.5;
    qreal fontSize = 11.0;
    QString contents;
    QString author;
    QString iconName;               ///< Stamp icon name (Draft, Approved, …).
};

/// Reads and writes the annotations of one document.
///
/// These are free functions rather than a class because they hold no state: the
/// document is the state. Every one of them takes the document's parse lock, so
/// they are safe to call from the GUI thread while pages render in the
/// background, and each marks the document as edited.
namespace annotations {

QList<Annotation> onPage(const PdfDocument &document, int pageIndex);

/// Every annotation in the document, in page order. Used by the comments panel.
QList<Annotation> all(const PdfDocument &document);

/// Creates an annotation and returns its object number, or 0 on failure.
int add(PdfDocument &document, const AnnotationSpec &spec);

bool remove(PdfDocument &document, int pageIndex, int objectNumber);

/// Changes colour, opacity or text of an existing annotation.
bool update(PdfDocument &document, int pageIndex, int objectNumber, const QColor &color,
            qreal opacity, const QString &contents);

/// Moves and resizes an existing annotation.
bool setRect(PdfDocument &document, int pageIndex, int objectNumber, const QRectF &rect);

/// True when the page carries at least one redaction annotation waiting to be
/// applied.
bool hasPendingRedactions(const PdfDocument &document, int pageIndex);

QString displayName(AnnotationType type);

} // namespace annotations
} // namespace sweet::core
