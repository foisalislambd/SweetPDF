#include "ui/panels/ToolOptionsPanel.h"

#include "ui/theme/Theme.h"

#include <QColorDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

using core::Tool;
using core::LineEnding;

constexpr int swatchSize = 22;
const char *const iconNameProperty = "sweetToolIcon";

/// What each tool is called in the panel, what it does in one line, and which of
/// the shared controls it uses. Keeping this in one table means a new tool needs
/// one row rather than a new branch in three functions.
struct ToolInfo
{
    Tool tool;
    const char *icon;
    const char *name;
    const char *hint;
    bool colour;
    bool opacity;
    bool border;
};

const ToolInfo toolInfo[] = {
    { Tool::Highlight, "highlight", "Highlight",
      "Drag across text to highlight it. Colour and opacity apply to the next mark.", true, true,
      false },
    { Tool::Underline, "underline", "Underline", "Drag across the text you want underlined.", true,
      false, false },
    { Tool::Strikeout, "strikeout", "Strikeout", "Drag across the text you want struck through.",
      true, false, false },
    { Tool::Squiggly, "squiggly", "Squiggly", "Drag across text to mark it with a wavy underline.",
      true, false, false },
    { Tool::Draw, "draw", "Draw", "Draw freehand on the page. Hold the button and move.", true, true,
      false },
    { Tool::Note, "note", "Note", "Click the page to leave a note, then type in the bubble.", true,
      false, false },
    { Tool::AddText, "add-text", "Add text", "Click where the text should start, then type.", true,
      false, false },
    { Tool::Shape, "shape", "Rectangle", "Drag to draw a rectangle.", true, true, true },
    { Tool::Ellipse, "ellipse", "Ellipse", "Drag to draw an ellipse.", true, true, true },
    { Tool::Line, "line", "Line", "Drag from one point to another to draw a straight line.", true,
      true, true },
    { Tool::Arrow, "arrow", "Arrow", "Drag to draw a line with an arrowhead at the end.", true, true,
      true },
    { Tool::Stamp, "stamp", "Stamp", "Click the page to place a ready-made stamp.", false, false,
      false },
    { Tool::Redact, "redact", "Mark redaction",
      "Drag over anything to be removed, then apply the redactions to delete it for good.", false,
      false, false },
    { Tool::EditText, "edit-text", "Edit text",
      "Click a line to edit it, or drag across the words you want to change.", false, false, false },
    { Tool::EditImage, "edit-image", "Edit image", "Click an image to replace or remove it.", false,
      false, false },
    { Tool::AddImage, "add-image", "Add image", "Click where the image should go.", false, false,
      false },
};

bool isShapeFamily(Tool tool)
{
    return tool == Tool::Shape || tool == Tool::Ellipse || tool == Tool::Line || tool == Tool::Arrow;
}

const ToolInfo *infoFor(Tool tool)
{
    for (const ToolInfo &info : toolInfo) {
        if (info.tool == tool)
            return &info;
    }
    return nullptr;
}

/// The colours offered before reaching for the colour dialog: the three markup
/// shades from the theme plus the ink colours people reach for on a form.
QList<QColor> presetColours()
{
    const Palette &colours = themePalette();
    return { colours.markupYellow, colours.markupGreen, colours.markupPink,
             QColor(0x2F, 0x86, 0xFF),  QColor(0xE5, 0x3E, 0x3E), QColor(0x1B, 0x1F, 0x27) };
}

QPixmap swatchPixmap(const QColor &colour, bool selected)
{
    const qreal ratio = 2.0;
    QPixmap pixmap(QSize(swatchSize, swatchSize) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF box(2, 2, swatchSize - 4, swatchSize - 4);
    painter.setBrush(colour);
    // A selected swatch gets a ring in the text colour: a tick would disappear on
    // the darker presets.
    painter.setPen(QPen(selected ? themePalette().text : themePalette().borderStrong,
                        selected ? 2.0 : 1.0));
    painter.drawEllipse(box);
    return pixmap;
}

} // namespace

ToolOptionsPanel::ToolOptionsPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ToolOptions"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(12, 10, 12, 12);
    column->setSpacing(8);

    auto *heading = new QHBoxLayout;
    heading->setSpacing(8);
    m_icon = new QLabel(this);
    heading->addWidget(m_icon, 0, Qt::AlignVCenter);
    m_name = new QLabel(this);
    m_name->setFont(themeFont(TextRole::Title));
    heading->addWidget(m_name, 1);
    column->addLayout(heading);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_hint->setFont(themeFont(TextRole::Small));
    column->addWidget(m_hint);

    m_body = new QVBoxLayout;
    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setSpacing(8);
    column->addLayout(m_body);

    connect(&Theme::instance(), &Theme::changed, this, &ToolOptionsPanel::retint);
    retint();
    rebuild();
}

