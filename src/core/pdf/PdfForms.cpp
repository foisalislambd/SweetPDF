#include "core/pdf/PdfForms.h"

#include "core/pdf/Geometry.h"
#include "core/pdf/MuContext.h"
#include "core/pdf/PdfDocument.h"
#include "core/util/Logging.h"

#include <QCoreApplication>

#include <mupdf/pdf.h>

namespace sweet::core::forms {
namespace {

FieldType typeOf(fz_context *ctx, pdf_annot *widget, int flags)
{
    switch (pdf_widget_type(ctx, widget)) {
    case PDF_WIDGET_TYPE_BUTTON:
        return (flags & PDF_BTN_FIELD_IS_PUSHBUTTON) ? FieldType::PushButton : FieldType::Checkbox;
    case PDF_WIDGET_TYPE_CHECKBOX:
        return FieldType::Checkbox;
    case PDF_WIDGET_TYPE_RADIOBUTTON:
        return FieldType::RadioButton;
    case PDF_WIDGET_TYPE_COMBOBOX:
        return FieldType::ComboBox;
    case PDF_WIDGET_TYPE_LISTBOX:
        return FieldType::ListBox;
    case PDF_WIDGET_TYPE_TEXT:
        return FieldType::Text;
    case PDF_WIDGET_TYPE_SIGNATURE:
        return FieldType::Signature;
    case PDF_WIDGET_TYPE_UNKNOWN:
        break;
    }
    return FieldType::Unknown;
}

QString ownedString(fz_context *ctx, char *owned)
{
    if (!owned)
        return {};
    const QString copied = QString::fromUtf8(owned);
    fz_free(ctx, owned);
    return copied;
}

QStringList choicesOf(fz_context *ctx, pdf_annot *widget)
{
    const int count = pdf_choice_widget_options(ctx, widget, 0, nullptr);
    if (count <= 0)
        return {};

    QList<const char *> raw(count, nullptr);
    pdf_choice_widget_options(ctx, widget, 0, raw.data());

    QStringList options;
    options.reserve(count);
    for (const char *option : raw)
        options.append(QString::fromUtf8(option ? option : ""));
    return options;
}

FormField describe(fz_context *ctx, pdf_annot *widget, int pageIndex)
{
    FormField field;
    field.pageIndex = pageIndex;

    pdf_obj *object = pdf_annot_obj(ctx, widget);
    field.objectNumber = pdf_to_num(ctx, object);

    const int flags = pdf_field_flags(ctx, object);
    field.type = typeOf(ctx, widget, flags);
    field.readOnly = (flags & PDF_FIELD_IS_READ_ONLY) != 0 || pdf_widget_is_readonly(ctx, widget);
    field.required = (flags & PDF_FIELD_IS_REQUIRED) != 0;
    field.multiline = (flags & PDF_TX_FIELD_IS_MULTILINE) != 0;

    field.name = ownedString(ctx, pdf_load_field_name(ctx, object));
    if (const char *label = pdf_field_label(ctx, object))
        field.label = QString::fromUtf8(label);
    if (const char *value = pdf_field_value(ctx, object))
        field.value = QString::fromUtf8(value);
    field.rect = toQRectF(pdf_bound_widget(ctx, widget));

    if (field.type == FieldType::ComboBox || field.type == FieldType::ListBox)
        field.options = choicesOf(ctx, widget);
    if (field.type == FieldType::Signature)
        field.signed_ = pdf_widget_is_signed(ctx, widget) != 0;

    return field;
}

/// Runs `visitor(ctx, page, widget)` on the widget carrying `objectNumber`.
template <typename Visitor>
bool withWidget(const PdfDocument &document, int pageIndex, int objectNumber, Visitor visitor)
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
        for (pdf_annot *widget = pdf_first_widget(ctx, page); widget;
             widget = pdf_next_widget(ctx, widget)) {
            if (pdf_to_num(ctx, pdf_annot_obj(ctx, widget)) != objectNumber)
                continue;
            visitor(ctx, pdf, widget);
            found = true;
            break;
        }
    }
    fz_always(ctx)
        fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
    fz_catch(ctx) {
        qCWarning(lcPdf) << "Form field access failed:" << MuContext::lastError(ctx);
        return false;
    }

    return found;
}

} // namespace

QList<FormField> onPage(const PdfDocument &document, int pageIndex)
{
    QList<FormField> fields;
    pdf_document *pdf = document.pdfHandle();
    if (!pdf || pageIndex < 0 || pageIndex >= document.pageCount())
        return fields;

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(document.parseMutex());

    pdf_page *page = nullptr;
    fz_var(page);
    fz_try(ctx) {
        page = pdf_load_page(ctx, pdf, pageIndex);
        for (pdf_annot *widget = pdf_first_widget(ctx, page); widget;
             widget = pdf_next_widget(ctx, widget)) {
            fields.append(describe(ctx, widget, pageIndex));
        }
    }
    fz_always(ctx)
        fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
    fz_catch(ctx) {
        qCWarning(lcPdf) << "Could not read the form on page" << pageIndex + 1 << ":"
                         << MuContext::lastError(ctx);
        return {};
    }

    return fields;
}

