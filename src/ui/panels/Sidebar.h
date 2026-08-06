#pragma once

#include <QWidget>

class QButtonGroup;
class QLabel;
class QStackedWidget;
class QVBoxLayout;

namespace sweet::core {
class DocumentSession;
}

namespace sweet::ui {

class CommentsPanel;
class FormsPanel;
class OutlinePanel;
class ThumbnailPanel;

/// Left navigation: an always-visible icon rail plus the panel it selects.
///
/// The rail keeps every navigator one click away while the panel itself can be
/// collapsed to give the page area the full window, which is the layout users
/// already know from Acrobat and Foxit.
class Sidebar : public QWidget
{
    Q_OBJECT

public:
    enum class Panel { Pages, Bookmarks, Comments, Forms, Attachments, Layers };

    explicit Sidebar(QWidget *parent = nullptr);

    void setSession(sweet::core::DocumentSession *session);

    Panel currentPanel() const noexcept { return m_currentPanel; }
    bool isPanelVisible() const;

public Q_SLOTS:
    void showPanel(sweet::ui::Sidebar::Panel panel);
    void setPanelVisible(bool visible);
    void togglePanel();

Q_SIGNALS:
    void panelVisibilityChanged(bool visible);

    /// A comment was picked in the comments panel, or a form field in the forms
    /// panel; both mean "show me this on the page".
    void annotationActivated(int pageIndex, int objectNumber);

    /// Page operations from the thumbnail panel that need a dialog, forwarded to
    /// the window: a panel has no business opening file dialogs.
    void extractRequested(const QList<int> &pages);
    void insertFileRequested(int atPage);
    void statusMessage(const QString &message);

private:
    void addRailButton(Panel panel, const QString &iconName, const QString &label);

    QWidget *m_content = nullptr;
    QLabel *m_title = nullptr;
    QStackedWidget *m_stack = nullptr;
    QButtonGroup *m_rail = nullptr;
    QVBoxLayout *m_railLayout = nullptr;

    ThumbnailPanel *m_thumbnails = nullptr;
    OutlinePanel *m_outline = nullptr;
    CommentsPanel *m_comments = nullptr;
    FormsPanel *m_forms = nullptr;

    Panel m_currentPanel = Panel::Pages;
};

} // namespace sweet::ui
