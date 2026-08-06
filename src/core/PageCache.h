#pragma once

#include "core/RenderRequest.h"

#include <QCache>
#include <QImage>
#include <QMutex>

namespace sweet::core {

/// Thread-safe LRU cache of rendered page bitmaps with a memory budget.
class PageCache
{
public:
    explicit PageCache(int budgetMegabytes = 256);

    QImage image(const RenderKey &key) const;
    bool contains(const RenderKey &key) const;
    void insert(const RenderKey &key, const QImage &image);

    /// Drops every bitmap of one page, whatever its zoom or rotation.
    void invalidatePage(int pageIndex);
    void clear();

    void setBudgetMegabytes(int megabytes);
    int budgetMegabytes() const;

private:
    mutable QMutex m_mutex;
    QCache<RenderKey, QImage> m_cache; ///< Cost is measured in kilobytes.
};

} // namespace sweet::core
