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
    bool linearise = false;      ///< Optimise for byte-at-a-time web viewing.
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

    /// Serialises the document. Throws MuError on failure.
    QByteArray toBytes(const SaveOptions &options = {}) const;

    /// Writes the document to `path`. Incremental saving is only possible over
    /// the file the document came from.
    void saveToFile(const QString &path, const SaveOptions &options = {}) const;

    int pageRotation(int index) const;
    void setPageRotation(int index, int degrees);

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
    bool applyRedactions(int pageIndex);

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
