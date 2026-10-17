#pragma once

#include "core/pdf/DocumentOutline.h"
#include "core/pdf/PdfPage.h"

#include <QByteArray>
#include <QList>
#include <QSizeF>
#include <QString>

#include <memory>
#include <mutex>

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <mupdf/pdf/recolor.h>

namespace sweet::core {

/// Permissions declared by the document's security handler.
struct DocumentPermissions
{
    bool print = true;
    bool modify = true;
    bool copy = true;
    bool annotate = true;
    bool form = true;
    bool assemble = true;
};

struct DocumentInfo
{
    QString title;
    QString author;
    QString subject;
    QString keywords;
    QString creator;
    QString producer;
    QString format;
    QString creationDate;
    QString modificationDate;
    qint64 fileSize = 0;
};

/// How a document is written back out.
struct SaveOptions
{
    /// Append only the changes. Fast and preserves signatures, but impossible
    /// after structural edits or when encryption changes.
    bool incremental = false;
    /// Ignored. MuPDF 1.26 removed linearisation and 1.28 throws if it is requested.
    bool linearise = false;
    bool compressStreams = true;
    bool cleanContentStreams = false; ///< Rewrite content streams (smaller, slower).
    bool garbageCollect = true;       ///< Drop unreachable objects and de-duplicate.
    bool decompressStreams = false;   ///< For debugging a document by hand.

    enum class Encryption {
        Keep,   ///< Leave the document's existing encryption alone.
        None,   ///< Remove encryption.
        Aes128, ///< Acrobat 7 compatible.
        Aes256, ///< Acrobat 9 and later.
    };
    Encryption encryption = Encryption::Keep;

    /// Opening password. Empty means the document opens without one, while the
    /// owner password still restricts what may be done with it.
    QString userPassword;
    QString ownerPassword;
    DocumentPermissions permissions; ///< Only applied when encrypting.
};

/// Options for burning redaction annotations into page content.
struct RedactOptions
{
    /// Paint black boxes over redacted areas. Usually false: the redaction
    /// annotation already paints one while pending.
    bool blackBoxes = false;

    enum class ImageMethod {
        Pixels, ///< Blank overlapping image pixels (keeps the image object).
        Remove, ///< Drop images that touch the redaction region.
        None,   ///< Leave images alone (insecure).
    };
    ImageMethod images = ImageMethod::Pixels;

    enum class LineArt {
        None,            ///< Leave vector art alone (insecure).
        RemoveIfCovered, ///< Drop paths fully inside the redaction.
        RemoveIfTouched, ///< Drop paths that intersect the redaction (default).
    };
    LineArt lineArt = LineArt::RemoveIfTouched;
};

/// One optional-content layer (an OCG). `index` is MuPDF's layer number.
struct OptionalLayer
{
    int index = 0;
    QString name;
    bool enabled = true;
};

/// A named optional-content configuration (the presets in the PDF's OCProperties).
struct LayerConfig
{
    int index = 0;
    QString name;
};

/// A file embedded in the PDF, from the document's EmbeddedFiles name tree.
struct EmbeddedFile
{
    QString name;
    QString mimeType;
    qint64 size = -1;
    /// Set for a file-attachment annotation. -1 for a document-level embedded file.
    int pageIndex = -1;
    int objectNumber = 0;
    /// Index inside the EmbeddedFiles name tree. -1 for an annotation attachment.
    int nameTreeIndex = -1;
};

/// An opened document. Pages are loaded lazily and shared, so a thumbnail and
/// the main view can render the same page without loading it twice.
///
/// Editing follows one rule: annotation-level changes happen in place on this
/// instance, while structural changes (pages added, removed, reordered) are made
/// in place and then the caller re-opens the document from `toBytes()`. Re-opening
/// is what keeps cached pages, display lists and render results from pointing at
/// objects that no longer exist, and it makes undo a matter of keeping the
/// previous bytes.
class PdfDocument
{
public:
    /// Opens `path`. Throws MuError on failure, including a wrong password.
    /// Check needsPassword() first when the document may be encrypted.
    static std::shared_ptr<PdfDocument> open(const QString &path,
                                             const QString &password = QString());

