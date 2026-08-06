#include "ui/dialogs/ExportImagesDialog.h"

#include "ui/dialogs/PageRangeInput.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

enum Scope { AllPages, CurrentPage, CustomRange };

} // namespace

ExportImagesDialog::ExportImagesDialog(int pageCount, int currentPage,
                                       const QString &suggestedName, QWidget *parent)
    : QDialog(parent)
    , m_pageCount(pageCount)
    , m_currentPage(currentPage)
{
    setWindowTitle(tr("Export as images"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(14);

    auto *heading = new QLabel(tr("Export as images"), this);
    heading->setFont(themeFont(TextRole::Title));
    layout->addWidget(heading);

    auto *form = new QFormLayout;
    form->setSpacing(8);

    m_scope = new QComboBox(this);
    m_scope->addItem(tr("All %n page(s)", nullptr, pageCount));
    m_scope->addItem(tr("Current page (%1)").arg(currentPage + 1));
    m_scope->addItem(tr("Pages…"));
    form->addRow(tr("Export:"), m_scope);

    m_range = new QLineEdit(this);
    m_range->setPlaceholderText(tr("for example 1-4, 8, 11-13"));
    m_range->setEnabled(false);
    form->addRow(QString(), m_range);

    auto *folderRow = new QHBoxLayout;
    m_directory = new QLineEdit(this);
    m_directory->setText(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    folderRow->addWidget(m_directory, 1);
    auto *browse = new QToolButton(this);
    browse->setIcon(icon(QStringLiteral("folder")));
    browse->setToolTip(tr("Choose a folder"));
    folderRow->addWidget(browse);
    form->addRow(tr("Folder:"), folderRow);

    m_baseName = new QLineEdit(suggestedName, this);
    form->addRow(tr("File name:"), m_baseName);

    m_format = new QComboBox(this);
    m_format->addItem(tr("PNG (lossless)"), QStringLiteral("png"));
    m_format->addItem(tr("JPEG (smaller)"), QStringLiteral("jpg"));
    m_format->addItem(tr("TIFF"), QStringLiteral("tiff"));
    m_format->addItem(tr("WebP"), QStringLiteral("webp"));
    form->addRow(tr("Format:"), m_format);

    m_dpi = new QSpinBox(this);
    m_dpi->setRange(36, 900);
    m_dpi->setValue(150);
    m_dpi->setSingleStep(6);
    m_dpi->setSuffix(tr(" dpi"));
    form->addRow(tr("Resolution:"), m_dpi);

    layout->addLayout(form);

    m_estimate = new QLabel(this);
    m_estimate->setObjectName(QStringLiteral("SectionLabel"));
    m_estimate->setWordWrap(true);
    layout->addWidget(m_estimate);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Export"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browse, &QToolButton::clicked, this, &ExportImagesDialog::chooseDirectory);
    connect(m_scope, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_range->setEnabled(index == CustomRange);
        if (index == CustomRange)
            m_range->setFocus();
        updateEstimate();
    });
    connect(m_range, &QLineEdit::textChanged, this, &ExportImagesDialog::updateEstimate);
    connect(m_dpi, &QSpinBox::valueChanged, this, &ExportImagesDialog::updateEstimate);

    updateEstimate();
}

void ExportImagesDialog::chooseDirectory()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Choose a folder"),
                                                             m_directory->text());
    if (!chosen.isEmpty())
        m_directory->setText(chosen);
}

void ExportImagesDialog::updateEstimate()
{
    const int count = int(pages().size());
    // Rough guide only, but enough to stop someone exporting 400 pages at 600 dpi
    // by accident.
    const qreal megapixels = qreal(m_dpi->value() * m_dpi->value()) * 8.5 * 11.0 / 1'000'000.0;
    m_estimate->setText(tr("%n file(s), roughly %1 megapixels each.", nullptr, count)
                            .arg(megapixels, 0, 'f', 1));
}

QList<int> ExportImagesDialog::pages() const
{
    switch (m_scope->currentIndex()) {
    case CurrentPage:
        return { m_currentPage };
    case CustomRange:
        return parsePageRange(m_range->text(), m_pageCount);
    default:
        return parsePageRange(QString(), m_pageCount);
    }
}

QString ExportImagesDialog::directory() const
{
    return m_directory->text();
}

QString ExportImagesDialog::baseName() const
{
    const QString name = m_baseName->text().trimmed();
    return name.isEmpty() ? QStringLiteral("page") : name;
}

QString ExportImagesDialog::format() const
{
    return m_format->currentData().toString();
}

int ExportImagesDialog::dpi() const
{
    return m_dpi->value();
}

} // namespace sweet::ui
