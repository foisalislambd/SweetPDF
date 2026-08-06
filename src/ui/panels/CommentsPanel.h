#pragma once

#include "core/pdf/PdfAnnotations.h"

#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QTimer;

namespace sweet::core {
class DocumentSession;
}

namespace sweet::ui {

/// Every annotation in the document, as a list of cards.
///
/// The list is rebuilt from the document rather than kept in step edit by edit:
/// the document is the only place annotations really live, and re-reading it is
/// cheap next to how often a person adds a comment. Rebuilds are coalesced
/// through a timer so a burst of edits costs one pass.
class CommentsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CommentsPanel(QWidget *parent = nullptr);

    void setSession(sweet::core::DocumentSession *session);

Q_SIGNALS:
    /// The user picked a comment and wants to see it on the page.
    void annotationActivated(int pageIndex, int objectNumber);

private:
    void scheduleRebuild();
    void rebuild();
    void deleteSelected();
    void editSelected();

    sweet::core::DocumentSession *m_session = nullptr;
    QListWidget *m_list = nullptr;
    QWidget *m_emptyState = nullptr;
    QTimer *m_rebuildTimer = nullptr;
};

} // namespace sweet::ui
