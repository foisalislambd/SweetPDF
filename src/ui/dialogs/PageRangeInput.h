#pragma once

#include <QList>
#include <QString>

namespace sweet::ui {

/// Parses page range text of the form "1-4, 8, 11-13" into zero-based indices.
///
/// Ranges are what people type into a print dialog, so every dialog that needs a
/// page set uses this instead of inventing its own syntax. Out-of-range and
/// reversed entries are dropped rather than rejected, so a typo narrows the
/// result instead of blocking the dialog.
QList<int> parsePageRange(const QString &text, int pageCount);

/// The inverse, used to show a selection as text: "1-4, 8".
QString formatPageRange(const QList<int> &pages);

} // namespace sweet::ui
