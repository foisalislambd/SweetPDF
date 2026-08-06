#pragma once

#include <QHashFunctions>

#include <QtGlobal>

namespace sweet::core {

/// Identifies one rasterised bitmap. Scale is quantised so that near-identical
/// zoom levels reuse the same cache entry instead of re-rendering.
struct RenderKey
{
    int pageIndex = 0;
    int scaleMilli = 1000; ///< scale * 1000, rounded.
    int rotation = 0;

    static int quantise(qreal scale) { return qRound(scale * 1000.0); }

    static RenderKey make(int pageIndex, qreal scale, int rotation)
    {
        return RenderKey{ pageIndex, quantise(scale), ((rotation % 360) + 360) % 360 };
    }

    qreal scale() const { return scaleMilli / 1000.0; }

    friend bool operator==(const RenderKey &a, const RenderKey &b)
    {
        return a.pageIndex == b.pageIndex && a.scaleMilli == b.scaleMilli
            && a.rotation == b.rotation;
    }
};

inline size_t qHash(const RenderKey &key, size_t seed = 0)
{
    return qHashMulti(seed, key.pageIndex, key.scaleMilli, key.rotation);
}

/// Higher values are rasterised first.
enum class RenderPriority {
    Thumbnail = 0,
    Prefetch = 1,
    Visible = 2,
    Interactive = 3,
};

} // namespace sweet::core
