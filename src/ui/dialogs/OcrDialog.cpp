#include "ui/dialogs/OcrDialog.h"

#include "core/pdf/PdfExport.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace sweet::ui {

OcrDialog::OcrDialog(const QString &suggestedOutput, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Recognise text"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(14);

    auto *heading = new QLabel(tr("Recognise text (OCR)"), this);
    heading->setFont(themeFont(TextRole::Title));
    layout->addWidget(heading);

    auto *explain = new QLabel(tr("Every page is rendered, read by Tesseract, and written to a "
                                  "new PDF with an invisible text layer, so the result looks "
                                  "the same but can be searched, selected and copied."),
                               this);
    explain->setObjectName(QStringLiteral("SectionLabel"));
    explain->setWordWrap(true);
    layout->addWidget(explain);

    auto *form = new QFormLayout;
    form->setSpacing(8);

    m_language = new QComboBox(this);
    m_tessdataDir = core::exporter::defaultTessdataDir();
    const QStringList languages = core::exporter::installedOcrLanguages(m_tessdataDir);
    if (languages.isEmpty()) {
        m_language->addItem(tr("No languages installed"), QString());
        m_language->setEnabled(false);
    } else {
        for (const QString &code : languages)
            m_language->addItem(code, code);
        const int english = m_language->findData(QStringLiteral("eng"));
        if (english >= 0)
            m_language->setCurrentIndex(english);
    }
    form->addRow(tr("Language:"), m_language);

    m_dpi = new QSpinBox(this);
    m_dpi->setRange(150, 600);
    m_dpi->setValue(300);
    m_dpi->setSingleStep(50);
    m_dpi->setSuffix(tr(" dpi"));
    form->addRow(tr("Render at:"), m_dpi);

    auto *outputRow = new QHBoxLayout;
    m_output = new QLineEdit(suggestedOutput, this);
    outputRow->addWidget(m_output, 1);
    auto *browse = new QToolButton(this);
    browse->setIcon(icon(QStringLiteral("folder")));
    browse->setToolTip(tr("Choose where to save"));
    outputRow->addWidget(browse);
    form->addRow(tr("Save as:"), outputRow);

    layout->addLayout(form);

    m_hint = new QLabel(languages.isEmpty()
                            ? tr("No language data was found. Install a Tesseract language "
                                 "package, for example tesseract-ocr-eng, then reopen this "
                                 "dialog.")
                            : tr("300 dpi suits most scans. Higher settings are slower and "
                                 "rarely read better."),
                        this);
    m_hint->setObjectName(QStringLiteral("SectionLabel"));
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Recognise"));
    buttons->button(QDialogButtonBox::Ok)->setEnabled(!languages.isEmpty());
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browse, &QToolButton::clicked, this, &OcrDialog::chooseOutput);
}

void OcrDialog::chooseOutput()
{
    const QString chosen = QFileDialog::getSaveFileName(this, tr("Save searchable PDF"),
                                                        m_output->text(),
                                                        tr("PDF documents (*.pdf)"));
    if (!chosen.isEmpty())
        m_output->setText(chosen);
}

QString OcrDialog::language() const
{
    return m_language->currentData().toString().isEmpty() ? m_language->currentText()
                                                          : m_language->currentData().toString();
}

QString OcrDialog::outputPath() const
{
    return m_output->text();
}

QString OcrDialog::tessdataDir() const
{
    return m_tessdataDir;
}

int OcrDialog::dpi() const
{
    return m_dpi->value();
}

} // namespace sweet::ui
