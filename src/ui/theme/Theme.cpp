#include "ui/theme/Theme.h"

#include "ui/theme/IconLibrary.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSvgRenderer>
#include <QWidget>

namespace sweet::ui {
namespace {

Palette lightPalette()
{
    Palette p;
    p.window = QColor(0xFC, 0xFD, 0xFE);
    p.surface = QColor(0xFF, 0xFF, 0xFF);
    p.surfaceAlt = QColor(0xF4, 0xF6, 0xFA);
    p.canvas = QColor(0xE8, 0xEB, 0xF1);
    p.border = QColor(0xE4, 0xE8, 0xEF);
    p.borderStrong = QColor(0xC6, 0xCE, 0xDA);
    p.text = QColor(0x18, 0x1F, 0x2B);
    p.textMuted = QColor(0x6A, 0x74, 0x86);
    p.textInverted = QColor(0xFF, 0xFF, 0xFF);
    p.accent = QColor(0x25, 0x63, 0xEB);
    p.accentHover = QColor(0x1B, 0x4C, 0xD4);
    p.accentSoft = QColor(0xE6, 0xEE, 0xFE);
    p.hover = QColor(0xEE, 0xF1, 0xF7);
    p.pressed = QColor(0xDE, 0xE4, 0xEE);
    p.markupYellow = QColor(0xFF, 0xDD, 0x4A);
    p.markupGreen = QColor(0x4A, 0xD8, 0x91);
    p.markupPink = QColor(0xFF, 0x76, 0xA8);
    p.shadow = QColor(0x0B, 0x14, 0x2B, 0x2B);
    return p;
}

Palette darkPalette()
{
    Palette p;
    p.window = QColor(0x1C, 0x1F, 0x26);
    p.surface = QColor(0x23, 0x27, 0x30);
    p.surfaceAlt = QColor(0x2A, 0x2F, 0x3A);
    p.canvas = QColor(0x12, 0x14, 0x18);
    p.border = QColor(0x32, 0x37, 0x42);
    p.borderStrong = QColor(0x48, 0x4F, 0x5C);
    p.text = QColor(0xE7, 0xEA, 0xF0);
    p.textMuted = QColor(0x96, 0x9F, 0xAF);
    p.textInverted = QColor(0xFF, 0xFF, 0xFF);
    p.accent = QColor(0x5A, 0x96, 0xFF);
    p.accentHover = QColor(0x7A, 0xAD, 0xFF);
    p.accentSoft = QColor(0x22, 0x31, 0x4C);
    p.hover = QColor(0x2E, 0x34, 0x40);
    p.pressed = QColor(0x39, 0x40, 0x4E);
    p.markupYellow = QColor(0xF2, 0xCB, 0x3E);
    p.markupGreen = QColor(0x3E, 0xC2, 0x80);
    p.markupPink = QColor(0xF2, 0x6A, 0x9A);
    p.shadow = QColor(0x00, 0x00, 0x00, 0x80);
    return p;
}

QPixmap renderIcon(const QString &name, const QColor &color, int size, qreal ratio)
{
    QString markup = icons::markup(name);
    if (markup.isEmpty())
        return {};

    QColor fill = color;
    fill.setAlphaF(0.16f);

    markup.replace(QLatin1String("fill='@F'"),
                   QStringLiteral("fill='%1' fill-opacity='%2'")
                       .arg(color.name(QColor::HexRgb))
                       .arg(fill.alphaF()));
    markup.replace(QLatin1String("@C"), color.name(QColor::HexRgb));

    QSvgRenderer renderer(markup.toUtf8());
    if (!renderer.isValid())
        return {};

    QPixmap pixmap(QSize(size, size) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size, size)));
    return pixmap;
}

void addVariant(QIcon &target, const QString &name, const QColor &color, QIcon::Mode mode)
{
    static const int sizes[] = { 16, 18, 20, 24, 28, 32, 48, 64 };
    for (const int size : sizes) {
        for (const qreal ratio : { 1.0, 2.0 }) {
            const QPixmap pixmap = renderIcon(name, color, size, ratio);
            if (!pixmap.isNull())
                target.addPixmap(pixmap, mode, QIcon::Off);
        }
    }
}

