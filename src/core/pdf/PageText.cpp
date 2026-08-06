#include "core/pdf/PageText.h"

#include <limits>

namespace sweet::core {
namespace {

bool isWordCharacter(QChar character)
{
    return character.isLetterOrNumber() || character == QLatin1Char('_');
}

} // namespace

PageText::PageText(QString text, QList<QRectF> boxes, QList<TextRange> lines)
    : m_text(std::move(text))
    , m_boxes(std::move(boxes))
    , m_lines(std::move(lines))
{
}

QRectF PageText::boxAt(int index) const
{
    if (index < 0 || index >= m_boxes.size())
        return {};
    return m_boxes.at(index);
}

QList<QRectF> PageText::boxes(const TextRange &range) const
{
    QList<QRectF> result;
    if (range.isEmpty())
        return result;

    const int from = qBound(0, range.from, length());
    const int to = qBound(from, range.to, length());

    // One rectangle per line, grown character by character. Line breaks have no
    // box, so they end the current run without adding a stray rectangle.
    for (const TextRange &line : m_lines) {
        if (line.to <= from || line.from >= to)
            continue;

        QRectF merged;
        for (int index = qMax(from, line.from); index < qMin(to, line.to); ++index) {
            const QRectF box = m_boxes.at(index);
            if (box.isNull())
                continue;
            merged = merged.isNull() ? box : merged.united(box);
        }
        if (!merged.isNull())
            result.append(merged);
    }
    return result;
}

QString PageText::textIn(const TextRange &range) const
{
    if (range.isEmpty())
        return {};
    const int from = qBound(0, range.from, length());
    const int to = qBound(from, range.to, length());
    return m_text.mid(from, to - from);
}

int PageText::indexAt(const QPointF &point, bool *hitText) const
{
    if (hitText)
        *hitText = false;
    if (m_boxes.isEmpty())
        return 0;

    // Prefer a line whose vertical band contains the point: that is what makes
    // dragging sideways behave like a text cursor rather than a lasso.
    const TextRange *bestLine = nullptr;
    qreal bestDistance = std::numeric_limits<qreal>::max();

    for (const TextRange &line : m_lines) {
        QRectF band;
        for (int index = line.from; index < line.to; ++index) {
            const QRectF box = m_boxes.at(index);
            if (!box.isNull())
                band = band.isNull() ? box : band.united(box);
        }
        if (band.isNull())
            continue;

        const qreal distance = point.y() < band.top() ? band.top() - point.y()
            : point.y() > band.bottom()               ? point.y() - band.bottom()
                                                      : 0.0;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestLine = &line;
            if (distance == 0.0)
                break;
        }
    }

    if (!bestLine)
        return 0;

    int nearest = bestLine->from;
    qreal nearestDistance = std::numeric_limits<qreal>::max();
    for (int index = bestLine->from; index < bestLine->to; ++index) {
        const QRectF box = m_boxes.at(index);
        if (box.isNull())
            continue;

        if (box.contains(point)) {
            if (hitText)
                *hitText = true;
            // Right half means "after this character", so a click between two
            // glyphs puts the caret where the user pointed.
            return point.x() > box.center().x() ? index + 1 : index;
        }

        const qreal distance = qAbs(point.x() - box.center().x());
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = point.x() > box.center().x() ? index + 1 : index;
        }
    }
    return nearest;
}

TextRange PageText::wordAt(int index) const
{
    if (m_text.isEmpty())
        return {};

    const int position = qBound(0, index, length() - 1);
    if (!isWordCharacter(m_text.at(position)))
        return { position, position + 1 };

    int from = position;
    while (from > 0 && isWordCharacter(m_text.at(from - 1)))
        --from;
    int to = position;
    while (to < length() && isWordCharacter(m_text.at(to)))
        ++to;
    return { from, to };
}

TextRange PageText::lineAt(int index) const
{
    for (const TextRange &line : m_lines) {
        if (line.contains(index))
            return line;
    }
    return m_lines.isEmpty() ? TextRange{} : m_lines.last();
}

QList<TextRange> PageText::findAll(const QString &needle, Qt::CaseSensitivity sensitivity,
                                  bool wholeWords) const
{
    QList<TextRange> hits;
    if (needle.isEmpty() || m_text.isEmpty())
        return hits;

    int from = 0;
    while (true) {
        const int at = m_text.indexOf(needle, from, sensitivity);
        if (at < 0)
            break;

        const int end = at + int(needle.size());
        from = at + 1;

        if (wholeWords) {
            const bool leftOk = at == 0 || !isWordCharacter(m_text.at(at - 1));
            const bool rightOk = end >= length() || !isWordCharacter(m_text.at(end));
            if (!leftOk || !rightOk)
                continue;
        }
        hits.append({ at, end });
    }
    return hits;
}

} // namespace sweet::core
