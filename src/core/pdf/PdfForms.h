#pragma once

#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace sweet::core {

class PdfDocument;

/// What MuPDF's signature check says about a signed field.
enum class SignatureStatus {
    NotApplicable, ///< Not a signature field.
    Unsigned,
    Valid,
    Invalid,
    BrokenByEdits, ///< The signature was valid until unsaved changes.
    Unchecked,     ///< The check could not run (missing crypto, or it threw).
};

/// The kinds of AcroForm field SweetPDF can fill in.
enum class FieldType {
    Unknown,
    PushButton, ///< Runs an action; there is no value to fill in.
    Checkbox,
    RadioButton,
    ComboBox,
    ListBox,
    Text,
    Signature,
};

/// One form field, flattened out of the field tree into something a panel can
/// show as a single row. `objectNumber` identifies the widget annotation, so it
/// survives repaints and list rebuilds; `name` is the fully qualified field name
/// and is what other widgets of the same field share.
struct FormField
{
    int pageIndex = -1;
    int objectNumber = 0;
    FieldType type = FieldType::Unknown;
    QString name;
    QString label;   ///< Tooltip text from the document, if the author set one.
    QString value;
    QStringList options; ///< Choices for combo and list boxes.
    QRectF rect;         ///< Widget box in unrotated page points.
    bool readOnly = false;
    bool required = false;
    bool multiline = false;
    bool signed_ = false; ///< Signature fields that already carry a signature.
    SignatureStatus signature = SignatureStatus::NotApplicable;
};

/// Reads and fills the interactive form of one document.
///
/// Like the annotation functions these are stateless and take the document's
/// parse lock, so they can be called from the GUI thread while pages render.
namespace forms {

/// Every fillable widget in the document, in page order.
QList<FormField> all(const PdfDocument &document);

QList<FormField> onPage(const PdfDocument &document, int pageIndex);

/// True when the document has at least one widget, i.e. it is a form.
bool hasFields(const PdfDocument &document);

enum class NewFieldKind { Text, Checkbox, Combo, Radio, Signature };

/// Adds a widget to the page and to the document's AcroForm. `choices` is the
/// option list for a combo box. The rectangle is in unrotated page points.
bool createField(PdfDocument &document, int pageIndex, NewFieldKind kind, const QString &name,
                 const QRectF &rect, const QStringList &choices);

/// Writes a new value. Checkboxes and radio buttons take "Off" or their on
/// state; text and choice fields take the text as typed. Returns false when the
/// field is missing, read-only, or the document's own validation script rejects
/// the value.
bool setValue(PdfDocument &document, int pageIndex, int objectNumber, const QString &value);

/// The name of the "on" state of a checkbox or radio button, which is what
/// `setValue` needs to tick it. Usually "Yes", but authors can pick anything.
QString onStateOf(const PdfDocument &document, int pageIndex, int objectNumber);

/// Puts every field back to the value the author shipped.
bool resetAll(PdfDocument &document);

/// Draws annotation and widget appearances into the page content and removes the
/// interactive objects, which is how a filled form is made final.
bool flatten(PdfDocument &document, bool annotationsToo, bool widgetsToo);

QString displayName(FieldType type);

} // namespace forms
} // namespace sweet::core
