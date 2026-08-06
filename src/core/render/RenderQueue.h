#pragma once

#include "core/pdf/PdfDocument.h"
#include "core/render/PageCache.h"
#include "core/render/RenderKey.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSet>

#include <atomic>
#include <memory>

namespace sweet::core {

/// Rasterises pages on a worker pool and caches the results.
///
/// Views ask for a bitmap and either get it straight from the cache or receive
/// it later through rendered(). Requests carry a generation counter, so a burst
/// of zoom or scroll events can discard everything queued for the old view
/// without waiting for those jobs to finish.
class RenderQueue : public QObject
{
    Q_OBJECT

public:
    explicit RenderQueue(QObject *parent = nullptr);
    ~RenderQueue() override;

    void setDocument(std::shared_ptr<PdfDocument> document);

    /// Bitmap already in the cache, or a null image.
    QImage cached(const RenderKey &key) const;

    /// Queues a render unless the bitmap is cached or the job is already
    /// running. Returns the cached bitmap when there is one.
    QImage request(int pageIndex, qreal scale, int rotation,
                   RenderPriority priority = RenderPriority::Visible);

    /// Makes every queued job obsolete. Jobs already running still finish, but
    /// their results are dropped.
    void discardQueued();

    PageCache &cache() { return m_cache; }

Q_SIGNALS:
    void rendered(const sweet::core::RenderKey &key, const QImage &image);
    void renderFailed(int pageIndex, const QString &message);

private:
    void submit(const RenderKey &key, RenderPriority priority);
    void finish(const RenderKey &key, const QImage &image, quint64 generation,
                const QString &error);

    std::shared_ptr<PdfDocument> m_document;
    PageCache m_cache;
    std::atomic<quint64> m_generation{ 0 };

    /// Cleared when this queue starts being destroyed. Jobs already on a worker
    /// thread check it before reaching back into the queue, so closing a document
    /// mid-render cannot touch a half-destroyed object.
    std::shared_ptr<std::atomic_bool> m_alive;

    mutable QMutex m_pendingMutex;
    QSet<RenderKey> m_pending;
};

} // namespace sweet::core
