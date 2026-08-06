#pragma once

#include <QColor>
#include <QFont>
#include <QHash>
#include <QIcon>
#include <QObject>
#include <QString>

class QWidget;

namespace sweet::ui {

enum class ThemeMode { Light, Dark };

/// Every colour the interface is allowed to use. Widgets never hardcode a
/// colour: they read it from here, so both themes stay consistent and a third
/// theme needs nothing but one more palette.
struct Palette
{
    QColor window;       ///< Chrome behind ribbon, docks and bars.
    QColor surface;      ///< Cards, panels, popups.
    QColor surfaceAlt;   ///< Slightly recessed areas inside a surface.
    QColor canvas;       ///< The area the pages float on.
    QColor border;       ///< Hairlines and panel separators.
    QColor borderStrong; ///< Control outlines, scrollbar handles.
    QColor text;
    QColor textMuted;
    QColor textInverted;
    QColor accent;       ///< Primary action and selection colour.
    QColor accentHover;
    QColor accentSoft;   ///< Tinted background for checked tools.
    QColor hover;
    QColor pressed;
    QColor markupYellow; ///< Default annotation colours.
    QColor markupGreen;
    QColor markupPink;
    QColor shadow;       ///< Base shadow colour, alpha included.
};

/// Named type sizes. Using the role instead of a literal point size keeps the
/// window readable when the platform font or DPI changes.
enum class TextRole {
    Display,      ///< Welcome screen headline.
    Title,        ///< Panel and dialog titles.
    Body,         ///< Default UI text.
    Small,        ///< Ribbon labels, status bar.
    Caption,      ///< Group captions, page numbers.
    SectionLabel, ///< Uppercase tracking labels inside inspectors.
};

/// Shadow depth for floating surfaces.
enum class Elevation {
    None,
    Low,  ///< Cards, popovers.
    High, ///< Floating toolbar over content.
};

/// Application-wide look: palette, typography, generated stylesheet and tinted
/// icons. One instance, reachable from anywhere, emitting changed() when the
/// mode flips so widgets can re-tint whatever the stylesheet cannot reach.
class Theme : public QObject
{
    Q_OBJECT

public:
    static Theme &instance();

    ThemeMode mode() const noexcept { return m_mode; }
    void setMode(ThemeMode mode);
    void toggleMode();

    const Palette &palette() const noexcept { return m_palette; }

    /// Icon tinted for the current theme, with muted disabled and accented
    /// checked variants. Cached per name.
    QIcon icon(const QString &name) const;

    /// Icon in one explicit colour, for swatches and badges.
    QIcon icon(const QString &name, const QColor &color) const;

    QFont font(TextRole role) const;

    /// Complete stylesheet for the current palette.
    QString styleSheet() const;

    /// Applies palette, font and stylesheet to the running application.
    void apply();

    /// Gives `widget` a drop shadow. Qt stylesheets cannot express shadows, so
    /// floating surfaces get one through a graphics effect instead.
    static void setElevation(QWidget *widget, Elevation elevation);

Q_SIGNALS:
    void changed();

private:
    Theme();

    /// First installed family from a preferred list, so the window looks native
    /// on each platform without bundling a font.
    static QString resolveFontFamily();

    ThemeMode m_mode = ThemeMode::Light;
    Palette m_palette;
    QString m_fontFamily;
    qreal m_basePointSize = 10.0;
    mutable QHash<QString, QIcon> m_iconCache;
};

/// Shorthand for Theme::instance().palette(). Deliberately not named palette():
/// inside a QWidget subclass that name would resolve to QWidget::palette().
inline const Palette &themePalette()
{
    return Theme::instance().palette();
}

/// Shorthand for Theme::instance().icon(name).
inline QIcon icon(const QString &name)
{
    return Theme::instance().icon(name);
}

/// Shorthand for Theme::instance().font(role).
inline QFont themeFont(TextRole role)
{
    return Theme::instance().font(role);
}

} // namespace sweet::ui
