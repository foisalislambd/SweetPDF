#include "core/pdf/PdfForms.h"

#include "core/pdf/Geometry.h"
#include "core/pdf/MuVersion.h"
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
    if (field.type == FieldType::Signature) {
        field.signed_ = pdf_widget_is_signed(ctx, widget) != 0;
        if (!field.signed_) {
            field.signature = SignatureStatus::Unsigned;
        } else {
            int status = 0;
            bool checked = false;
            fz_var(status);
            fz_var(checked);
            fz_try(ctx)
            {
#if SWEETPDF_MUPDF_1_28
                status = pdf_validate_signature_widget(ctx, widget);
#else
                status = pdf_validate_signature(ctx, widget);
#endif
                checked = true;
            }
            fz_catch(ctx)
                checked = false;

            if (!checked)
                field.signature = SignatureStatus::Unchecked;
            else if (status == 0)
                field.signature = SignatureStatus::Valid;
            else if (status < 0)
                field.signature = SignatureStatus::BrokenByEdits;
            else
                field.signature = SignatureStatus::Invalid;
        }
    }

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
    document.ensureStructure();
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
    document.ensureStructure();
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
    document.ensureStructure();
    pdf_document *pdf = document.pdfHandle();
    if (!pdf || (!annotationsToo && !widgetsToo))
        return false;

    fz_context *ctx = MuContext::forThread();
    {
        const std::lock_guard<std::mutex> guard(document.parseMutex());
        fz_try(ctx)
            pdf_bake_document(ctx, pdf, annotationsToo ? 1 : 0, widgetsToo ? 1 : 0);
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not flatten the document:" << MuContext::lastError(ctx);
            return false;
        }
    }

    // Baked content replaces annotation appearances; drop every cached display
    // list so the next render shows the baked page, not the old overlay.
    document.discardPageCaches(-1);
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

