#pragma once

#include <QColor>
#include <QList>
#include <QPointF>
#include <QString>

namespace sweet::core {

/// One entry of the document bookmark tree.
struct OutlineNode
{
    QString title;
    QString uri;              ///< External link target, empty for internal jumps.
    int pageIndex = -1;       ///< Destination page, -1 when unresolved.
    QPointF pageOffset;       ///< Destination inside the page, in points.
    bool openByDefault = false;
    bool bold = false;
    bool italic = false;
    /// Invalid when the document does not colour this entry.
    QColor color;
    QList<OutlineNode> children;
};

} // namespace sweet::core
