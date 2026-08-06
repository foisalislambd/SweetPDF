#pragma once

#include <QHash>
#include <QMainWindow>
#include <QStringList>

class QAction;
class QLabel;
class QLineEdit;
class QMenu;
class QStackedWidget;
class QSlider;
class QSplitter;
class QTabWidget;
class QToolButton;

namespace sweet::core {
class DocumentSession;
}

namespace sweet::ui {

class PageView;
class PropertiesPanel;
class RibbonBar;
class Sidebar;
class WelcomeView;

/// The application window: ribbon, sidebar, document tabs, inspector, status bar.
///
/// Every open document lives in its own tab with its own DocumentSession, so
/// zoom, undo history and page position are per document. The window only ever
/// forwards commands to the session of the active tab.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    /// Opens `path` in a new tab, or focuses the tab that already has it.
    /// Prompts for a password when the document is encrypted.
    bool openFile(const QString &path);

public Q_SLOTS:
    void openFileDialog();
    void closeCurrentDocument();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void createActions();
    /// Builds the application menu. It hangs off a button in the ribbon tab strip
    /// instead of a separate menu bar, so the window has one command row instead
    /// of two stacked ones.
    void createAppMenu();
    void createRibbon();
    void createCentralArea();
    void createStatusBar();
    void connectSessionSignals();
    void updateForCurrentDocument();
    void updateWindowTitleFromSession();
    void showWelcomeIfEmpty();

    void closeTab(int index);
    PageView *currentView() const;
    sweet::core::DocumentSession *currentSession() const;
    PageView *viewAt(int tabIndex) const;
    int tabForPath(const QString &path) const;

    QAction *makeAction(const QString &id, const QString &text, const QString &iconName,
                        const QKeySequence &shortcut = QKeySequence());
    QAction *action(const QString &id) const;
    /// Registers an action that has no implementation yet: visible, disabled and
    /// labelled as planned so the command surface stays discoverable.
    QAction *makePlannedAction(const QString &id, const QString &text, const QString &iconName);

    void addRecentFile(const QString &path);
    void rebuildRecentMenu();
    QStringList recentFiles() const;

    RibbonBar *m_ribbon = nullptr;
    QSplitter *m_splitter = nullptr;
    Sidebar *m_sidebar = nullptr;
    PropertiesPanel *m_properties = nullptr;
    QTabWidget *m_tabs = nullptr;
    QStackedWidget *m_center = nullptr;
    WelcomeView *m_welcome = nullptr;

    QLineEdit *m_pageField = nullptr;
    QLabel *m_pageCountLabel = nullptr;
    QToolButton *m_zoomButton = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QMenu *m_appMenu = nullptr;
    QMenu *m_recentMenu = nullptr;

    QHash<QString, QAction *> m_actions;
    QList<QAction *> m_documentActions; ///< Enabled only while a document is open.
};

} // namespace sweet::ui
