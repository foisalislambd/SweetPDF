#pragma once

#include "core/pdf/DocumentOutline.h"
#include "core/pdf/PdfPage.h"

#include <QList>
#include <QSizeF>
#include <QString>

#include <memory>
#include <mutex>

#include <mupdf/fitz.h>

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

/// An opened document. Pages are loaded lazily and shared, so a thumbnail and
/// the main view can render the same page without loading it twice.
class PdfDocument
{
public:
    /// Opens `path`. Throws MuError on failure, including a wrong password.
    /// Check needsPassword() first when the document may be encrypted.
    static std::shared_ptr<PdfDocument> open(const QString &path,
                                             const QString &password = QString());

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

private:
    PdfDocument(fz_document *document, QString path);

    void readPageSizes();
    void readInfo();
    void readPermissions();
    void readOutline();

    fz_document *m_document = nullptr;
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
