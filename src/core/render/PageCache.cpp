#include "core/render/PageCache.h"

#include <QMutexLocker>

namespace sweet::core {
namespace {

int costKilobytes(const QImage &image)
{
    return qMax(1, static_cast<int>(image.sizeInBytes() / 1024));
}

} // namespace

PageCache::PageCache(int budgetMegabytes)
    : m_cache(budgetMegabytes * 1024)
{
}

QImage PageCache::image(const RenderKey &key) const
{
    const QMutexLocker locker(&m_mutex);
    if (const QImage *cached = m_cache.object(key))
        return *cached;
    return {};
}

bool PageCache::contains(const RenderKey &key) const
{
    const QMutexLocker locker(&m_mutex);
    return m_cache.contains(key);
}

void PageCache::insert(const RenderKey &key, const QImage &image)
{
    if (image.isNull())
        return;

    const QMutexLocker locker(&m_mutex);
    m_cache.insert(key, new QImage(image), costKilobytes(image));
}

void PageCache::invalidatePage(int pageIndex)
{
    const QMutexLocker locker(&m_mutex);
    const QList<RenderKey> keys = m_cache.keys();
    for (const RenderKey &key : keys) {
        if (key.pageIndex == pageIndex)
            m_cache.remove(key);
    }
}

void PageCache::clear()
{
    const QMutexLocker locker(&m_mutex);
    m_cache.clear();
}

void PageCache::setBudgetMegabytes(int megabytes)
{
    const QMutexLocker locker(&m_mutex);
    m_cache.setMaxCost(qMax(16, megabytes) * 1024);
}

int PageCache::budgetMegabytes() const
{
    const QMutexLocker locker(&m_mutex);
    return m_cache.maxCost() / 1024;
}

} // namespace sweet::core
