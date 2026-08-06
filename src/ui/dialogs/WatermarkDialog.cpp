#include "ui/dialogs/WatermarkDialog.h"

#include "ui/dialogs/PageRangeInput.h"
#include "ui/theme/Theme.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

enum Scope { AllPages, CurrentPage, CustomRange };

QPixmap swatch(const QColor &colour)
{
    QPixmap pixmap(18, 18);
    pixmap.fill(colour);
    return pixmap;
}

} // namespace

WatermarkDialog::WatermarkDialog(int pageCount, int currentPage, QWidget *parent)
    : QDialog(parent)
    , m_pageCount(pageCount)
    , m_currentPage(currentPage)
{
    setWindowTitle(tr("Watermark"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(14);

    auto *heading = new QLabel(tr("Add a watermark"), this);
    heading->setFont(themeFont(TextRole::Title));
    layout->addWidget(heading);

    auto *form = new QFormLayout;
    form->setSpacing(8);

    m_text = new QLineEdit(tr("DRAFT"), this);
    form->addRow(tr("Text:"), m_text);

    m_colourButton = new QToolButton(this);
    m_colourButton->setIcon(swatch(m_colour));
    m_colourButton->setText(tr("Choose…"));
    m_colourButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    form->addRow(tr("Colour:"), m_colourButton);

    m_opacity = new QSlider(Qt::Horizontal, this);
    m_opacity->setRange(5, 100);
    m_opacity->setValue(22);
    form->addRow(tr("Opacity:"), m_opacity);

    m_fontSize = new QSpinBox(this);
    m_fontSize->setRange(8, 160);
    m_fontSize->setValue(56);
    form->addRow(tr("Size:"), m_fontSize);

    m_position = new QComboBox(this);
    m_position->addItem(tr("Centre of the page"));
    m_position->addItem(tr("Header"));
    m_position->addItem(tr("Footer"));
    form->addRow(tr("Position:"), m_position);

    m_scope = new QComboBox(this);
    m_scope->addItem(tr("All %n page(s)", nullptr, pageCount));
    m_scope->addItem(tr("Current page (%1)").arg(currentPage + 1));
    m_scope->addItem(tr("Pages…"));
    form->addRow(tr("Apply to:"), m_scope);

    m_range = new QLineEdit(this);
    m_range->setPlaceholderText(tr("for example 1-4, 8"));
    m_range->setEnabled(false);
    form->addRow(QString(), m_range);

    layout->addLayout(form);

    auto *note = new QLabel(tr("The watermark is added as a text annotation, so it can be "
                               "selected and removed again later."),
                            this);
    note->setObjectName(QStringLiteral("SectionLabel"));
    note->setWordWrap(true);
    layout->addWidget(note);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Add watermark"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_colourButton, &QToolButton::clicked, this, &WatermarkDialog::pickColour);
    connect(m_scope, &QComboBox::currentIndexChanged, this,
            [this](int index) { m_range->setEnabled(index == CustomRange); });
}

void WatermarkDialog::pickColour()
{
    const QColor chosen = QColorDialog::getColor(m_colour, this, tr("Watermark colour"));
    if (!chosen.isValid())
        return;
    m_colour = chosen;
    m_colourButton->setIcon(swatch(m_colour));
}

QString WatermarkDialog::text() const
{
    return m_text->text();
}

QColor WatermarkDialog::color() const
{
    return m_colour;
}

qreal WatermarkDialog::opacity() const
{
    return m_opacity->value() / 100.0;
}

int WatermarkDialog::fontSize() const
{
    return m_fontSize->value();
}

WatermarkDialog::Position WatermarkDialog::position() const
{
    switch (m_position->currentIndex()) {
    case 1:
        return Position::Top;
    case 2:
        return Position::Bottom;
    default:
        return Position::Centre;
    }
}

QList<int> WatermarkDialog::pages() const
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

} // namespace sweet::ui