    /// Opens a document held in memory. `displayPath` is only used for the title
    /// and for "save" to know where it came from; it may be empty for a document
    /// that has never been on disk.
    static std::shared_ptr<PdfDocument> openFromBytes(QByteArray bytes, QString displayPath);

    /// An empty one-page PDF, for "New document".
    static std::shared_ptr<PdfDocument> createEmpty(const QSizeF &pageSizePoints);

    /// Recolors every page to gray. The page count and boxes stay the same.
    void convertToGrayscale();

    /// True when `path` cannot be opened without a password.
    static bool needsPassword(const QString &path);

    ~PdfDocument();

    PdfDocument(const PdfDocument &) = delete;
    PdfDocument &operator=(const PdfDocument &) = delete;

    const QString &filePath() const noexcept { return m_filePath; }
    int pageCount() const noexcept { return m_pageCount; }
    bool isEncrypted() const noexcept { return m_encrypted; }
    const DocumentPermissions &permissions() const noexcept { return m_permissions; }
    const DocumentInfo &info() const noexcept { return m_info; }
    const QList<OutlineNode> &outline() const noexcept { return m_outline; }

    /// Appends a bookmark for `pageIndex` at the top level. PDF only.
    bool addBookmark(const QString &title, int pageIndex);
    /// Appends a child of the bookmark at `parentPath`.
    bool addChildBookmark(const QList<int> &parentPath, const QString &title, int pageIndex);
    /// `path` is the sibling index at each depth, matching the outline tree.
    bool renameBookmark(const QList<int> &path, const QString &title);
    bool removeBookmark(const QList<int> &path);

    /// Page box in points. Cheap: sizes are read once when the document opens.
    QSizeF pageSizePoints(int index) const;

    /// Largest page box in points, used to size the viewport.
    QSizeF maxPageSizePoints() const noexcept { return m_maxPageSize; }

    /// Loads a page, reusing the instance while any caller still holds it.
    std::shared_ptr<PdfPage> page(int index) const;

    // --- Editing ----------------------------------------------------------
    //
    // Everything below needs a PDF; XPS, EPUB and CBZ documents can be viewed
    // but not edited. Check isPdf() first.

    bool isPdf() const noexcept { return m_pdf != nullptr; }

    /// True when the file on disk no longer matches this document.
    bool hasUnsavedChanges() const;

    /// True when MuPDF can append changes to the original file. False for
    /// memory-backed documents, encryption changes, and after many structural
    /// edits — callers must then do a full rewrite.
    bool canSaveIncrementally() const;

    /// Serialises the document. Throws MuError on failure.
    QByteArray toBytes(const SaveOptions &options = {}) const;

    /// Writes the document to `path`. Incremental saving is only possible over
    /// the file the document came from, and only when canSaveIncrementally().
    void saveToFile(const QString &path, const SaveOptions &options = {}) const;

    /// Updates Info dictionary fields (title, author, …). Empty strings clear.
    void setInfo(const DocumentInfo &info);

    int pageRotation(int index) const;
    void setPageRotation(int index, int degrees);

    /// Insets the page's crop box by the given margins, in points. PDF only.
    void cropPage(int index, qreal left, qreal top, qreal right, qreal bottom);

    /// Sets the page's media box and drops any crop box. Size is in points.
    void setPageSize(int index, const QSizeF &sizePoints);

    /// Rotates the page content around its centre by `degrees` (counter-clockwise
    /// in PDF space). Used to straighten a scan. The page box stays put.
    void rotatePageContent(int index, qreal degrees);

    /// Replaces one literal string in the page content when it occurs exactly
    /// once, so the original font run stays. Returns false without changing the
    /// page when the wording is not a single literal (kerned text, for example).
    bool replaceTextLiteral(int index, const QString &from, const QString &to);

    /// Drops document info, XMP metadata, JavaScript, open actions and embedded
    /// files. Page content is left alone.
    void stripHiddenData();