QString applyTokens(QString sheet, const Palette &p, const QString &fontFamily, qreal basePoint)
{
    const QHash<QString, QString> tokens = {
        { QStringLiteral("window"), p.window.name() },
        { QStringLiteral("surface"), p.surface.name() },
        { QStringLiteral("surfaceAlt"), p.surfaceAlt.name() },
        { QStringLiteral("canvas"), p.canvas.name() },
        { QStringLiteral("border"), p.border.name() },
        { QStringLiteral("borderStrong"), p.borderStrong.name() },
        { QStringLiteral("text"), p.text.name() },
        { QStringLiteral("textMuted"), p.textMuted.name() },
        { QStringLiteral("textInverted"), p.textInverted.name() },
        { QStringLiteral("accent"), p.accent.name() },
        { QStringLiteral("accentHover"), p.accentHover.name() },
        { QStringLiteral("accentSoft"), p.accentSoft.name() },
        { QStringLiteral("hover"), p.hover.name() },
        { QStringLiteral("pressed"), p.pressed.name() },
        { QStringLiteral("fontFamily"), fontFamily },
        { QStringLiteral("fontBody"), QString::number(basePoint, 'f', 1) },
        { QStringLiteral("fontSmall"), QString::number(basePoint - 0.5, 'f', 1) },
        { QStringLiteral("fontCaption"), QString::number(basePoint - 1.5, 'f', 1) },
    };

    for (auto it = tokens.cbegin(); it != tokens.cend(); ++it)
        sheet.replace(QLatin1Char('{') + it.key() + QLatin1Char('}'), it.value());
    return sheet;
}

} // namespace

Theme::Theme()
    : m_palette(lightPalette())
    , m_fontFamily(resolveFontFamily())
{
    const QFont base = QApplication::font();
    if (base.pointSizeF() > 0)
        m_basePointSize = qMax(9.5, base.pointSizeF());

    // The cache holds QPixmaps, and this object is a function-local static, so it
    // would otherwise be destroyed after QApplication - by which point the
    // platform plugin is gone and freeing a pixmap is undefined behaviour (an
    // access violation on Windows). A post routine runs at the start of
    // ~QCoreApplication, while the GUI stack is still up.
    qAddPostRoutine([] { Theme::instance().m_iconCache.clear(); });
}

Theme &Theme::instance()
{
    // Deliberately never destroyed. A QObject must not outlive QApplication, and
    // a function-local static object would be destroyed after it, once the
    // platform plugin and the application's thread data are already gone. The
    // pixmaps it caches are freed while the GUI is still up; see the constructor.
    static Theme *theme = new Theme;
    return *theme;
}

QString Theme::resolveFontFamily()
{
    // Ordered by how well each family suits a dense desktop UI. The first one
    // that is actually installed wins; otherwise Qt's default stays.
    static const QStringList preferred = {
        QStringLiteral("Inter"),          QStringLiteral("SF Pro Text"),
        QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI"),
        QStringLiteral("Ubuntu"),         QStringLiteral("Noto Sans"),
        QStringLiteral("Cantarell"),      QStringLiteral("DejaVu Sans"),
    };

    const QStringList installed = QFontDatabase::families();
    for (const QString &candidate : preferred) {
        if (installed.contains(candidate, Qt::CaseInsensitive))
            return candidate;
    }
    return QApplication::font().family();
}

void Theme::setMode(ThemeMode mode)
{
    if (mode == m_mode && !m_iconCache.isEmpty())
        return;

    m_mode = mode;
    m_palette = mode == ThemeMode::Dark ? darkPalette() : lightPalette();
    m_iconCache.clear();
    apply();
    Q_EMIT changed();
}

void Theme::toggleMode()
{
    setMode(m_mode == ThemeMode::Dark ? ThemeMode::Light : ThemeMode::Dark);
}

QIcon Theme::icon(const QString &name) const
{
    if (const auto it = m_iconCache.constFind(name); it != m_iconCache.cend())
        return it.value();

    QIcon result;
    addVariant(result, name, m_palette.text, QIcon::Normal);
    addVariant(result, name, m_palette.textMuted, QIcon::Disabled);
    addVariant(result, name, m_palette.accent, QIcon::Active);
    addVariant(result, name, m_palette.accent, QIcon::Selected);

    m_iconCache.insert(name, result);
    return result;
}

QIcon Theme::icon(const QString &name, const QColor &color) const
{
    QIcon result;
    addVariant(result, name, color, QIcon::Normal);
    addVariant(result, name, color, QIcon::Disabled);
    return result;
}

