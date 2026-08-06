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
/// Previews go through the same render queue and cache as the main canvas but at
/// the lowest priority, so opening a large document never makes the page area
/// wait behind a few hundred previews. Only the previews near the viewport are
/// requested, and the list is painted by a delegate rather than by icon-mode
/// defaults so pages get a real page look: rounded corners, a shadow and a
/// selection ring.
class ThumbnailPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ThumbnailPanel(QWidget *parent = nullptr);

    void setSession(sweet::core::DocumentSession *session);

    /// Pages the user has selected, in ascending order. Falls back to the current
    /// page when the selection is empty.
    QList<int> selectedPages() const;

Q_SIGNALS:
    /// A page operation needs a file dialog or a confirmation, which belongs to
    /// the window rather than to a panel.
    void extractRequested(const QList<int> &pages);
    void insertFileRequested(int atPage);
    void statusMessage(const QString &message);

private:
    void rebuild();
    void showContextMenu(const QPoint &at);
    void applyReorder(int first, int last, int destination);
    void rotateSelection(int degrees);
    void deleteSelection();
    void duplicateSelection();
    void insertBlank(int atPage);
    void requestVisibleThumbnails();
    void syncSelection(int pageIndex);
    void onRendered(const sweet::core::RenderKey &key, const QImage &image);

    QListWidget *m_list = nullptr;
    sweet::core::DocumentSession *m_session = nullptr;
    QList<qreal> m_scales; ///< Render scale per page, so any page size fits.
    bool m_syncing = false;
    bool m_reordering = false; ///< Guards the rowsMoved handler during a rebuild.
};

} // namespace sweet::ui
