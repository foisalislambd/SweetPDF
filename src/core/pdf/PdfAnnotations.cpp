#include "core/pdf/PdfAnnotations.h"

#include "core/pdf/Geometry.h"
#include "core/pdf/MuContext.h"
#include "core/pdf/PdfDocument.h"
#include "core/util/Logging.h"

#include <QCoreApplication>

#include <algorithm>

#include <mupdf/pdf.h>

namespace sweet::core::annotations {
namespace {

AnnotationType fromMuType(enum pdf_annot_type type)
{
    switch (type) {
    case PDF_ANNOT_HIGHLIGHT:
        return AnnotationType::Highlight;
    case PDF_ANNOT_UNDERLINE:
        return AnnotationType::Underline;
    case PDF_ANNOT_STRIKE_OUT:
        return AnnotationType::StrikeOut;
    case PDF_ANNOT_SQUIGGLY:
        return AnnotationType::Squiggly;
    case PDF_ANNOT_INK:
        return AnnotationType::Ink;
    case PDF_ANNOT_TEXT:
        return AnnotationType::Note;
    case PDF_ANNOT_FREE_TEXT:
        return AnnotationType::FreeText;
    case PDF_ANNOT_SQUARE:
        return AnnotationType::Square;
    case PDF_ANNOT_CIRCLE:
        return AnnotationType::Circle;
    case PDF_ANNOT_LINE:
        return AnnotationType::Line;
    case PDF_ANNOT_REDACT:
        return AnnotationType::Redact;
    case PDF_ANNOT_STAMP:
        return AnnotationType::Stamp;
    case PDF_ANNOT_LINK:
        return AnnotationType::Link;
    case PDF_ANNOT_WIDGET:
        return AnnotationType::Widget;
    default:
        return AnnotationType::Other;
    }
}

enum pdf_annot_type toMuType(AnnotationType type)
{
    switch (type) {
    case AnnotationType::Highlight:
        return PDF_ANNOT_HIGHLIGHT;
    case AnnotationType::Underline:
        return PDF_ANNOT_UNDERLINE;
    case AnnotationType::StrikeOut:
        return PDF_ANNOT_STRIKE_OUT;
    case AnnotationType::Squiggly:
        return PDF_ANNOT_SQUIGGLY;
    case AnnotationType::Ink:
        return PDF_ANNOT_INK;
    case AnnotationType::Note:
        return PDF_ANNOT_TEXT;
    case AnnotationType::FreeText:
        return PDF_ANNOT_FREE_TEXT;
    case AnnotationType::Square:
        return PDF_ANNOT_SQUARE;
    case AnnotationType::Circle:
        return PDF_ANNOT_CIRCLE;
    case AnnotationType::Line:
        return PDF_ANNOT_LINE;
    case AnnotationType::Redact:
        return PDF_ANNOT_REDACT;
    case AnnotationType::Stamp:
        return PDF_ANNOT_STAMP;
    default:
        return PDF_ANNOT_SQUARE;
    }
}

bool isTextMarkup(AnnotationType type)
{
    return type == AnnotationType::Highlight || type == AnnotationType::Underline
        || type == AnnotationType::StrikeOut || type == AnnotationType::Squiggly;
}

/// PDF colours are component arrays; MuPDF picks the space from the count.
int colorComponents(const QColor &color, float out[4])
{
    if (!color.isValid())
        return 0;
    out[0] = float(color.redF());
    out[1] = float(color.greenF());
    out[2] = float(color.blueF());
    return 3;
}

QColor colorOf(fz_context *ctx, pdf_annot *annot)
{
    int count = 0;
    float components[4] = { 0, 0, 0, 0 };
    fz_try(ctx)
        pdf_annot_color(ctx, annot, &count, components);
    fz_catch(ctx)
        return {};

    switch (count) {
    case 1:
        return QColor::fromRgbF(components[0], components[0], components[0]);
    case 3:
        return QColor::fromRgbF(components[0], components[1], components[2]);
    case 4: {
        // CMYK, converted the same naive way MuPDF does for display.
        const float k = components[3];
        return QColor::fromRgbF((1 - components[0]) * (1 - k), (1 - components[1]) * (1 - k),
                                (1 - components[2]) * (1 - k));
    }
    default:
        return {};
    }
}

Annotation describe(fz_context *ctx, pdf_annot *annot, int pageIndex)
{
    Annotation described;
    described.pageIndex = pageIndex;
    described.objectNumber = pdf_to_num(ctx, pdf_annot_obj(ctx, annot));
    described.type = fromMuType(pdf_annot_type(ctx, annot));
    described.rect = toQRectF(pdf_bound_annot(ctx, annot));
    described.color = colorOf(ctx, annot);
    described.editable = described.type != AnnotationType::Widget
        && described.type != AnnotationType::Other && described.type != AnnotationType::Link;

    fz_try(ctx)
        described.opacity = pdf_annot_opacity(ctx, annot);
    fz_catch(ctx)
        described.opacity = 1.0;

    fz_try(ctx) {
        if (const char *contents = pdf_annot_contents(ctx, annot))
            described.contents = QString::fromUtf8(contents);
        if (const char *author = pdf_annot_author(ctx, annot))
            described.author = QString::fromUtf8(author);
        const int64_t modified = pdf_annot_modification_date(ctx, annot);
        if (modified > 0)
            described.modified = QDateTime::fromSecsSinceEpoch(modified);
    }
    fz_catch(ctx) {
        // Metadata is optional: an annotation without it is still usable.
    }

    if (isTextMarkup(described.type)) {
        fz_try(ctx) {
            const int quads = pdf_annot_quad_point_count(ctx, annot);
            for (int index = 0; index < quads; ++index)
                described.quads.append(toQRectF(pdf_annot_quad_point(ctx, annot, index)));
        }
        fz_catch(ctx) {
            described.quads.clear();
        }
    } else if (described.type == AnnotationType::Ink) {
        // Read back the strokes so deleting a drawing stays undoable.
        fz_try(ctx) {
            const int strokes = pdf_annot_ink_list_count(ctx, annot);
            for (int stroke = 0; stroke < strokes; ++stroke) {
                QPolygonF points;
                const int vertices = pdf_annot_ink_list_stroke_count(ctx, annot, stroke);
                points.reserve(vertices);
                for (int vertex = 0; vertex < vertices; ++vertex) {
                    const fz_point point =
                        pdf_annot_ink_list_stroke_vertex(ctx, annot, stroke, vertex);
                    points.append(QPointF(point.x, point.y));
                }
                described.strokes.append(points);
            }
        }
        fz_catch(ctx) {
            described.strokes.clear();
        }
    } else if (described.type == AnnotationType::Line) {
        fz_try(ctx) {
            fz_point a{};
            fz_point b{};
            pdf_annot_line(ctx, annot, &a, &b);
            described.lineStart = QPointF(a.x, a.y);
            described.lineEnd = QPointF(b.x, b.y);
            enum pdf_line_ending startStyle = PDF_ANNOT_LE_NONE;
            enum pdf_line_ending endStyle = PDF_ANNOT_LE_NONE;
            pdf_annot_line_ending_styles(ctx, annot, &startStyle, &endStyle);
            described.lineStartStyle = static_cast<LineEnding>(startStyle);
            described.lineEndStyle = static_cast<LineEnding>(endStyle);
        }
        fz_catch(ctx) {
            described.lineStart = {};
            described.lineEnd = {};
        }
    } else if (described.type == AnnotationType::Stamp) {
        fz_try(ctx) {
            if (const char *name = pdf_annot_icon_name(ctx, annot))
                described.iconName = QString::fromUtf8(name);
        }
        fz_catch(ctx) {
            described.iconName.clear();
        }
    }

    fz_try(ctx)
        described.borderWidth = pdf_annot_border_width(ctx, annot);
    fz_catch(ctx)
        described.borderWidth = 1.5;

    return described;
}

/// Runs `visitor` for the annotation with `objectNumber`, with the page loaded.
/// Returns false when the annotation is not there.
template <typename Visitor>
bool withAnnotation(PdfDocument &document, int pageIndex, int objectNumber, Visitor visitor)
{
    pdf_document *pdf = document.pdfHandle();
    if (!pdf || pageIndex < 0 || pageIndex >= document.pageCount())
        return false;

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(document.parseMutex());

    pdf_page *page = nullptr;
    bool found = false;
    fz_var(page);
    fz_var(found);
    fz_try(ctx) {
        page = pdf_load_page(ctx, pdf, pageIndex);
        for (pdf_annot *annot = pdf_first_annot(ctx, page); annot;
             annot = pdf_next_annot(ctx, annot)) {
            if (pdf_to_num(ctx, pdf_annot_obj(ctx, annot)) != objectNumber)
                continue;
            visitor(ctx, page, annot);
            pdf_update_annot(ctx, annot);
            found = true;
            break;
        }
    }
    fz_always(ctx)
        fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
    fz_catch(ctx) {
        qCWarning(lcPdf) << "Annotation edit failed:" << MuContext::lastError(ctx);
        return false;
    }

    return found;
}

} // namespace

QList<Annotation> onPage(const PdfDocument &document, int pageIndex)
{
    QList<Annotation> found;
    pdf_document *pdf = document.pdfHandle();
    if (!pdf || pageIndex < 0 || pageIndex >= document.pageCount())
        return found;

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(document.parseMutex());

    pdf_page *page = nullptr;
    fz_var(page);
    fz_try(ctx) {
        page = pdf_load_page(ctx, pdf, pageIndex);
        for (pdf_annot *annot = pdf_first_annot(ctx, page); annot;
             annot = pdf_next_annot(ctx, annot)) {
            found.append(describe(ctx, annot, pageIndex));
        }
    }
    fz_always(ctx)
        fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
    fz_catch(ctx) {
        qCWarning(lcPdf) << "Could not list annotations on page" << pageIndex + 1 << ":"
                         << MuContext::lastError(ctx);
        return {};
    }

    return found;
}

QList<Annotation> all(const PdfDocument &document)
{
    QList<Annotation> found;
    for (int index = 0; index < document.pageCount(); ++index)
        found.append(onPage(document, index));
    return found;
}

int add(PdfDocument &document, const AnnotationSpec &spec)
{
    pdf_document *pdf = document.pdfHandle();
    if (!pdf || spec.pageIndex < 0 || spec.pageIndex >= document.pageCount())
        return 0;

    fz_context *ctx = MuContext::forThread();
    int objectNumber = 0;

    {
        const std::lock_guard<std::mutex> guard(document.parseMutex());
        pdf_page *page = nullptr;
        fz_var(page);
        fz_var(objectNumber);
        fz_try(ctx) {
            page = pdf_load_page(ctx, pdf, spec.pageIndex);
            pdf_annot *annot = pdf_create_annot(ctx, page, toMuType(spec.type));

            float components[4] = { 0, 0, 0, 0 };
            if (const int count = colorComponents(spec.color, components))
                pdf_set_annot_color(ctx, annot, count, components);
            if (spec.opacity < 1.0)
                pdf_set_annot_opacity(ctx, annot, float(spec.opacity));

            if (isTextMarkup(spec.type) || spec.type == AnnotationType::Redact) {
                QList<fz_quad> quads;
                quads.reserve(spec.quads.size());
                for (const QRectF &box : spec.quads)
                    quads.append(fz_quad_from_rect(toFzRect(box)));
                if (!quads.isEmpty())
                    pdf_set_annot_quad_points(ctx, annot, int(quads.size()), quads.constData());
                else if (!spec.rect.isEmpty())
                    pdf_set_annot_rect(ctx, annot, toFzRect(spec.rect));
            } else if (spec.type == AnnotationType::Ink) {
                for (const QPolygonF &stroke : spec.strokes) {
                    if (stroke.isEmpty())
                        continue;
                    pdf_add_annot_ink_list_stroke(ctx, annot);
                    for (const QPointF &point : stroke) {
                        pdf_add_annot_ink_list_stroke_vertex(
                            ctx, annot, fz_make_point(float(point.x()), float(point.y())));
                    }
                }
                pdf_set_annot_border_width(ctx, annot, float(spec.borderWidth));
            } else if (spec.type == AnnotationType::Note) {
                // Sticky notes are a fixed-size marker wherever they are dropped.
                const QPointF at = spec.rect.topLeft();
                pdf_set_annot_rect(ctx, annot,
                                   toFzRect(QRectF(at, QSizeF(20, 20))));
            } else if (spec.type == AnnotationType::Line) {
                QPointF a = spec.lineStart;
                QPointF b = spec.lineEnd;
                if (!spec.hasLineEndpoints && !spec.rect.isEmpty()) {
                    a = spec.rect.topLeft();
                    b = spec.rect.bottomRight();
                }
                pdf_set_annot_line(ctx, annot, fz_make_point(float(a.x()), float(a.y())),
                                   fz_make_point(float(b.x()), float(b.y())));
                pdf_set_annot_border_width(ctx, annot, float(spec.borderWidth));
                pdf_set_annot_line_ending_styles(
                    ctx, annot, static_cast<enum pdf_line_ending>(spec.lineStartStyle),
                    static_cast<enum pdf_line_ending>(spec.lineEndStyle));
            } else {
                if (!spec.rect.isEmpty())
                    pdf_set_annot_rect(ctx, annot, toFzRect(spec.rect));
                if (spec.type == AnnotationType::Square || spec.type == AnnotationType::Circle) {
                    pdf_set_annot_border_width(ctx, annot, float(spec.borderWidth));
                    float interior[4] = { 0, 0, 0, 0 };
                    if (const int count = colorComponents(spec.interiorColor, interior))
                        pdf_set_annot_interior_color(ctx, annot, count, interior);
                }
                if (spec.type == AnnotationType::Stamp && !spec.iconName.isEmpty())
                    pdf_set_annot_icon_name(ctx, annot, spec.iconName.toUtf8().constData());
                if (spec.type == AnnotationType::FreeText) {
                    float text[4] = { 0, 0, 0, 0 };
                    const int count = colorComponents(spec.color, text);
                    pdf_set_annot_default_appearance(ctx, annot, "Helv", float(spec.fontSize),
                                                     count, text);
                    // Replacement text should look like page content, not a boxed note.
                    pdf_set_annot_border_width(ctx, annot, float(qMax(0.0, spec.borderWidth)));
                }
            }

            if (!spec.contents.isEmpty())
                pdf_set_annot_contents(ctx, annot, spec.contents.toUtf8().constData());
            if (!spec.author.isEmpty())
                pdf_set_annot_author(ctx, annot, spec.author.toUtf8().constData());

            const int64_t now = QDateTime::currentSecsSinceEpoch();
            pdf_set_annot_creation_date(ctx, annot, now);
            pdf_set_annot_modification_date(ctx, annot, now);

            pdf_update_annot(ctx, annot);
            objectNumber = pdf_to_num(ctx, pdf_annot_obj(ctx, annot));
        }
        fz_always(ctx)
            fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not create the annotation:" << MuContext::lastError(ctx);
            return 0;
        }
    }