QFont Theme::font(TextRole role) const
{
    QFont result(m_fontFamily);
    result.setHintingPreference(QFont::PreferFullHinting);
    result.setPointSizeF(m_basePointSize);

    switch (role) {
    case TextRole::Display:
        result.setPointSizeF(m_basePointSize * 2.1);
        result.setWeight(QFont::DemiBold);
        break;
    case TextRole::Title:
        result.setPointSizeF(m_basePointSize + 0.5);
        result.setWeight(QFont::DemiBold);
        break;
    case TextRole::Body:
        break;
    case TextRole::Small:
        result.setPointSizeF(m_basePointSize - 0.5);
        break;
    case TextRole::Caption:
        result.setPointSizeF(m_basePointSize - 1.5);
        break;
    case TextRole::SectionLabel:
        result.setPointSizeF(m_basePointSize - 1.5);
        result.setWeight(QFont::DemiBold);
        result.setCapitalization(QFont::AllUppercase);
        result.setLetterSpacing(QFont::PercentageSpacing, 106);
        break;
    }
    return result;
}

void Theme::setElevation(QWidget *widget, Elevation elevation)
{
    if (!widget)
        return;

    if (elevation == Elevation::None) {
        widget->setGraphicsEffect(nullptr);
        return;
    }

    const Palette &colors = instance().palette();
    auto *shadow = new QGraphicsDropShadowEffect(widget);
    QColor tint = colors.shadow;

    if (elevation == Elevation::Low) {
        tint.setAlpha(qMin(255, colors.shadow.alpha() + 10));
        shadow->setBlurRadius(18);
        shadow->setOffset(0, 3);
    } else {
        tint.setAlpha(qMin(255, colors.shadow.alpha() + 30));
        shadow->setBlurRadius(34);
        shadow->setOffset(0, 8);
    }

    shadow->setColor(tint);
    widget->setGraphicsEffect(shadow);
}

void Theme::apply()
{
    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app)
        return;

    QApplication::setFont(font(TextRole::Body));

    QPalette qtPalette = app->palette();
    qtPalette.setColor(QPalette::Window, m_palette.window);
    qtPalette.setColor(QPalette::WindowText, m_palette.text);
    qtPalette.setColor(QPalette::Base, m_palette.surface);
    qtPalette.setColor(QPalette::AlternateBase, m_palette.surfaceAlt);
    qtPalette.setColor(QPalette::Text, m_palette.text);
    qtPalette.setColor(QPalette::PlaceholderText, m_palette.textMuted);
    qtPalette.setColor(QPalette::Button, m_palette.surface);
    qtPalette.setColor(QPalette::ButtonText, m_palette.text);
    qtPalette.setColor(QPalette::Highlight, m_palette.accent);
    qtPalette.setColor(QPalette::HighlightedText, m_palette.textInverted);
    qtPalette.setColor(QPalette::ToolTipBase, m_palette.surface);
    qtPalette.setColor(QPalette::ToolTipText, m_palette.text);
    qtPalette.setColor(QPalette::Link, m_palette.accent);
    qtPalette.setColor(QPalette::Disabled, QPalette::Text, m_palette.textMuted);
    qtPalette.setColor(QPalette::Disabled, QPalette::ButtonText, m_palette.textMuted);
    qtPalette.setColor(QPalette::Disabled, QPalette::WindowText, m_palette.textMuted);
    app->setPalette(qtPalette);
    app->setStyleSheet(styleSheet());
}

QString Theme::styleSheet() const
{
    static const QString sheet = QStringLiteral(R"(
* { outline: none; }

/* No font-size here: a size on the universal selector would beat every
   widget-level setFont, which is how the type scale in Theme::font is applied.
   The base size comes from QApplication::setFont instead. */
QWidget { color: {text}; font-family: "{fontFamily}"; }
QMainWindow, QDialog { background: {window}; }

QToolTip {
    background: {surface}; color: {text}; border: 1px solid {border};
    border-radius: 7px; padding: 5px 9px; font-size: {fontSmall}pt;
}

QMenu {
    background: {surface}; border: 1px solid {border};
    border-radius: 11px; padding: 6px;
}
QMenu::item { padding: 7px 28px 7px 32px; border-radius: 7px; }
QMenu::item:selected { background: {accentSoft}; color: {accent}; }
QMenu::item:disabled { color: {textMuted}; }
QMenu::separator { height: 1px; background: {border}; margin: 5px 10px; }
QMenu::icon { left: 9px; }
QMenu::right-arrow { width: 12px; height: 12px; }

/* --- Title strip and ribbon -------------------------------------------- */
#TitleStrip { background: {window}; }
#DocumentTitle { color: {text}; font-size: {fontBody}pt; font-weight: 600; }
#DocumentSubtitle { color: {textMuted}; font-size: {fontCaption}pt; }

#RibbonBar { background: {window}; border-bottom: 1px solid {border}; }
#RibbonTabStrip { background: transparent; }
#RibbonPage { background: {window}; }
#RibbonGroupTitle { color: {textMuted}; font-size: {fontCaption}pt; }
#RibbonGroupSeparator { background: {border}; }

QPushButton#RibbonTab {
    background: transparent; border: none; border-radius: 8px;
    padding: 6px 15px; color: {textMuted}; font-size: {fontSmall}pt; font-weight: 500;
}
QPushButton#RibbonTab:hover { background: {hover}; color: {text}; }
QPushButton#RibbonTab:checked {
    color: {accent}; background: {accentSoft}; font-weight: 600;
}

