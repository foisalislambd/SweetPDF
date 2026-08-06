#pragma once

#include <QString>
#include <QStringList>

namespace sweet::ui::icons {

/// SVG markup for `name`, or an empty string when the name is unknown.
///
/// Two placeholders are left for Theme to substitute: `@C` for the stroke colour
/// and `fill='@F'` for the optional tinted fill that gives some icons weight.
/// Icons live in the binary rather than as resource files so one glyph can be
/// tinted per theme, per widget state and per accent colour without shipping a
/// separate file for each combination.
QString markup(const QString &name);

/// Names of every available icon, sorted.
QStringList available();

} // namespace sweet::ui::icons
