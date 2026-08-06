#pragma once

#include "core/pdf/PdfDocument.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

namespace sweet::ui {

/// Password protection and permissions.
///
/// The two passwords do different jobs and people mix them up, so the dialog says
/// what each one does rather than just labelling them: the user password is needed
/// to open the file at all, the owner password is what unlocks the restrictions.
class SecurityDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SecurityDialog(const sweet::core::DocumentPermissions &current, bool alreadyEncrypted,
                            QWidget *parent = nullptr);

    /// Save options carrying the chosen encryption, passwords and permissions.
    sweet::core::SaveOptions options() const;

private:
    void updateWarning();

    QCheckBox *m_encrypt = nullptr;
    QComboBox *m_strength = nullptr;
    QLineEdit *m_userPassword = nullptr;
    QLineEdit *m_userConfirm = nullptr;
    QLineEdit *m_ownerPassword = nullptr;
    QLabel *m_warning = nullptr;

    QCheckBox *m_allowPrint = nullptr;
    QCheckBox *m_allowCopy = nullptr;
    QCheckBox *m_allowModify = nullptr;
    QCheckBox *m_allowAnnotate = nullptr;
    QCheckBox *m_allowForms = nullptr;
    QCheckBox *m_allowAssemble = nullptr;
};

} // namespace sweet::ui