QToolButton {
    background: transparent; border: 1px solid transparent;
    border-radius: 9px; padding: 4px; color: {text};
}
QToolButton:hover { background: {hover}; }
QToolButton:pressed { background: {pressed}; }
QToolButton:checked { background: {accentSoft}; border-color: {accent}; color: {accent}; }
QToolButton:disabled { color: {textMuted}; }
QToolButton::menu-indicator { image: none; }
QToolButton#RibbonAction { padding: 6px 6px 5px 6px; font-size: {fontSmall}pt; }
QToolButton#RailButton {
    border-radius: 10px; font-size: {fontCaption}pt; color: {textMuted};
}
QToolButton#RailButton:hover { color: {text}; }
QToolButton#RailButton:checked { color: {accent}; }
QToolButton#AppMenu { padding: 5px 9px; font-weight: 600; }

/* --- Panels ------------------------------------------------------------- */
#PanelHeader { background: {window}; border-bottom: 1px solid {border}; }
#PanelTitle { font-size: {fontBody}pt; font-weight: 600; }
#SidebarRail { background: {window}; border-right: 1px solid {border}; }
#Sidebar { background: {window}; border-right: 1px solid {border}; }
#Inspector { background: {window}; border-left: 1px solid {border}; }
#PropertiesPanel { background: {window}; }
#SectionLabel { color: {textMuted}; font-size: {fontCaption}pt; font-weight: 600; }

/* The tool's own settings, sitting above the document properties. The tint marks
   it as being about what you are doing rather than about the file. */
#ToolOptions { background: {surfaceAlt}; border-bottom: 1px solid {border}; }
#ToolOptions QPushButton { padding: 5px 10px; text-align: left; }
QToolButton#ColourSwatch {
    border: none; border-radius: 4px; padding: 1px; min-width: 24px; min-height: 24px;
    font-weight: 600; color: {textMuted};
}
QToolButton#ColourSwatch:hover { background: {hover}; }
QToolButton#ColourSwatch:checked { background: {accentSoft}; }
#FieldLabel { color: {textMuted}; font-size: {fontSmall}pt; }
#FieldValue { font-size: {fontSmall}pt; }
#Divider { background: {border}; }

QScrollArea { border: none; background: transparent; }

/* --- Document tabs ------------------------------------------------------ */
QTabWidget::pane { border: none; background: {canvas}; }
QTabBar { background: {window}; qproperty-drawBase: 0; }
QTabBar::tab {
    background: transparent; color: {textMuted}; font-size: {fontSmall}pt;
    border: 1px solid transparent; border-top-left-radius: 9px;
    border-top-right-radius: 9px; padding: 7px 12px; margin-right: 2px;
}
QTabBar::tab:hover { background: {hover}; color: {text}; }
QTabBar::tab:selected {
    background: {canvas}; color: {text};
    border-color: {border}; border-bottom-color: {canvas};
}

/* --- Lists and trees ---------------------------------------------------- */
QListView, QTreeView {
    background: transparent; border: none; outline: none;
    show-decoration-selected: 1; font-size: {fontSmall}pt;
}
QTreeView::item { border-radius: 7px; padding: 4px; margin: 1px 5px; color: {text}; }
QTreeView::item:hover { background: {hover}; }
QTreeView::item:selected { background: {accentSoft}; color: {accent}; }
QTreeView::branch { background: transparent; }
QHeaderView::section {
    background: {surfaceAlt}; color: {textMuted};
    border: none; border-bottom: 1px solid {border}; padding: 5px 8px;
}