QList<FormField> all(const PdfDocument &document)
{
    QList<FormField> fields;
    for (int index = 0; index < document.pageCount(); ++index)
        fields.append(onPage(document, index));
    return fields;
}

bool hasFields(const PdfDocument &document)
{
    for (int index = 0; index < document.pageCount(); ++index) {
        if (!onPage(document, index).isEmpty())
            return true;
    }
    return false;
}

bool setValue(PdfDocument &document, int pageIndex, int objectNumber, const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    bool accepted = false;

    const bool found = withWidget(document, pageIndex, objectNumber,
                                  [&](fz_context *ctx, pdf_document *pdf, pdf_annot *widget) {
        if (pdf_widget_is_readonly(ctx, widget))
            return;

        switch (pdf_widget_type(ctx, widget)) {
        case PDF_WIDGET_TYPE_TEXT:
            accepted = pdf_set_text_field_value(ctx, widget, utf8.constData()) != 0;
            break;
        case PDF_WIDGET_TYPE_COMBOBOX:
        case PDF_WIDGET_TYPE_LISTBOX:
            accepted = pdf_set_choice_field_value(ctx, widget, utf8.constData()) != 0;
            break;
        default:
            // Buttons carry a state name rather than free text, and MuPDF's
            // generic setter is the one that knows how to toggle them.
            accepted = pdf_set_field_value(ctx, pdf, pdf_annot_obj(ctx, widget), utf8.constData(),
                                           0)
                != 0;
            break;
        }

        if (accepted)
            pdf_update_widget(ctx, widget);
    });

    if (found && accepted) {
        // Widget appearances live in the page content, so the cached raster and
        // the text layer both go stale.
        if (auto page = document.page(pageIndex)) {
            page->discardDisplayList();
            page->discardTextLayer();
        }
    }
    return found && accepted;
}

QString onStateOf(const PdfDocument &document, int pageIndex, int objectNumber)
{
    QString state;
    withWidget(document, pageIndex, objectNumber,
               [&](fz_context *ctx, pdf_document *, pdf_annot *widget) {
                   if (pdf_obj *on = pdf_button_field_on_state(ctx, pdf_annot_obj(ctx, widget)))
                       state = QString::fromUtf8(pdf_to_name(ctx, on));
               });
    return state.isEmpty() ? QStringLiteral("Yes") : state;
}

bool resetAll(PdfDocument &document)
{
    pdf_document *pdf = document.pdfHandle();
    if (!pdf)
        return false;

    fz_context *ctx = MuContext::forThread();
    {
        const std::lock_guard<std::mutex> guard(document.parseMutex());
        fz_try(ctx) {
            pdf_reset_form(ctx, pdf, nullptr, 0);
            pdf_calculate_form(ctx, pdf);
        }
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not reset the form:" << MuContext::lastError(ctx);
            return false;
        }
    }

    for (int index = 0; index < document.pageCount(); ++index) {
        if (auto page = document.page(index))
            page->discardDisplayList();
    }
    return true;
}

bool flatten(PdfDocument &document, bool annotationsToo, bool widgetsToo)
{
    pdf_document *pdf = document.pdfHandle();
    if (!pdf || (!annotationsToo && !widgetsToo))
        return false;

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(document.parseMutex());
    fz_try(ctx)
        pdf_bake_document(ctx, pdf, annotationsToo ? 1 : 0, widgetsToo ? 1 : 0);
    fz_catch(ctx) {
        qCWarning(lcPdf) << "Could not flatten the document:" << MuContext::lastError(ctx);
        return false;
    }
    return true;
}

QString displayName(FieldType type)
{
    switch (type) {
    case FieldType::PushButton:
        return QCoreApplication::translate("FormField", "Button");
    case FieldType::Checkbox:
        return QCoreApplication::translate("FormField", "Checkbox");
    case FieldType::RadioButton:
        return QCoreApplication::translate("FormField", "Radio button");
    case FieldType::ComboBox:
        return QCoreApplication::translate("FormField", "Dropdown");
    case FieldType::ListBox:
        return QCoreApplication::translate("FormField", "List");
    case FieldType::Text:
        return QCoreApplication::translate("FormField", "Text field");
    case FieldType::Signature:
        return QCoreApplication::translate("FormField", "Signature");
    case FieldType::Unknown:
        break;
    }
    return QCoreApplication::translate("FormField", "Field");
}

} // namespace sweet::core::forms
