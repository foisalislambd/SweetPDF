#pragma once

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>

namespace sweet::core {

/// A range of characters on one page, as half-open [from, to).
struct TextRange
{
    int from = 0;
    int to = 0;

    bool isEmpty() const noexcept { return to <= from; }
    bool contains(int index) const noexcept { return index >= from && index < to; }
};

/// The text of one page, flattened into a string with a box per character.
///
/// Selection, copying and search all work on indices into `plainText()`, which
/// keeps them simple: a selection is two integers, a search hit is a range, and
/// turning either into something drawable is `boxes()`. Boxes are in unrotated
/// page points, the same space page annotations use.
class PageText
{
public:
    /// Extracted from a MuPDF structured text page. Only PdfPage builds these.
    PageText(QString text, QList<QRectF> boxes, QList<TextRange> lines);

    const QString &plainText() const noexcept { return m_text; }
    int length() const noexcept { return int(m_text.size()); }
    bool isEmpty() const noexcept { return m_text.isEmpty(); }

    /// Lines, in reading order, as ranges into `plainText()`.
    const QList<TextRange> &lines() const noexcept { return m_lines; }

    QRectF boxAt(int index) const;

    /// One rectangle per line touched by `range`, merged so a selection reads as
    /// a few bars rather than a box per glyph.
    QList<QRectF> boxes(const TextRange &range) const;

    QString textIn(const TextRange &range) const;

    /// Character index nearest `point`. Returns the insertion position, so
    /// clicking the right half of a glyph selects from after it. `hitText` tells
    /// callers whether the point actually landed on text.
    int indexAt(const QPointF &point, bool *hitText = nullptr) const;

    /// Range covering the word around `index`, for double-click selection.
    TextRange wordAt(int index) const;

    /// Range of the line containing `index`, for triple-click selection.
    TextRange lineAt(int index) const;

    /// Every occurrence of `needle`, in reading order.
    QList<TextRange> findAll(const QString &needle, Qt::CaseSensitivity sensitivity,
                             bool wholeWords) const;

private:
    QString m_text;
    QList<QRectF> m_boxes; ///< One per character; null for line breaks.
    QList<TextRange> m_lines;
};

} // namespace sweet::core