/* --- Inputs ------------------------------------------------------------- */
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
    background: {surface}; border: 1px solid {border};
    border-radius: 8px; padding: 4px 8px; selection-background-color: {accent};
    selection-color: {textInverted}; min-height: 20px; font-size: {fontSmall}pt;
}
QLineEdit:hover, QSpinBox:hover, QComboBox:hover { border-color: {borderStrong}; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus,
QPlainTextEdit:focus, QTextEdit:focus { border-color: {accent}; }
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled { color: {textMuted}; }

QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView {
    background: {surface}; border: 1px solid {border};
    border-radius: 9px; padding: 4px; selection-background-color: {accentSoft};
    selection-color: {accent};
}
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
    background: transparent; border: none; width: 14px;
}

QPushButton {
    background: {surface}; border: 1px solid {border};
    border-radius: 9px; padding: 7px 15px; min-height: 20px; font-size: {fontSmall}pt;
}
QPushButton:hover { background: {hover}; border-color: {borderStrong}; }
QPushButton:pressed { background: {pressed}; }
QPushButton:disabled { color: {textMuted}; }
QPushButton[accent="true"] {
    background: {accent}; color: {textInverted}; border-color: {accent}; font-weight: 600;
}
QPushButton[accent="true"]:hover { background: {accentHover}; border-color: {accentHover}; }
QPushButton[flat="true"] { background: transparent; border-color: transparent; }
QPushButton[flat="true"]:hover { background: {hover}; }

QCheckBox, QRadioButton { spacing: 8px; }
QCheckBox::indicator, QRadioButton::indicator {
    width: 15px; height: 15px; border: 1px solid {borderStrong};
    background: {surface}; border-radius: 4px;
}
QRadioButton::indicator { border-radius: 8px; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked {
    background: {accent}; border-color: {accent};
}

/* --- Sliders and scrollbars -------------------------------------------- */
QSlider::groove:horizontal { height: 4px; background: {border}; border-radius: 2px; }
QSlider::sub-page:horizontal { background: {accent}; border-radius: 2px; }
QSlider::handle:horizontal {
    width: 13px; height: 13px; margin: -5px 0; border-radius: 7px;
    background: {surface}; border: 2px solid {accent};
}
QSlider::handle:horizontal:hover { border-color: {accentHover}; }

QScrollBar:vertical { background: transparent; width: 11px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 11px; margin: 2px; }
QScrollBar::handle {
    background: {borderStrong}; border-radius: 4px; min-height: 36px; min-width: 36px;
}
QScrollBar::handle:hover { background: {textMuted}; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* --- Status bar and floating surfaces ---------------------------------- */
QStatusBar { background: {window}; border-top: 1px solid {border}; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: {textMuted}; font-size: {fontCaption}pt; }
#StatusValue { color: {text}; font-size: {fontCaption}pt; font-weight: 600; }

#FloatingBar {
    background: {surface}; border: 1px solid {border}; border-radius: 13px;
}
#PageCanvas { background: {canvas}; border: none; }

/* --- Home screen ------------------------------------------------------- */
#WelcomePanel {
    background: {surface}; border: 1px solid {border}; border-radius: 14px;
}
#WelcomeCard, #WelcomeTile {
    background: {surface}; border: 1px solid {border}; border-radius: 12px;
}
#WelcomeCard:hover, #WelcomeTile:hover { border-color: {accent}; background: {accentSoft}; }
#WelcomeDropZone {
    border: 2px dashed {borderStrong}; border-radius: 14px; background: transparent;
}
#VersionBadge {
    background: {accentSoft}; border-radius: 8px; padding: 2px 8px; font-weight: 600;
}
#StepBadge { background: {accent}; border-radius: 10px; font-weight: 600; }
#KeyChip {
    background: {surfaceAlt}; border: 1px solid {border}; border-radius: 5px;
    padding: 1px 6px; font-weight: 600;
}

QSplitter::handle { background: {border}; width: 1px; height: 1px; }
QSplitter::handle:hover { background: {accent}; }
QProgressBar {
    background: {surfaceAlt}; border: none; border-radius: 4px;
    height: 6px; text-align: center; color: transparent;
}
QProgressBar::chunk { background: {accent}; border-radius: 4px; }
)");

    return applyTokens(sheet, m_palette, m_fontFamily, m_basePointSize);
}

} // namespace sweet::ui
