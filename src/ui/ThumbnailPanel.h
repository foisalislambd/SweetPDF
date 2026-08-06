#pragma once

#include <QList>
#include <QWidget>

class QImage;
class QListWidget;

namespace sweet::core {
class DocumentSession;
struct RenderKey;
}

namespace sweet::ui {

/// Page thumbnails.
///
/// Thumbnails go through the same render queue and cache as the main canvas, at
/// the lowest priority, so opening a large document never makes the page area
/// wait behind a few hundred previews.
class ThumbnailPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ThumbnailPanel(QWidget *parent = nullptr);

    void setSession(sweet::core::DocumentSession *session);

private:
    void rebuild();
    void requestVisibleThumbnails();
    void syncSelection(int pageIndex);
    void onRendered(const sweet::core::RenderKey &key, const QImage &image);

    QListWidget *m_list = nullptr;
    sweet::core::DocumentSession *m_session = nullptr;
    QList<qreal> m_scales; ///< Render scale per page, so pages of any size fit.
    bool m_syncing = false;
};

} // namespace sweet::ui
