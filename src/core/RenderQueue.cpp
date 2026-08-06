#include "core/RenderQueue.h"

#include "core/Logging.h"
#include "core/MuContext.h"

#include <QMutexLocker>
#include <QPointer>
#include <QThread>
#include <QThreadPool>

namespace sweet::core {
namespace {

int workerCount()
{
    const int cores = QThread::idealThreadCount();
    return qBound(2, cores - 1, 8);
}

QThreadPool *renderPool()
{
    static QThreadPool pool;
    static bool configured = false;
    if (!configured) {
        pool.setMaxThreadCount(workerCount());
        pool.setObjectName(QStringLiteral("sweetpdf-render"));
        configured = true;
    }
    return &pool;
}

} // namespace

RenderQueue::RenderQueue(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<sweet::core::RenderKey>("sweet::core::RenderKey");
}

RenderQueue::~RenderQueue()
{
    discardQueued();
    // Outstanding jobs hold a shared_ptr to the document, so they can finish
    // safely, but they must not outlive this object's signal emissions.
    renderPool()->waitForDone();
}

void RenderQueue::setDocument(std::shared_ptr<PdfDocument> document)
{
    discardQueued();
    m_cache.clear();
    m_document = std::move(document);
}

QImage RenderQueue::cached(const RenderKey &key) const
{
    return m_cache.image(key);
}

QImage RenderQueue::request(int pageIndex, qreal scale, int rotation, RenderPriority priority)
{
    if (!m_document || pageIndex < 0 || pageIndex >= m_document->pageCount() || scale <= 0.0)
        return {};

    const RenderKey key = RenderKey::make(pageIndex, scale, rotation);
    if (QImage hit = m_cache.image(key); !hit.isNull())
        return hit;

    {
        const QMutexLocker locker(&m_pendingMutex);
        if (m_pending.contains(key))
            return {};
        m_pending.insert(key);
    }

    submit(key, priority);
    return {};
}

void RenderQueue::submit(const RenderKey &key, RenderPriority priority)
{
    const quint64 generation = m_generation.load(std::memory_order_relaxed);
    auto document = m_document;
    QPointer<RenderQueue> self(this);

    renderPool()->start(
        [self, document, key, generation]() {
            QImage image;
            QString error;
            try {
                if (auto page = document->page(key.pageIndex))
                    image = page->render(key.scale(), key.rotation);
            } catch (const MuError &failure) {
                error = failure.details();
            } catch (const std::exception &failure) {
                error = QString::fromUtf8(failure.what());
            }

            if (!self)
                return;

            QMetaObject::invokeMethod(
                self, [self, key, image, generation, error]() {
                    if (self)
                        self->finish(key, image, generation, error);
                },
                Qt::QueuedConnection);
        },
        static_cast<int>(priority));
}

void RenderQueue::finish(const RenderKey &key, const QImage &image, quint64 generation,
                         const QString &error)
{
    {
        const QMutexLocker locker(&m_pendingMutex);
        m_pending.remove(key);
    }

    if (image.isNull()) {
        qCWarning(lcRender) << "Render failed for page" << key.pageIndex + 1 << ":" << error;
        Q_EMIT renderFailed(key.pageIndex, error);
        return;
    }

    m_cache.insert(key, image);

    // Stale bitmaps are still worth caching, but nothing is waiting for them.
    if (generation == m_generation.load(std::memory_order_relaxed))
        Q_EMIT rendered(key, image);
}

void RenderQueue::discardQueued()
{
    m_generation.fetch_add(1, std::memory_order_relaxed);
    const QMutexLocker locker(&m_pendingMutex);
    m_pending.clear();
}

} // namespace sweet::core