    if (objectNumber != 0) {
        if (auto page = document.page(spec.pageIndex))
            page->discardDisplayList();
    }
    return objectNumber;
}

bool remove(PdfDocument &document, int pageIndex, int objectNumber)
{
    pdf_document *pdf = document.pdfHandle();
    if (!pdf || pageIndex < 0 || pageIndex >= document.pageCount())
        return false;

    fz_context *ctx = MuContext::forThread();
    bool removed = false;

    {
        const std::lock_guard<std::mutex> guard(document.parseMutex());
        pdf_page *page = nullptr;
        fz_var(page);
        fz_var(removed);
        fz_try(ctx) {
            page = pdf_load_page(ctx, pdf, pageIndex);
            for (pdf_annot *annot = pdf_first_annot(ctx, page); annot;
                 annot = pdf_next_annot(ctx, annot)) {
                if (pdf_to_num(ctx, pdf_annot_obj(ctx, annot)) != objectNumber)
                    continue;
                pdf_delete_annot(ctx, page, annot);
                removed = true;
                break;
            }
        }
        fz_always(ctx)
            fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not delete the annotation:" << MuContext::lastError(ctx);
            return false;
        }
    }

    if (removed) {
        if (auto page = document.page(pageIndex))
            page->discardDisplayList();
    }
    return removed;
}

