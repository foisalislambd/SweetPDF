#include "ui/dialogs/OcrDialog.h"

#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
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
namespace {

/// Language data that is actually installed, so the list never offers something
/// that will fail halfway through a long document.
QStringList installedLanguages()
{
    static const QStringList searchPaths = {
        QStringLiteral("/usr/share/tesseract-ocr/5/tessdata"),
        QStringLiteral("/usr/share/tesseract-ocr/4.00/tessdata"),
        QStringLiteral("/usr/share/tesseract-ocr/tessdata"),
        QStringLiteral("/usr/share/tessdata"),
        QStringLiteral("/opt/homebrew/share/tessdata"),
    };

    QStringList found;
    for (const QString &path : searchPaths) {
        const QDir directory(path);
        if (!directory.exists())
            continue;
        const QStringList files = directory.entryList({ QStringLiteral("*.traineddata") },
                                                      QDir::Files, QDir::Name);
        for (const QString &file : files) {
            const QString code = QFileInfo(file).completeBaseName();
            if (code != QLatin1String("osd") && !found.contains(code))
                found.append(code);
        }
    }
    return found;
}

} // namespace

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

    auto *explain = new QLabel(tr("Every page is rendered, read by the OCR engine, and written "
                                  "to a new PDF with an invisible text layer, so the result "
                                  "looks the same but can be searched and copied."),
                               this);
    explain->setObjectName(QStringLiteral("SectionLabel"));
    explain->setWordWrap(true);
    layout->addWidget(explain);

    auto *form = new QFormLayout;
    form->setSpacing(8);

    m_language = new QComboBox(this);
    const QStringList languages = installedLanguages();
    if (languages.isEmpty()) {
        m_language->addItem(tr("English (eng)"), QStringLiteral("eng"));
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
                                 "package, for example tesseract-ocr-eng.")
                            : tr("300 dpi suits most scans. Higher settings are slower and "
                                 "rarely read better."),
                        this);
    m_hint->setObjectName(QStringLiteral("SectionLabel"));
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Recognise"));
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

int OcrDialog::dpi() const
{
    return m_dpi->value();
}

} // namespace sweet::ui
