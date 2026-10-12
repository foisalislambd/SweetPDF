#pragma once

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QRectF>
#include <QStringList>

#include <functional>

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

class FindBar;
class PageView;
class PropertiesPanel;
class ToolOptionsPanel;
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
    void newDocument();
    void newDocumentFromImages();
    void closeCurrentDocument();

    /// Runs one of the tasks offered by the home screen. `taskId` is an action id,
    /// or "organise" for the page panel. Asks for a document first when none is
    /// open, so a tile is a complete flow rather than a dead end.
    void runHomeTask(const QString &taskId);

    // --- File ------------------------------------------------------------
    bool saveDocument();
    bool saveDocumentAs(bool asCopy = false);
    void printDocument();
    void exportImages();
    void exportText();
    void exportHtml();

    // --- Pages -----------------------------------------------------------
    void insertBlankPage();
    void insertPagesFromFile(int atPage);
    void deleteCurrentPages();
    void extractPages(const QList<int> &pages);
    void saveEmbeddedFile(int index);
    void mergeDocuments();
    void splitDocument();
    void reversePages();
    void rotateAllPages();
    void deleteOddOrEvenPages();
    void convertToGrayscale();
    void removeAllComments();
    void exportComments();
    void showDocumentStats();
    void reloadDocument();
    void revealDocument();
    void goToPage();
    void copyPageImage();
    void setPresentation(bool on);
    void duplicateSelection();
    void copySelectionAcrossPages();
    void clearDescription();

    // --- Content ---------------------------------------------------------
    void addImageToPage();
    void replaceSelectedImage();
    void addLinkToPage();
    void signDocument();
    void replaceSelectedText();
    void replaceSelectedTextWith(const QString &replacement);
    void replaceSelectedTextWith(const QString &replacement, int pageIndex,
                                 const QList<QRectF> &boxes);
    void replaceAllText();
    void addHeaderFooter();
    void cropCurrentPage();
    void manageLinks();
    void addFormField();
    void compareDocument();
    void replyToSelection();
    void chooseCustomStamp();

    // --- Tools -----------------------------------------------------------
    void compressDocument();
    void protectDocument();
    void addWatermark();
    void applyRedactions();
    void recogniseText();
    void flattenDocument();
    void showPreferences();

    // --- Find ------------------------------------------------------------
    void showFindBar();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    /// Width-driven chrome density. Applied only when the mode itself changes,
    /// so a manual Ctrl+B / Ctrl+J is not undone by the next pixel of resize.
    enum class LayoutMode { Comfortable, Compact, Narrow };

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
    void finishOpening(sweet::core::DocumentSession *session, const QString &path, bool remember);
    void prepareNewSession(sweet::core::DocumentSession *session);

    LayoutMode layoutModeForWidth(int width) const;
    void applyLayoutMode(bool force = false);
    void clampSplitterSizes();
    void syncSidebarWidth();

    void closeTab(int index);
    PageView *currentView() const;
    sweet::core::DocumentSession *currentSession() const;
    PageView *viewAt(int tabIndex) const;
    FindBar *currentFindBar() const;
    int tabForPath(const QString &path) const;

    /// True when the current document can be edited at all, with a message shown
    /// when it cannot: a viewer-only format, or a document whose permissions say no.
    bool requireEditableDocument(bool needsAssembly = false);

    /// Runs a long job with a cancellable progress dialog. `work` receives a
    /// progress reporter it must call, and returns false when it was cancelled.
    void runWithProgress(const QString &title,
                         const std::function<bool(const std::function<bool(int, int)> &)> &work);

    /// A path next to the document, with `suffix` appended before the extension.
    QString suggestedPath(const QString &suffix, const QString &extension) const;

    void report(const QString &message);
    void reportError(const QString &title, const QString &details);

    /// Opens the inspector when the tool that was just picked has settings.
    void revealToolOptions();

    /// Shows the inspector when tool options need space and/or Properties is on.
    void updateInspectorVisibility();

    /// Keeps ribbon tool check-state in sync when the session tool changes
    /// outside a ribbon click (e.g. Stamp/Note auto-return to Select).
    void syncToolActions();

    QAction *makeAction(const QString &id, const QString &text, const QString &iconName,
                        const QKeySequence &shortcut = QKeySequence());
    QAction *action(const QString &id) const;

    void addRecentFile(const QString &path);
    void forgetRecentFile(const QString &path);
    void rebuildRecentMenu();
    QStringList recentFiles() const;

    RibbonBar *m_ribbon = nullptr;
    QSplitter *m_splitter = nullptr;
    Sidebar *m_sidebar = nullptr;
    QWidget *m_inspector = nullptr;
    ToolOptionsPanel *m_toolOptions = nullptr;
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
    QList<QAction *> m_presentKeys;
    bool m_presenting = false;

    LayoutMode m_layoutMode = LayoutMode::Comfortable;
    bool m_layoutModeReady = false;
};

} // namespace sweet::ui