bool update(PdfDocument &document, int pageIndex, int objectNumber, const QColor &color,
            qreal opacity, const QString &contents)
{
    const bool changed = withAnnotation(
        document, pageIndex, objectNumber, [&](fz_context *ctx, pdf_page *, pdf_annot *annot) {
            float components[4] = { 0, 0, 0, 0 };
            if (const int count = colorComponents(color, components))
                pdf_set_annot_color(ctx, annot, count, components);
            pdf_set_annot_opacity(ctx, annot, float(qBound(0.0, opacity, 1.0)));
            pdf_set_annot_contents(ctx, annot, contents.toUtf8().constData());
            pdf_set_annot_modification_date(ctx, annot, QDateTime::currentSecsSinceEpoch());
        });

    if (changed) {
        if (auto page = document.page(pageIndex))
            page->discardDisplayList();
    }
    return changed;
}

bool setRect(PdfDocument &document, int pageIndex, int objectNumber, const QRectF &rect)
{
    const bool changed = withAnnotation(
        document, pageIndex, objectNumber, [&](fz_context *ctx, pdf_page *, pdf_annot *annot) {
            pdf_set_annot_rect(ctx, annot, toFzRect(rect));
            pdf_set_annot_modification_date(ctx, annot, QDateTime::currentSecsSinceEpoch());
        });

    if (changed) {
        if (auto page = document.page(pageIndex))
            page->discardDisplayList();
    }
    return changed;
}

