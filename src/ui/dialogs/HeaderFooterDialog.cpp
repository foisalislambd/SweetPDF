#include "ui/dialogs/HeaderFooterDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace sweet::ui {

HeaderFooterDialog::HeaderFooterDialog(int pageCount, int currentPage, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Header and footer"));
    setMinimumWidth(380);

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_header = new QLineEdit(this);
    m_header->setPlaceholderText(tr("Optional"));
    form->addRow(tr("Header"), m_header);
    m_footer = new QLineEdit(this);
    m_footer->setPlaceholderText(tr("Optional"));
    form->addRow(tr("Footer"), m_footer);

    m_numbers = new QCheckBox(tr("Add page numbers"), this);
    m_numbers->setChecked(true);
    form->addRow(QString(), m_numbers);

    m_bates = new QCheckBox(tr("Bates number"), this);
    form->addRow(QString(), m_bates);
    m_batesPrefix = new QLineEdit(this);
    m_batesPrefix->setPlaceholderText(tr("ACME"));
    form->addRow(tr("Bates prefix"), m_batesPrefix);
    m_batesStart = new QSpinBox(this);
    m_batesStart->setRange(0, 9999999);
    m_batesStart->setValue(1);
    form->addRow(tr("Bates start"), m_batesStart);
    m_batesDigits = new QSpinBox(this);
    m_batesDigits->setRange(1, 10);
    m_batesDigits->setValue(6);
    form->addRow(tr("Bates digits"), m_batesDigits);

    m_fontSize = new QSpinBox(this);
    m_fontSize->setRange(8, 36);
    m_fontSize->setValue(10);
    m_fontSize->setSuffix(tr(" pt"));
    form->addRow(tr("Size"), m_fontSize);

    m_scope = new QComboBox(this);
    m_scope->addItem(tr("All %n page(s)", nullptr, pageCount), true);
    m_scope->addItem(tr("Current page (%1)").arg(currentPage + 1), false);
    form->addRow(tr("Pages"), m_scope);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *column = new QVBoxLayout(this);
    column->addLayout(form);
    auto *hint = new QLabel(tr("Use {n} for the page number and {total} for the page count."), this);
    hint->setWordWrap(true);
    column->addWidget(hint);
    column->addWidget(buttons);
}

QString HeaderFooterDialog::headerText() const
{
    return m_header->text().trimmed();
}

QString HeaderFooterDialog::footerText() const
{
    return m_footer->text().trimmed();
}

bool HeaderFooterDialog::pageNumbers() const
{
    return m_numbers->isChecked();
}

bool HeaderFooterDialog::batesNumbers() const
{
    return m_bates->isChecked();
}

QString HeaderFooterDialog::batesPrefix() const
{
    return m_batesPrefix->text().trimmed();
}

int HeaderFooterDialog::batesStart() const
{
    return m_batesStart->value();
}

int HeaderFooterDialog::batesDigits() const
{
    return m_batesDigits->value();
}

int HeaderFooterDialog::fontSize() const
{
    return m_fontSize->value();
}

bool HeaderFooterDialog::allPages() const
{
    return m_scope->currentData().toBool();
}

} // namespace sweet::ui