void ToolOptionsPanel::setSession(core::DocumentSession *session)
{
    if (m_session == session)
        return;

    if (m_session)
        m_session->disconnect(this);

    m_session = session;
    if (m_session) {
        connect(m_session, &core::DocumentSession::toolChanged, this, [this](core::Tool tool) {
            m_tool = tool;
            rebuild();
        });
        connect(m_session, &core::DocumentSession::markupStyleChanged, this,
                &ToolOptionsPanel::applyStyleFromSession);
        m_tool = m_session->tool();
    } else {
        m_tool = core::Tool::Select;
    }

    rebuild();
}

void ToolOptionsPanel::rebuild()
{
    // The body is rebuilt rather than shown and hidden piecemeal: each tool needs
    // a different set, and stale controls left behind are how a panel ends up
    // offering opacity for a tool that ignores it.
    while (QLayoutItem *item = m_body->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_swatches.clear();
    m_endingButtons.clear();
    m_stampButtons.clear();
    m_opacity = nullptr;
    m_opacityValue = nullptr;
    m_borderWidth = nullptr;
    m_borderWidthValue = nullptr;

    const ToolInfo *info = infoFor(m_tool);
    m_hasOptions = info != nullptr;

    // Select and Hand have nothing to configure. The inspector hides this panel
    // then, rather than keeping a box of placeholder text on screen for the tool
    // people spend most of their time in.
    if (!info) {
        Q_EMIT optionsChanged();
        return;
    }

    m_name->setText(tr(info->name));
    m_hint->setText(tr(info->hint));
    m_icon->setProperty(iconNameProperty, QString::fromLatin1(info->icon));

    if (isShapeFamily(m_tool))
        m_body->addWidget(buildShapePresets(this));
    if (m_tool == Tool::Stamp)
        m_body->addWidget(buildStampPresets(this));
    if (info->colour)
        m_body->addWidget(buildColourRow(this));
    if (info->opacity)
        m_body->addWidget(buildOpacityRow(this));
    if (info->border)
        m_body->addWidget(buildBorderWidthRow(this));
    if (m_tool == Tool::Arrow)
        m_body->addWidget(buildArrowEndingRow(this));

    if (m_tool == Tool::Redact) {
        m_body->addWidget(buildCommandButton(QStringLiteral("applyRedactions"),
                                             tr("Apply redactions"), QStringLiteral("redact"),
                                             this));
    }
    if (info->colour || m_tool == Tool::Stamp) {
        m_body->addWidget(buildCommandButton(QStringLiteral("deleteAnnotation"),
                                             tr("Delete selection"),
                                             QStringLiteral("trash"), this));
    }

    applyStyleFromSession();
    retint();
    Q_EMIT optionsChanged();
}

QWidget *ToolOptionsPanel::buildColourRow(QWidget *parent)
{
    auto *holder = new QWidget(parent);
    auto *column = new QVBoxLayout(holder);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);

    auto *caption = new QLabel(tr("Colour"), holder);
    caption->setObjectName(QStringLiteral("SectionLabel"));
    column->addWidget(caption);

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);

    for (const QColor &colour : presetColours()) {
        auto *swatch = new QToolButton(holder);
        swatch->setObjectName(QStringLiteral("ColourSwatch"));
        swatch->setCheckable(true);
        swatch->setAutoRaise(true);
        swatch->setIconSize(QSize(swatchSize, swatchSize));
        swatch->setToolTip(colour.name());
        swatch->setProperty("sweetColour", colour);
        connect(swatch, &QToolButton::clicked, this, [this, colour] {
            if (m_session)
                m_session->setMarkupColor(colour);
        });
        row->addWidget(swatch);
        m_swatches.append(swatch);
    }

    auto *more = new QToolButton(holder);
    more->setObjectName(QStringLiteral("ColourSwatch"));
    more->setText(QStringLiteral("…"));
    more->setToolTip(tr("Choose another colour"));
    more->setAutoRaise(true);
    connect(more, &QToolButton::clicked, this, [this] {
        if (!m_session)
            return;
        const QColor chosen =
            QColorDialog::getColor(m_session->markupColor(), this, tr("Markup colour"));
        if (chosen.isValid())
            m_session->setMarkupColor(chosen);
    });
    row->addWidget(more);
    row->addStretch(1);
    column->addLayout(row);
    return holder;
}