bool hasPendingRedactions(const PdfDocument &document, int pageIndex)
{
    const QList<Annotation> found = onPage(document, pageIndex);
    return std::any_of(found.cbegin(), found.cend(), [](const Annotation &annotation) {
        return annotation.type == AnnotationType::Redact;
    });
}

QString displayName(AnnotationType type)
{
    switch (type) {
    case AnnotationType::Highlight:
        return QCoreApplication::translate("Annotation", "Highlight");
    case AnnotationType::Underline:
        return QCoreApplication::translate("Annotation", "Underline");
    case AnnotationType::StrikeOut:
        return QCoreApplication::translate("Annotation", "Strikeout");
    case AnnotationType::Squiggly:
        return QCoreApplication::translate("Annotation", "Squiggly");
    case AnnotationType::Ink:
        return QCoreApplication::translate("Annotation", "Drawing");
    case AnnotationType::Note:
        return QCoreApplication::translate("Annotation", "Note");
    case AnnotationType::FreeText:
        return QCoreApplication::translate("Annotation", "Text box");
    case AnnotationType::Square:
        return QCoreApplication::translate("Annotation", "Rectangle");
    case AnnotationType::Circle:
        return QCoreApplication::translate("Annotation", "Ellipse");
    case AnnotationType::Line:
        return QCoreApplication::translate("Annotation", "Line");
    case AnnotationType::Redact:
        return QCoreApplication::translate("Annotation", "Redaction");
    case AnnotationType::Stamp:
        return QCoreApplication::translate("Annotation", "Stamp");
    case AnnotationType::Link:
        return QCoreApplication::translate("Annotation", "Link");
    case AnnotationType::Widget:
        return QCoreApplication::translate("Annotation", "Form field");
    case AnnotationType::Other:
        break;
    }
    return QCoreApplication::translate("Annotation", "Annotation");
}

} // namespace sweet::core::annotations