bool createField(PdfDocument &document, int pageIndex, NewFieldKind kind, const QString &name,
                 const QRectF &rect, const QStringList &choices)
{
    document.ensureStructure();
    pdf_document *pdf = document.pdfHandle();
    if (!pdf || name.trimmed().isEmpty() || rect.isEmpty() || pageIndex < 0
        || pageIndex >= document.pageCount()) {
        return false;
    }

    fz_context *ctx = MuContext::forThread();
    const QByteArray fieldName = name.trimmed().toUtf8();
    bool created = false;
    bool began = false;

    {
        const std::lock_guard<std::mutex> guard(document.parseMutex());
        pdf_page *page = nullptr;
        pdf_annot *annot = nullptr;
        fz_var(page);
        fz_var(annot);
        fz_try(ctx) {
            page = pdf_load_page(ctx, pdf, pageIndex);
            pdf_begin_operation(ctx, pdf, "Create field");
            began = true;
            annot = pdf_create_annot_raw(ctx, page, PDF_ANNOT_WIDGET);
            pdf_obj *obj = pdf_annot_obj(ctx, annot);
            pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, pdf), PDF_NAME(Root));
            pdf_obj *acroform = pdf_dict_get(ctx, root, PDF_NAME(AcroForm));
            if (!acroform) {
                acroform = pdf_new_dict(ctx, pdf, 2);
                pdf_dict_put_drop(ctx, root, PDF_NAME(AcroForm), acroform);
            }
            pdf_obj *fields = pdf_dict_get(ctx, acroform, PDF_NAME(Fields));
            if (!fields) {
                fields = pdf_new_array(ctx, pdf, 1);
                pdf_dict_put_drop(ctx, acroform, PDF_NAME(Fields), fields);
            }

            pdf_set_annot_rect(ctx, annot, toFzRect(rect));
            pdf_dict_put_int(ctx, obj, PDF_NAME(F), PDF_ANNOT_IS_PRINT);
            pdf_dict_put_text_string(ctx, obj, PDF_NAME(T), fieldName.constData());
            pdf_dict_put_text_string(ctx, obj, PDF_NAME(DA), "/Helv 12 Tf 0 g");
            char radioOn[16] = { 0 };

            if (kind == NewFieldKind::Checkbox) {
                pdf_dict_put(ctx, obj, PDF_NAME(FT), PDF_NAME(Btn));
                pdf_dict_put_name(ctx, obj, PDF_NAME(V), "Off");
                pdf_dict_put_name(ctx, obj, PDF_NAME(AS), "Off");
            } else if (kind == NewFieldKind::Radio) {
                pdf_obj *parent = nullptr;
                pdf_obj *ownedParent = nullptr;
                const int fieldCount = pdf_array_len(ctx, fields);
                for (int fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex) {
                    pdf_obj *field = pdf_array_get(ctx, fields, fieldIndex);
                    const char *existing =
                        pdf_to_text_string(ctx, pdf_dict_get(ctx, field, PDF_NAME(T)));
                    pdf_obj *fieldType = pdf_dict_get(ctx, field, PDF_NAME(FT));
                    const int flags = pdf_dict_get_int(ctx, field, PDF_NAME(Ff));
                    if (existing && fieldName == existing && pdf_name_eq(ctx, fieldType, PDF_NAME(Btn))
                        && (flags & PDF_BTN_FIELD_IS_RADIO) != 0)
                        parent = field;
                }
                if (!parent) {
                    ownedParent = pdf_add_new_dict(ctx, pdf, 4);
                    parent = ownedParent;
                    pdf_dict_put(ctx, parent, PDF_NAME(FT), PDF_NAME(Btn));
                    pdf_dict_put_int(ctx, parent, PDF_NAME(Ff), PDF_BTN_FIELD_IS_RADIO);
                    pdf_dict_put_text_string(ctx, parent, PDF_NAME(T), fieldName.constData());
                    pdf_dict_put_name(ctx, parent, PDF_NAME(V), "Off");
                    pdf_dict_put_drop(ctx, parent, PDF_NAME(Kids), pdf_new_array(ctx, pdf, 2));
                    pdf_array_push(ctx, fields, parent);
                    pdf_drop_obj(ctx, ownedParent);
                }
                pdf_dict_del(ctx, obj, PDF_NAME(T));
                pdf_dict_put(ctx, obj, PDF_NAME(Parent), parent);
                pdf_dict_put_name(ctx, obj, PDF_NAME(AS), "Off");
                pdf_obj *kids = pdf_dict_get(ctx, parent, PDF_NAME(Kids));
                fz_snprintf(radioOn, sizeof(radioOn), "Ch%d", pdf_array_len(ctx, kids));
                pdf_array_push(ctx, kids, obj);
            } else if (kind == NewFieldKind::Signature) {
                pdf_dict_put(ctx, obj, PDF_NAME(FT), PDF_NAME(Sig));
            } else if (kind == NewFieldKind::Combo) {
                pdf_dict_put(ctx, obj, PDF_NAME(FT), PDF_NAME(Ch));
                pdf_dict_put_int(ctx, obj, PDF_NAME(Ff), PDF_CH_FIELD_IS_COMBO);
                pdf_obj *options = pdf_new_array(ctx, pdf, choices.size());
                for (const QString &choice : choices) {
                    const QByteArray utf8 = choice.toUtf8();
                    pdf_array_push_text_string(ctx, options, utf8.constData());
                }
                pdf_dict_put_drop(ctx, obj, PDF_NAME(Opt), options);
            } else {
                pdf_dict_put(ctx, obj, PDF_NAME(FT), PDF_NAME(Tx));
                pdf_dict_put_text_string(ctx, obj, PDF_NAME(V), "");
            }

            if (kind != NewFieldKind::Radio)
                pdf_array_push(ctx, fields, obj);
            pdf_update_annot(ctx, annot);
            if (radioOn[0] != '\0') {
                // The generated appearance is named Yes. Each radio in a group needs
                // its own on-state, otherwise selecting one selects all of them.
                pdf_obj *normal = pdf_dict_get(ctx, pdf_dict_get(ctx, obj, PDF_NAME(AP)), PDF_NAME(N));
                pdf_obj *yes = pdf_dict_get(ctx, normal, PDF_NAME(Yes));
                if (yes) {
                    pdf_obj *key = pdf_new_name(ctx, radioOn);
                    pdf_dict_put(ctx, normal, key, yes);
                    pdf_drop_obj(ctx, key);
                    pdf_dict_del(ctx, normal, PDF_NAME(Yes));
                }
            }
            pdf_end_operation(ctx, pdf);
            created = true;
        }
        fz_always(ctx) {
            if (created)
                pdf_drop_annot(ctx, annot);
            else if (began)
                pdf_abandon_operation(ctx, pdf);
            if (!created && annot)
                pdf_delete_annot(ctx, page, annot);
            fz_drop_page(ctx, reinterpret_cast<fz_page *>(page));
        }
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not create the form field:" << MuContext::lastError(ctx);
            return false;
        }
    }

    if (created) {
        if (auto page = document.page(pageIndex))
            page->discardDisplayList();
    }
    return created;
}

} // namespace sweet::core::forms
