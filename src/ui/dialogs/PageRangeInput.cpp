#include "ui/dialogs/PageRangeInput.h"

#include <QStringList>

#include <algorithm>

namespace sweet::ui {

QList<int> parsePageRange(const QString &text, int pageCount)
{
    QList<int> pages;
    if (pageCount <= 0)
        return pages;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.compare(QLatin1String("all"), Qt::CaseInsensitive) == 0) {
        pages.reserve(pageCount);
        for (int index = 0; index < pageCount; ++index)
            pages.append(index);
        return pages;
    }

    const QStringList parts = trimmed.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString piece = part.trimmed();
        if (piece.isEmpty())
            continue;

        const int dash = piece.indexOf(QLatin1Char('-'), 1);
        if (dash < 0) {
            bool ok = false;
            const int page = piece.toInt(&ok);
            if (ok && page >= 1 && page <= pageCount)
                pages.append(page - 1);
            continue;
        }

        bool fromOk = false;
        bool toOk = false;
        const int from = piece.left(dash).trimmed().toInt(&fromOk);
        const QString tail = piece.mid(dash + 1).trimmed();
        // "5-" means "from five to the end", which is how print dialogs behave.
        const int to = tail.isEmpty() ? pageCount : tail.toInt(&toOk);
        if (!fromOk || (!toOk && !tail.isEmpty()))
            continue;

        for (int page = qMax(1, from); page <= qMin(pageCount, to); ++page)
            pages.append(page - 1);
    }

    std::sort(pages.begin(), pages.end());
    pages.erase(std::unique(pages.begin(), pages.end()), pages.end());
    return pages;
}

QString formatPageRange(const QList<int> &pages)
{
    if (pages.isEmpty())
        return {};

    QList<int> sorted = pages;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    QStringList parts;
    int runStart = sorted.first();
    int previous = runStart;

    const auto flush = [&parts](int start, int end) {
        parts.append(start == end ? QString::number(start + 1)
                                  : QStringLiteral("%1-%2").arg(start + 1).arg(end + 1));
    };

    for (int index = 1; index < sorted.size(); ++index) {
        const int page = sorted.at(index);
        if (page == previous + 1) {
            previous = page;
            continue;
        }
        flush(runStart, previous);
        runStart = page;
        previous = page;
    }
    flush(runStart, previous);

    return parts.join(QStringLiteral(", "));
}

} // namespace sweet::ui