QWidget *ToolOptionsPanel::buildOpacityRow(QWidget *parent)
{
    auto *holder = new QWidget(parent);
    auto *column = new QVBoxLayout(holder);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);

    auto *heading = new QHBoxLayout;
    heading->setContentsMargins(0, 0, 0, 0);
    auto *caption = new QLabel(tr("Opacity"), holder);
    caption->setObjectName(QStringLiteral("SectionLabel"));
    heading->addWidget(caption, 1);
    m_opacityValue = new QLabel(holder);
    m_opacityValue->setFont(themeFont(TextRole::Caption));
    heading->addWidget(m_opacityValue, 0, Qt::AlignRight);
    column->addLayout(heading);

    m_opacity = new QSlider(Qt::Horizontal, holder);
    m_opacity->setRange(10, 100);
    m_opacity->setToolTip(tr("Opacity of new highlights and shapes"));
    connect(m_opacity, &QSlider::valueChanged, this, [this](int value) {
        if (m_opacityValue)
            m_opacityValue->setText(tr("%1%").arg(value));
        if (m_session)
            m_session->setMarkupOpacity(value / 100.0);
    });
    column->addWidget(m_opacity);
    return holder;
}

QWidget *ToolOptionsPanel::buildBorderWidthRow(QWidget *parent)
{
    auto *holder = new QWidget(parent);
    auto *column = new QVBoxLayout(holder);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);

    auto *heading = new QHBoxLayout;
    heading->setContentsMargins(0, 0, 0, 0);
    auto *caption = new QLabel(tr("Border"), holder);
    caption->setObjectName(QStringLiteral("SectionLabel"));
    heading->addWidget(caption, 1);
    m_borderWidthValue = new QLabel(holder);
    m_borderWidthValue->setFont(themeFont(TextRole::Caption));
    heading->addWidget(m_borderWidthValue, 0, Qt::AlignRight);
    column->addLayout(heading);

    m_borderWidth = new QSlider(Qt::Horizontal, holder);
    m_borderWidth->setRange(1, 24); // half-points: 0.5 … 12
    m_borderWidth->setToolTip(tr("Stroke width for new shapes and lines"));
    connect(m_borderWidth, &QSlider::valueChanged, this, [this](int value) {
        const qreal points = value / 2.0;
        if (m_borderWidthValue)
            m_borderWidthValue->setText(tr("%1 pt").arg(points, 0, 'f', 1));
        if (m_session)
            m_session->setMarkupBorderWidth(points);
    });
    column->addWidget(m_borderWidth);
    return holder;
}

QWidget *ToolOptionsPanel::buildShapePresets(QWidget *parent)
{
    auto *holder = new QWidget(parent);
    auto *column = new QVBoxLayout(holder);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);

    auto *caption = new QLabel(tr("Shape"), holder);
    caption->setObjectName(QStringLiteral("SectionLabel"));
    column->addWidget(caption);

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);

    const struct {
        const char *actionId;
        const char *icon;
        const char *tip;
        Tool tool;
    } presets[] = {
        { "addShape", "shape", "Rectangle", Tool::Shape },
        { "addEllipse", "ellipse", "Ellipse", Tool::Ellipse },
        { "addLine", "line", "Line", Tool::Line },
        { "addArrow", "arrow", "Arrow", Tool::Arrow },
    };

    for (const auto &preset : presets) {
        auto *chip = new QToolButton(holder);
        chip->setObjectName(QStringLiteral("ToolChip"));
        chip->setCheckable(true);
        chip->setChecked(m_tool == preset.tool);
        chip->setAutoRaise(true);
        chip->setIconSize(QSize(18, 18));
        chip->setToolTip(tr(preset.tip));
        chip->setProperty(iconNameProperty, QString::fromLatin1(preset.icon));
        const QString actionId = QString::fromLatin1(preset.actionId);
        connect(chip, &QToolButton::clicked, this,
                [this, actionId] { Q_EMIT commandRequested(actionId); });
        row->addWidget(chip);
    }
    row->addStretch(1);
    column->addLayout(row);
    return holder;
}

