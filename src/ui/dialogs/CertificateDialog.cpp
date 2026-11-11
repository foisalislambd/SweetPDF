#include "ui/dialogs/CertificateDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace sweet::ui {

CertificateDialog::CertificateDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Sign with a certificate"));
    setMinimumWidth(440);

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_path = new QLineEdit(this);
    m_path->setPlaceholderText(tr("certificate.p12"));
    auto *browse = new QPushButton(tr("Browse…"), this);
    connect(browse, &QPushButton::clicked, this, &CertificateDialog::browse);
    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(browse);
    form->addRow(tr("Certificate"), pathRow);

    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Password"), m_password);

    m_reason = new QLineEdit(this);
    m_reason->setPlaceholderText(tr("I approve this document"));
    form->addRow(tr("Reason"), m_reason);

    m_location = new QLineEdit(this);
    form->addRow(tr("Location"), m_location);

    auto *hint = new QLabel(
        tr("Uses a PKCS#12 file (.p12 or .pfx). The signature is written into the file "
           "on disk, so the document is saved first. A later full save can invalidate it; "
           "incremental save keeps it."),
        this);
    hint->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Sign"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *column = new QVBoxLayout(this);
    column->addLayout(form);
    column->addWidget(hint);
    column->addWidget(buttons);
}

void CertificateDialog::browse()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Certificate"), QString(),
        tr("PKCS#12 certificates (*.p12 *.pfx);;All files (*)"));
    if (!path.isEmpty())
        m_path->setText(path);
}

QString CertificateDialog::certificatePath() const
{
    return m_path->text().trimmed();
}

QString CertificateDialog::password() const
{
    return m_password->text();
}

QString CertificateDialog::reason() const
{
    return m_reason->text().trimmed();
}

QString CertificateDialog::location() const
{
    return m_location->text().trimmed();
}

} // namespace sweet::ui
