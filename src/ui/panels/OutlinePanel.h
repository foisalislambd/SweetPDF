#pragma once

#include <QList>
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

namespace sweet::core {
class DocumentSession;
struct OutlineNode;
}

namespace sweet::ui {

/// The document's bookmark tree.
class OutlinePanel : public QWidget
{
    Q_OBJECT

public:
    explicit OutlinePanel(QWidget *parent = nullptr);

    void setSession(sweet::core::DocumentSession *session);

private:
    void rebuild();
    void appendNodes(QTreeWidgetItem *parent, const QList<sweet::core::OutlineNode> &nodes,
                     const QList<int> &prefix);
    QList<int> pathOf(QTreeWidgetItem *item) const;
    void addBookmark();
    void renameBookmark();
    void removeBookmark();

    QTreeWidget *m_tree = nullptr;
    QWidget *m_emptyState = nullptr;
    QPushButton *m_addButton = nullptr;
    sweet::core::DocumentSession *m_session = nullptr;
};

} // namespace sweet::ui
