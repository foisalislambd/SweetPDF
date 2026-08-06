#pragma once

#include "core/pdf/PdfForms.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QStackedLayout;
class QTimer;
class QVBoxLayout;

namespace sweet::core {
class DocumentSession;
}

namespace sweet::ui {

/// Fill-in view of the document's interactive form.
///
/// Every field gets the editor its type asks for - a line edit, a checkbox, a
/// dropdown - grouped under the page it sits on, so a long form can be filled in
/// from the keyboard without hunting for boxes on the page. Values are written
/// straight back into the document when an editor is committed, which is also what
/// updates the page itself.
class FormsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit FormsPanel(QWidget *parent = nullptr);

    void setSession(sweet::core::DocumentSession *session);

    /// True when the open document has at least one fillable field.
    bool hasFields() const noexcept { return m_fieldCount > 0; }

Q_SIGNALS:
    /// The user focused a field and wants to see it on the page.
    void fieldActivated(int pageIndex, int objectNumber);
    void statusMessage(const QString &message);

private:
    void scheduleRebuild();
    void rebuild();
    void clearRows();

    /// Builds the editor for one field and wires it to the session.
    void addRow(const sweet::core::FormField &field);

    sweet::core::DocumentSession *m_session = nullptr;
    QStackedLayout *m_stack = nullptr;
    QWidget *m_fields = nullptr;
    QVBoxLayout *m_fieldsLayout = nullptr;
    QWidget *m_emptyState = nullptr;
    QLabel *m_summary = nullptr;
    QTimer *m_rebuildTimer = nullptr;
    int m_fieldCount = 0;

    /// Set while an editor is writing its value, so the resulting change signal
    /// does not rebuild the panel out from under the widget being typed in.
    bool m_writing = false;
};

} // namespace sweet::ui
