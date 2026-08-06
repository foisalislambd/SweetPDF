#pragma once

#include <QString>
#include <QStringList>

namespace sweet::ui::icons {

/// SVG markup for `name`, with `@C` where the stroke colour belongs, or an
/// empty string when the name is unknown. Icons are kept in the binary rather
/// than as resource files so a single glyph can be tinted per theme, per state
/// and per accent colour without shipping duplicates.
QString markup(const QString &name);

/// Names of every available icon, sorted.
QStringList available();

} // namespace sweet::ui::icons
