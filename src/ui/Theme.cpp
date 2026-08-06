#include "ui/Theme.h"

#include "ui/IconLibrary.h"

#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace sweet::ui {
namespace {

Palette lightPalette()
{
    Palette p;
    p.window = QColor(0xFF, 0xFF, 0xFF);
    p.surface = QColor(0xFF, 0xFF, 0xFF);
    p.surfaceAlt = QColor(0xF4, 0xF6, 0xFA);
    p.canvas = QColor(0xE7, 0xEA, 0xF0);
    p.border = QColor(0xE3, 0xE7, 0xEE);
    p.borderStrong = QColor(0xC9, 0xD0, 0xDB);
    p.text = QColor(0x1B, 0x22, 0x2E);
    p.textMuted = QColor(0x6B, 0x74, 0x84);
    p.textInverted = QColor(0xFF, 0xFF, 0xFF);
    p.accent = QColor(0x25, 0x63, 0xEB);
    p.accentHover = QColor(0x1D, 0x4E, 0xD8);
    p.accentSoft = QColor(0xE5, 0xED, 0xFD);
    p.hover = QColor(0xEE, 0xF1, 0xF6);
    p.pressed = QColor(0xDF, 0xE4, 0xEC);
    p.highlightYellow = QColor(0xFF, 0xE2, 0x4D);
    p.shadow = QColor(0x0F, 0x17, 0x2A, 0x2E);
    return p;
}

Palette darkPalette()
{
    Palette p;
    p.window = QColor(0x1E, 0x21, 0x27);
    p.surface = QColor(0x23, 0x27, 0x2E);
    p.surfaceAlt = QColor(0x2A, 0x2F, 0x38);
    p.canvas = QColor(0x14, 0x16, 0x1A);
    p.border = QColor(0x33, 0x38, 0x42);
    p.borderStrong = QColor(0x44, 0x4A, 0x56);
    p.text = QColor(0xE6, 0xE9, 0xEF);
    p.textMuted = QColor(0x99, 0xA1, 0xB0);
    p.textInverted = QColor(0xFF, 0xFF, 0xFF);
    p.accent = QColor(0x4B, 0x8B, 0xFF);
    p.accentHover = QColor(0x69, 0x9F, 0xFF);
    p.accentSoft = QColor(0x25, 0x33, 0x4D);
    p.hover = QColor(0x2E, 0x34, 0x3E);
    p.pressed = QColor(0x38, 0x3F, 0x4B);
    p.highlightYellow = QColor(0xF5, 0xD0, 0x42);
    p.shadow = QColor(0x00, 0x00, 0x00, 0x66);
    return p;
}

QPixmap renderIcon(const QString &name, const QColor &color, int size, qreal ratio)
{
    QString markup = icons::markup(name);
    if (markup.isEmpty())
        return {};
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
    static const int sizes[] = { 16, 18, 20, 24, 28, 32, 48 };
    for (const int size : sizes) {
        for (const qreal ratio : { 1.0, 2.0 }) {
            const QPixmap pixmap = renderIcon(name, color, size, ratio);
            if (!pixmap.isNull())
                target.addPixmap(pixmap, mode, QIcon::Off);
        }
    }
}

QString applyTokens(QString sheet, const Palette &p)
{
    const QHash<QString, QColor> tokens = {
        { QStringLiteral("window"), p.window },
        { QStringLiteral("surface"), p.surface },
        { QStringLiteral("surfaceAlt"), p.surfaceAlt },
        { QStringLiteral("canvas"), p.canvas },
        { QStringLiteral("border"), p.border },
        { QStringLiteral("borderStrong"), p.borderStrong },
        { QStringLiteral("text"), p.text },
        { QStringLiteral("textMuted"), p.textMuted },
        { QStringLiteral("textInverted"), p.textInverted },
        { QStringLiteral("accent"), p.accent },
        { QStringLiteral("accentHover"), p.accentHover },
        { QStringLiteral("accentSoft"), p.accentSoft },
        { QStringLiteral("hover"), p.hover },
        { QStringLiteral("pressed"), p.pressed },
    };

    for (auto it = tokens.cbegin(); it != tokens.cend(); ++it)
        sheet.replace(QLatin1Char('{') + it.key() + QLatin1Char('}'), it.value().name());
    return sheet;
}

} // namespace

Theme::Theme()
    : m_palette(lightPalette())
{
}

Theme &Theme::instance()
{
    static Theme theme;
    return theme;
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

void Theme::apply()
{
    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app)
        return;

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

QWidget { color: {text}; }
QMainWindow, QDialog { background: {window}; }
QToolTip {
    background: {surface}; color: {text}; border: 1px solid {border};
    border-radius: 6px; padding: 5px 8px;
}

/* --- Menu bar ------------------------------------------------------------ */
QMenuBar { background: {window}; border: none; padding: 2px 6px; }
QMenuBar::item { padding: 5px 10px; border-radius: 6px; background: transparent; }
QMenuBar::item:selected { background: {hover}; }
QMenuBar::item:pressed { background: {pressed}; }

QMenu {
    background: {surface}; border: 1px solid {border};
    border-radius: 10px; padding: 6px;
}
QMenu::item { padding: 7px 26px 7px 30px; border-radius: 7px; }
QMenu::item:selected { background: {accentSoft}; color: {accent}; }
QMenu::item:disabled { color: {textMuted}; }
QMenu::separator { height: 1px; background: {border}; margin: 5px 8px; }
QMenu::icon { left: 8px; }

/* --- Ribbon -------------------------------------------------------------- */
#RibbonBar { background: {window}; border-bottom: 1px solid {border}; }
#RibbonTabStrip { background: transparent; }
#RibbonPage { background: {window}; }
#RibbonGroupTitle { color: {textMuted}; font-size: 10px; }
#RibbonGroupSeparator { background: {border}; }

QPushButton#RibbonTab {
    background: transparent; border: none; border-radius: 7px;
    padding: 6px 14px; color: {textMuted}; font-weight: 500;
}
QPushButton#RibbonTab:hover { background: {hover}; color: {text}; }
QPushButton#RibbonTab:checked { color: {accent}; background: {accentSoft}; font-weight: 600; }

