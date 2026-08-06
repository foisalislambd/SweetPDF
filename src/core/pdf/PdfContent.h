#pragma once

#include <QImage>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>

namespace sweet::core {

class PdfDocument;

/// Adds content that is neither a markup annotation nor a page operation:
/// pictures, signatures and links.
///
/// Pictures and signatures are written as stamp annotations. That is what
/// Acrobat does too, and it means placing one is a single undoable step that can
/// be moved, resized and deleted like any other annotation. Flatten the document
/// (see `forms::flatten`) to bake them into the page content for good.
namespace content {

/// Places `image` inside `rect` (unrotated page points) as a stamp annotation.
/// Returns the annotation's object number, or 0 on failure.
int addImage(PdfDocument &document, int pageIndex, const QImage &image, const QRectF &rect);

/// `rect` scaled to preserve `image`'s aspect ratio, anchored at the top left.
/// Handy for dropping a picture at its natural size without distorting it.
QRectF fitToAspect(const QSizeF &imageSize, const QRectF &rect);

/// A link over `rect`. `uri` is either an external URL or a destination made by
/// `internalUri`. Returns false when the page or document cannot take it.
bool addLink(PdfDocument &document, int pageIndex, const QRectF &rect, const QString &uri);

/// One clickable link on a page. Links are not annotations as far as MuPDF is
/// concerned, so they are listed separately from `annotations::onPage`.
struct Link
{
    QRectF rect; ///< Hot zone in unrotated page points.
    QString uri; ///< External URL, or an internal destination.
};

QList<Link> linksOnPage(const PdfDocument &document, int pageIndex);

/// Removes the link whose hot zone contains `at`. Returns false when there is
/// none there.
bool removeLinkAt(PdfDocument &document, int pageIndex, const QPointF &at);

/// The document-internal URI that jumps to the top of `pageIndex`, for links
/// that point somewhere else in the same file.
QString internalUri(const PdfDocument &document, int pageIndex);

/// The page a link points at, or -1 when it leads outside the document.
int pageForUri(const PdfDocument &document, const QString &uri);

} // namespace content
} // namespace sweet::core
