#include "core/render/RenderQueue.h"

#include "core/pdf/MuContext.h"
#include "core/util/Logging.h"

#include <QCoreApplication>
#include <QMutexLocker>
#include <QPointer>
#include <QThread>
#include <QThreadPool>

#include <memory>

namespace sweet::core {
namespace {

int workerCount()
{
    const int cores = QThread::idealThreadCount();
    return qBound(2, cores - 1, 8);
}

/// The pool every queue shares. Deliberately not a plain static object: a
/// QThreadPool must not outlive QCoreApplication, because its workers touch
/// per-application state as they wind down, and a static would be destroyed
/// after the application is gone.
QThreadPool *&poolSlot()
{
    static QThreadPool *pool = nullptr;
    return pool;
}

QThreadPool *renderPool()
{
    QThreadPool *&pool = poolSlot();
    if (!pool) {
        pool = new QThreadPool;
        pool->setMaxThreadCount(workerCount());
        pool->setObjectName(QStringLiteral("sweetpdf-render"));

        // Post routines run at the start of ~QCoreApplication, which is early
        // enough to stop the workers cleanly. Their thread-local MuPDF contexts
        // are then dropped while the base context is still alive too.
        qAddPostRoutine([] {
            QThreadPool *&owned = poolSlot();
            if (!owned)
                return;
            owned->waitForDone();
            delete owned;
            owned = nullptr;
        });
    }
    return pool;
}

} // namespace

RenderQueue::RenderQueue(QObject *parent)
    : QObject(parent)
    , m_alive(std::make_shared<std::atomic_bool>(true))
{
    qRegisterMetaType<sweet::core::RenderKey>("sweet::core::RenderKey");
}

RenderQueue::~RenderQueue()
{
    // Clearing this first means a job finishing on a worker thread right now
    // returns without touching a queue that is already unwinding.
    m_alive->store(false);
    discardQueued();

    // Outstanding jobs hold a shared_ptr to the document, so they can finish
    // safely, but they must not outlive this object. Never ask for the pool here:
    // during application shutdown it is already gone, and recreating it would
    // start threads nothing will ever stop.
    if (QThreadPool *pool = poolSlot())
        pool->waitForDone();
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
    auto alive = m_alive;
    QPointer<RenderQueue> self(this);

    renderPool()->start(
        [self, alive, document, key, generation]() {
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

            if (!alive->load() || !self)
                return;

            QMetaObject::invokeMethod(
                self, [self, alive, key, image, generation, error]() {
                    if (alive->load() && self)
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
