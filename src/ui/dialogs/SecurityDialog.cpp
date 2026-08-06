#include "ui/dialogs/SecurityDialog.h"

#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace sweet::ui {

SecurityDialog::SecurityDialog(const core::DocumentPermissions &current, bool alreadyEncrypted,
                               QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Protect document"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(14);

    auto *heading = new QLabel(tr("Protect document"), this);
    heading->setFont(themeFont(TextRole::Title));
    layout->addWidget(heading);

    m_encrypt = new QCheckBox(tr("Encrypt this document with a password"), this);
    m_encrypt->setChecked(true);
    layout->addWidget(m_encrypt);

    auto *passwords = new QGroupBox(tr("Passwords"), this);
    auto *passwordForm = new QFormLayout(passwords);
    passwordForm->setSpacing(8);

    m_userPassword = new QLineEdit(passwords);
    m_userPassword->setEchoMode(QLineEdit::Password);
    m_userConfirm = new QLineEdit(passwords);
    m_userConfirm->setEchoMode(QLineEdit::Password);
    m_ownerPassword = new QLineEdit(passwords);
    m_ownerPassword->setEchoMode(QLineEdit::Password);

    passwordForm->addRow(tr("To open the document:"), m_userPassword);
    passwordForm->addRow(tr("Confirm:"), m_userConfirm);
    passwordForm->addRow(tr("To change permissions:"), m_ownerPassword);

    auto *explain = new QLabel(tr("Leave the first password empty to let anyone open the file "
                                  "while still restricting what they can do with it."),
                               passwords);
    explain->setObjectName(QStringLiteral("SectionLabel"));
    explain->setWordWrap(true);
    passwordForm->addRow(explain);

    m_strength = new QComboBox(passwords);
    m_strength->addItem(tr("AES 256-bit (Acrobat 9 and later)"));
    m_strength->addItem(tr("AES 128-bit (Acrobat 7 and later)"));
    passwordForm->addRow(tr("Encryption:"), m_strength);
    layout->addWidget(passwords);

    auto *permissions = new QGroupBox(tr("Allow the reader to"), this);
    auto *permissionLayout = new QVBoxLayout(permissions);
    permissionLayout->setSpacing(6);

    const auto addPermission = [permissions, permissionLayout](const QString &label, bool on) {
        auto *box = new QCheckBox(label, permissions);
        box->setChecked(on);
        permissionLayout->addWidget(box);
        return box;
    };
    m_allowPrint = addPermission(tr("Print the document"), current.print);
    m_allowCopy = addPermission(tr("Copy text and images"), current.copy);
    m_allowModify = addPermission(tr("Change the content"), current.modify);
    m_allowAnnotate = addPermission(tr("Add comments and annotations"), current.annotate);
    m_allowForms = addPermission(tr("Fill in form fields"), current.form);
    m_allowAssemble = addPermission(tr("Insert, delete and rotate pages"), current.assemble);
    layout->addWidget(permissions);

    m_warning = new QLabel(this);
    m_warning->setObjectName(QStringLiteral("SectionLabel"));
    m_warning->setWordWrap(true);
    layout->addWidget(m_warning);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save protected copy…"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    const auto refresh = [this, passwords, permissions, buttons] {
        const bool on = m_encrypt->isChecked();
        passwords->setEnabled(on);
        permissions->setEnabled(on);
        updateWarning();
        buttons->button(QDialogButtonBox::Save)
            ->setEnabled(!on || m_userPassword->text() == m_userConfirm->text());
    };
    connect(m_encrypt, &QCheckBox::toggled, this, refresh);
    connect(m_userPassword, &QLineEdit::textChanged, this, refresh);
    connect(m_userConfirm, &QLineEdit::textChanged, this, refresh);

    if (alreadyEncrypted) {
        m_warning->setText(tr("This document is already encrypted. Saving replaces its existing "
                              "protection."));
    }
    refresh();
}

void SecurityDialog::updateWarning()
{
    if (!m_encrypt->isChecked()) {
        m_warning->setText(tr("The saved copy will have no encryption and no restrictions."));
        return;
    }
    if (m_userPassword->text() != m_userConfirm->text()) {
        m_warning->setText(tr("The two passwords do not match."));
        return;
    }
    if (m_userPassword->text().isEmpty() && m_ownerPassword->text().isEmpty()) {
        m_warning->setText(tr("Without an owner password the restrictions can be lifted by any "
                              "PDF tool. Set one to make them stick."));
        return;
    }
    m_warning->clear();
}

core::SaveOptions SecurityDialog::options() const
{
    core::SaveOptions options;
    // Encryption changes the file structure, so this can never be incremental.
    options.incremental = false;
    options.garbageCollect = true;

    if (!m_encrypt->isChecked()) {
        options.encryption = core::SaveOptions::Encryption::None;
        return options;
    }

    options.encryption = m_strength->currentIndex() == 0
        ? core::SaveOptions::Encryption::Aes256
        : core::SaveOptions::Encryption::Aes128;
    options.userPassword = m_userPassword->text();
    options.ownerPassword = m_ownerPassword->text();
    options.permissions.print = m_allowPrint->isChecked();
    options.permissions.copy = m_allowCopy->isChecked();
    options.permissions.modify = m_allowModify->isChecked();
    options.permissions.annotate = m_allowAnnotate->isChecked();
    options.permissions.form = m_allowForms->isChecked();
    options.permissions.assemble = m_allowAssemble->isChecked();
    return options;
}

} // namespace sweet::ui