QToolButton {
    background: transparent; border: 1px solid transparent;
    border-radius: 8px; padding: 4px; color: {text};
}
QToolButton:hover { background: {hover}; }
QToolButton:pressed { background: {pressed}; }
QToolButton:checked { background: {accentSoft}; border-color: {accent}; color: {accent}; }
QToolButton:disabled { color: {textMuted}; }
QToolButton::menu-indicator { image: none; }

QToolButton#RibbonAction { padding: 6px 8px; }

/* --- Docks and panels ---------------------------------------------------- */
QDockWidget { background: {window}; titlebar-close-icon: none; titlebar-normal-icon: none; }
QDockWidget::title { background: transparent; padding: 0; }

#PanelHeader { background: {window}; border-bottom: 1px solid {border}; }
#PanelTitle { font-weight: 600; }
#SidebarRail { background: {window}; border-right: 1px solid {border}; }
#Sidebar { background: {window}; border-right: 1px solid {border}; }
#PropertiesPanel { background: {window}; border-left: 1px solid {border}; }
#SectionLabel { color: {textMuted}; font-size: 11px; font-weight: 600; }

QScrollArea { border: none; background: transparent; }

/* --- Document tabs ------------------------------------------------------- */
QTabWidget::pane { border: none; background: {canvas}; }
QTabBar { background: {window}; qproperty-drawBase: 0; }
QTabBar::tab {
    background: transparent; color: {textMuted};
    border: 1px solid transparent; border-top-left-radius: 8px;
    border-top-right-radius: 8px; padding: 7px 14px; margin-right: 2px;
}
QTabBar::tab:hover { background: {hover}; color: {text}; }
QTabBar::tab:selected {
    background: {canvas}; color: {text};
    border-color: {border}; border-bottom-color: {canvas};
}
QTabBar::close-button { subcontrol-position: right; margin-left: 6px; }

/* --- Lists and trees ----------------------------------------------------- */
QListView, QTreeView {
    background: {window}; border: none; outline: none;
    show-decoration-selected: 1;
}
QListView::item, QTreeView::item {
    border-radius: 7px; padding: 3px; margin: 1px 4px; color: {text};
}
QListView::item:hover, QTreeView::item:hover { background: {hover}; }
QListView::item:selected, QTreeView::item:selected {
    background: {accentSoft}; color: {accent};
}
QTreeView::branch { background: transparent; }
QHeaderView::section {
    background: {surfaceAlt}; color: {textMuted};
    border: none; border-bottom: 1px solid {border}; padding: 5px 8px;
}

/* --- Inputs -------------------------------------------------------------- */
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
    background: {surface}; border: 1px solid {border};
    border-radius: 8px; padding: 5px 9px; selection-background-color: {accent};
    selection-color: {textInverted}; min-height: 20px;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus,
QPlainTextEdit:focus, QTextEdit:focus { border-color: {accent}; }
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled { color: {textMuted}; }

QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background: {surface}; border: 1px solid {border};
    border-radius: 8px; padding: 4px; selection-background-color: {accentSoft};
    selection-color: {accent};
}
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
    background: transparent; border: none; width: 16px;
}

QPushButton {
    background: {surface}; border: 1px solid {border};
    border-radius: 8px; padding: 6px 14px; min-height: 20px;
}
QPushButton:hover { background: {hover}; }
QPushButton:pressed { background: {pressed}; }
QPushButton:disabled { color: {textMuted}; }
QPushButton[accent="true"] {
    background: {accent}; color: {textInverted}; border-color: {accent}; font-weight: 600;
}
QPushButton[accent="true"]:hover { background: {accentHover}; border-color: {accentHover}; }

QCheckBox, QRadioButton { spacing: 8px; }
QCheckBox::indicator, QRadioButton::indicator {
    width: 15px; height: 15px; border: 1px solid {borderStrong};
    background: {surface}; border-radius: 4px;
}
QRadioButton::indicator { border-radius: 8px; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked {
    background: {accent}; border-color: {accent};
}

/* --- Sliders and scrollbars --------------------------------------------- */
QSlider::groove:horizontal {
    height: 4px; background: {border}; border-radius: 2px;
}
QSlider::sub-page:horizontal { background: {accent}; border-radius: 2px; }
QSlider::handle:horizontal {
    width: 14px; height: 14px; margin: -6px 0; border-radius: 7px;
    background: {surface}; border: 2px solid {accent};
}

QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 2px; }
QScrollBar::handle {
    background: {borderStrong}; border-radius: 4px; min-height: 32px; min-width: 32px;
}
QScrollBar::handle:hover { background: {textMuted}; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* --- Status bar and overlays -------------------------------------------- */
QStatusBar { background: {window}; border-top: 1px solid {border}; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: {textMuted}; }

#FloatingBar {
    background: {surface}; border: 1px solid {border}; border-radius: 12px;
}
#PageCanvas { background: {canvas}; border: none; }
QSplitter::handle { background: {border}; width: 1px; height: 1px; }
QProgressBar {
    background: {surfaceAlt}; border: none; border-radius: 4px;
    height: 6px; text-align: center; color: transparent;
}
QProgressBar::chunk { background: {accent}; border-radius: 4px; }
)");

    return applyTokens(sheet, m_palette);
}

} // namespace sweet::ui