QWidget *ToolOptionsPanel::buildArrowEndingRow(QWidget *parent)
{
    auto *holder = new QWidget(parent);
    auto *column = new QVBoxLayout(holder);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);

    auto *caption = new QLabel(tr("Arrowhead"), holder);
    caption->setObjectName(QStringLiteral("SectionLabel"));
    column->addWidget(caption);

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);

    const struct {
        LineEnding style;
        const char *label;
    } endings[] = {
        { LineEnding::None, "None" },
        { LineEnding::OpenArrow, "Open" },
        { LineEnding::ClosedArrow, "Closed" },
    };

    for (const auto &ending : endings) {
        auto *button = new QToolButton(holder);
        button->setObjectName(QStringLiteral("ToolChip"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setText(tr(ending.label));
        button->setProperty("sweetLineEnding", static_cast<int>(ending.style));
        connect(button, &QToolButton::clicked, this, [this, style = ending.style] {
            if (m_session)
                m_session->setLineEndStyle(style);
        });
        row->addWidget(button);
        m_endingButtons.append(button);
    }
    row->addStretch(1);
    column->addLayout(row);
    return holder;
}

QWidget *ToolOptionsPanel::buildStampPresets(QWidget *parent)
{
    auto *holder = new QWidget(parent);
    auto *column = new QVBoxLayout(holder);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);

    auto *caption = new QLabel(tr("Stamp"), holder);
    caption->setObjectName(QStringLiteral("SectionLabel"));
    column->addWidget(caption);

    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(4);

    const char *names[] = { "Draft", "Approved", "Confidential", "Final" };
    int index = 0;
    for (const char *name : names) {
        auto *chip = new QToolButton(holder);
        chip->setObjectName(QStringLiteral("ToolChip"));
        chip->setCheckable(true);
        chip->setAutoRaise(true);
        chip->setText(QString::fromLatin1(name));
        chip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        chip->setProperty("sweetStampName", QString::fromLatin1(name));
        connect(chip, &QToolButton::clicked, this, [this, name = QString::fromLatin1(name)] {
            if (!m_session)
                return;
            m_session->setStampIconName(name);
            Q_EMIT commandRequested(QStringLiteral("toolStamp"));
        });
        grid->addWidget(chip, index / 2, index % 2);
        m_stampButtons.append(chip);
        ++index;
    }
    column->addLayout(grid);
    return holder;
}

QWidget *ToolOptionsPanel::buildCommandButton(const QString &actionId, const QString &text,
                                              const QString &iconName, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setProperty(iconNameProperty, iconName);
    button->setCursor(Qt::PointingHandCursor);
    connect(button, &QPushButton::clicked, this,
            [this, actionId] { Q_EMIT commandRequested(actionId); });
    return button;
}

void ToolOptionsPanel::applyStyleFromSession()
{
    if (!m_session)
        return;

    const QColor current = m_session->markupColor();
    for (QToolButton *swatch : m_swatches) {
        const QColor colour = swatch->property("sweetColour").value<QColor>();
        const bool selected = colour.rgb() == current.rgb();
        swatch->setChecked(selected);
        swatch->setIcon(swatchPixmap(colour, selected));
    }

    if (m_opacity) {
        const int percent = qRound(m_session->markupOpacity() * 100.0);
        QSignalBlocker blocker(m_opacity);
        m_opacity->setValue(percent);
        if (m_opacityValue)
            m_opacityValue->setText(tr("%1%").arg(percent));
    }

    if (m_borderWidth) {
        const int ticks = qRound(m_session->markupBorderWidth() * 2.0);
        QSignalBlocker blocker(m_borderWidth);
        m_borderWidth->setValue(ticks);
        if (m_borderWidthValue)
            m_borderWidthValue->setText(
                tr("%1 pt").arg(m_session->markupBorderWidth(), 0, 'f', 1));
    }

    const LineEnding ending = m_session->lineEndStyle();
    for (QToolButton *button : m_endingButtons) {
        const auto style = static_cast<LineEnding>(button->property("sweetLineEnding").toInt());
        button->setChecked(style == ending);
    }

    const QString stamp = m_session->stampIconName();
    for (QToolButton *button : m_stampButtons) {
        const QString name = button->property("sweetStampName").toString();
        button->setChecked(name == stamp);
    }
}

void ToolOptionsPanel::retint()
{
    const Palette &colours = themePalette();
    m_name->setStyleSheet(QStringLiteral("color: %1;").arg(colours.text.name()));
    m_hint->setStyleSheet(QStringLiteral("color: %1;").arg(colours.textMuted.name()));

    const QString name = m_icon->property(iconNameProperty).toString();
    if (!name.isEmpty())
        m_icon->setPixmap(icon(name).pixmap(20, 20));

    // Icons on the generated controls are tinted here too, so a theme flip does
    // not leave dark glyphs on a dark panel.
    for (QObject *child : findChildren<QObject *>()) {
        const QString childIcon = child->property(iconNameProperty).toString();
        if (childIcon.isEmpty())
            continue;
        if (auto *button = qobject_cast<QPushButton *>(child))
            button->setIcon(icon(childIcon));
        else if (auto *tool = qobject_cast<QToolButton *>(child))
            tool->setIcon(icon(childIcon));
    }

    applyStyleFromSession();
}

} // namespace sweet::ui