    /// Removes pages, given in any order. Structural: re-open afterwards.
    void deletePages(QList<int> indices);
    void insertBlankPage(int at, const QSizeF &sizePoints);
    void duplicatePage(int index);
    void movePage(int from, int to);

    /// Rearranges the pages so that page `newOrder[i]` ends up at position `i`.
    /// `newOrder` must be a permutation of every page index.
    void reorderPages(const QList<int> &newOrder);

    /// Appends every page of the PDF at `path`. Used by merge.
    void appendPagesFrom(const QString &path, const QString &password = QString());

    /// A new document holding only `indices`, in the order given.
    QByteArray extractPages(const QList<int> &indices) const;

    /// Burns pending redaction annotations into the page, removing the content
    /// underneath rather than covering it. Returns true when anything changed.
    bool applyRedactions(int pageIndex, const RedactOptions &options = {});

    /// Drops cached display lists / text layers for one page, or every page when
    /// `pageIndex` is negative. Does not load pages that were never requested.
    void discardPageCaches(int pageIndex = -1) const;

    /// Scans the PDF for structural damage and repairs it in memory if needed.
    /// MuPDF 1.28 reports whether that repair rewrote anything. Cheap after the
    /// first call. Editing and saving call this themselves.
    void ensureStructure() const;
    bool structureRepaired() const noexcept { return m_structureRepaired; }
    /// Set when the structure check itself failed. Empty when it succeeded.
    const QString &structureWarning() const noexcept { return m_structureWarning; }

    /// Optional content groups. Empty when the file is not a PDF or has none.
    QList<OptionalLayer> layers() const;
    QList<LayerConfig> layerConfigs() const;
    /// Applies a named configuration and drops cached rasters.
    bool selectLayerConfig(int index);
    /// Shows or hides one layer and drops cached rasters. Returns false when
    /// `index` is not a layer of this document.
    bool setLayerEnabled(int index, bool enabled);

    QList<EmbeddedFile> embeddedFiles() const;
    /// Bytes of embedded file `index` in the list from embeddedFiles().
    QByteArray embeddedFileContents(int index) const;

    /// The MuPDF handles. For use inside `core/pdf` only: callers must hold
    /// parseMutex() and must not let these outlive the document.
    pdf_document *pdfHandle() const noexcept { return m_pdf; }
    fz_document *handle() const noexcept { return m_document; }
    std::mutex &parseMutex() const noexcept { return m_parseMutex; }

    /// Re-reads page count, sizes, metadata and outline after an edit, and drops
    /// cached page objects. Callers that changed the page tree should re-open the
    /// document instead: this only covers in-place edits.
    void refreshStructure();

private:
    PdfDocument(fz_document *document, QString path);

    /// Wraps a freshly created or opened MuPDF document, taking ownership.
    static std::shared_ptr<PdfDocument> adopt(fz_document *document, QString path,
                                              QByteArray backingBytes = {});

    pdf_document *requirePdf() const;
    void markEdited();
    void updateCachedPageSize(int index);

    void readPageSizes();
    void readInfo();
    void readPermissions();
    void readOutline();

    fz_document *m_document = nullptr;
    pdf_document *m_pdf = nullptr; ///< Same document, PDF interface, or null.
    /// Kept alive for as long as the document: MuPDF reads from this buffer
    /// lazily when the document was opened from memory.
    QByteArray m_backingBytes;
    bool m_edited = false;
    mutable bool m_structureChecked = false;
    mutable bool m_structureRepaired = false;
    mutable QString m_structureWarning;
    QString m_filePath;
    int m_pageCount = 0;
    bool m_encrypted = false;
    DocumentPermissions m_permissions;
    DocumentInfo m_info;
    QList<OutlineNode> m_outline;
    QList<QSizeF> m_pageSizes;
    QSizeF m_maxPageSize;

    /// Serialises everything that reads from the file. MuPDF's own multi-thread
    /// guidance is that only rasterising a finished display list may run in
    /// parallel; parsing must not.
    mutable std::mutex m_parseMutex;
    mutable std::mutex m_pageMutex;
    mutable QList<std::weak_ptr<PdfPage>> m_pages;
};

} // namespace sweet::core
