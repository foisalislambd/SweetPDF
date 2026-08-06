#pragma once

#include <QColor>
#include <QHash>
#include <QIcon>
#include <QObject>
#include <QString>

namespace sweet::ui {

enum class ThemeMode { Light, Dark };

/// Every colour the interface is allowed to use. Widgets never hardcode a
/// colour: they read it from here so both themes stay consistent and a third
/// theme only needs one new palette.
struct Palette
{
    QColor window;      ///< Chrome behind ribbons, docks and bars.
    QColor surface;     ///< Cards, panels, popups.
    QColor surfaceAlt;  ///< Slightly recessed areas inside a surface.
    QColor canvas;      ///< The area the pages float on.
    QColor border;      ///< Hairlines and panel separators.
    QColor borderStrong;
    QColor text;
    QColor textMuted;
    QColor textInverted;
    QColor accent;      ///< Primary action / selection colour.
    QColor accentHover;
    QColor accentSoft;  ///< Tinted background for checked tool buttons.
    QColor hover;
    QColor pressed;
    QColor highlightYellow; ///< Default markup colours.
    QColor shadow;
};

/// Application-wide look: palette, generated stylesheet and tinted icons.
class Theme : public QObject
{
    Q_OBJECT

public:
    static Theme &instance();

    ThemeMode mode() const noexcept { return m_mode; }
    void setMode(ThemeMode mode);
    void toggleMode();

    const Palette &palette() const noexcept { return m_palette; }

    /// Icon tinted for the current theme, including muted disabled and accented
    /// checked variants. Cached per name.
    QIcon icon(const QString &name) const;

    /// Icon in one explicit colour, for badges and colour swatches.
    QIcon icon(const QString &name, const QColor &color) const;

    /// Complete stylesheet for the current palette.
    QString styleSheet() const;

    /// Applies palette, fonts and stylesheet to the running application.
    void apply();

Q_SIGNALS:
    void changed();

private:
    Theme();

    ThemeMode m_mode = ThemeMode::Light;
    Palette m_palette;
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

} // namespace sweet::ui
