#pragma once

#include <QImage>
#include <QList>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include <mutex>

#include <mupdf/fitz.h>

namespace sweet::core {

/// A single loaded page.
///
/// The first render builds a display list, which every later render reuses.
/// That makes zooming and scrolling cheap and lets several worker threads
/// rasterise the same page at once, since a finished display list is read-only.
class PdfPage
{
public:
    /// `parseMutex` is the document's parse lock, shared by every page of that
    /// document. MuPDF only allows one thread at a time to read from the file,
    /// so loading a page and building its display list must hold it.
    PdfPage(fz_document *document, int index, std::mutex &parseMutex);
    ~PdfPage();

    PdfPage(const PdfPage &) = delete;
    PdfPage &operator=(const PdfPage &) = delete;

    int index() const noexcept { return m_index; }

    /// Page box in PDF points, ignoring any view rotation.
    QSizeF sizePoints() const noexcept { return m_sizePoints; }

    /// Page box in points for a view rotated by `rotation` degrees.
    QSizeF sizePoints(int rotation) const;

    /// Rasterises the page. `scale` is 1.0 for 72 dpi. `rotation` must be a
    /// multiple of 90. Safe to call from any thread.
    QImage render(qreal scale, int rotation = 0) const;

    /// Plain text of the page with lines and blocks separated by newlines.
    QString text() const;

    /// Boxes of every match, in unrotated page points.
    QList<QRectF> search(const QString &needle, int maxHits = 500) const;

    /// Maps a point from page points to the coordinates of a rendered image.
    static QTransform pageToImage(const QSizeF &sizePoints, qreal scale, int rotation);

private:
    fz_display_list *displayList(fz_context *ctx) const;

    fz_document *m_document = nullptr;
    fz_page *m_page = nullptr;
    mutable fz_display_list *m_displayList = nullptr;
    std::mutex *m_parseMutex = nullptr;
    int m_index = -1;
    QSizeF m_sizePoints;
    fz_rect m_bounds{};
};

} // namespace sweet::core
