#pragma once

#include "core/pdf/PageText.h"

#include <QImage>
#include <QList>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include <memory>
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

    /// The text layer: characters with their boxes, for selection and search.
    /// Built on first use and cached, since extraction is not cheap and the
    /// selection code asks for it on every mouse move.
    std::shared_ptr<const PageText> textLayer() const;

    /// Throws the cached text layer away, after the page content changed.
    void discardTextLayer();

    /// Throws the cached display list away, so the next render re-reads the page.
    /// Needed after annotations are added, removed or redacted.
    void discardDisplayList();

    /// Boxes of every match, in unrotated page points.
    QList<QRectF> search(const QString &needle, int maxHits = 500) const;

    /// Maps a point from page points to the coordinates of a rendered image.
    static QTransform pageToImage(const QSizeF &sizePoints, qreal scale, int rotation);

    /// The underlying MuPDF page. Callers must hold the document's parse lock and
    /// stay inside `core/pdf`.
    fz_page *handle() const noexcept { return m_page; }

private:
    fz_display_list *displayList(fz_context *ctx) const;

    fz_document *m_document = nullptr;
    fz_page *m_page = nullptr;
    mutable fz_display_list *m_displayList = nullptr;
    mutable std::shared_ptr<const PageText> m_textLayer;
    std::mutex *m_parseMutex = nullptr;
    int m_index = -1;
    QSizeF m_sizePoints;
    fz_rect m_bounds{};
};

} // namespace sweet::core
