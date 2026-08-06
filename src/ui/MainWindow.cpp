#include "ui/MainWindow.h"

#include "core/DocumentSession.h"
#include "core/PdfDocument.h"
#include "ui/PageView.h"
#include "ui/PropertiesPanel.h"
#include "ui/RibbonBar.h"
#include "ui/Sidebar.h"
#include "ui/Theme.h"
#include "ui/ViewerToolbar.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

namespace sweet::ui {
namespace {

constexpr int recentFileLimit = 12;
constexpr int zoomSliderResolution = 1000;
const char *const recentFilesKey = "recentFiles";
const char *const iconNameProperty = "sweetIconName";

int zoomToSlider(qreal zoom)
{
    const qreal span = std::log(core::DocumentSession::maxZoom / core::DocumentSession::minZoom);
    const qreal position = std::log(zoom / core::DocumentSession::minZoom) / span;
    return qBound(0, qRound(position * zoomSliderResolution), zoomSliderResolution);
}

qreal sliderToZoom(int value)
{
    const qreal span = std::log(core::DocumentSession::maxZoom / core::DocumentSession::minZoom);
    return core::DocumentSession::minZoom
        * std::exp(span * value / static_cast<qreal>(zoomSliderResolution));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("SweetPDF"));
    setAcceptDrops(true);
    resize(1360, 880);

    createActions();
    createMenuBar();
    createCentralArea();
    createRibbon();
    createStatusBar();

    connect(&Theme::instance(), &Theme::changed, this, [this] {
        for (QAction *entry : std::as_const(m_actions)) {
            const QString iconName = entry->property(iconNameProperty).toString();
            if (!iconName.isEmpty())
                entry->setIcon(icon(iconName));
        }
    });

    rebuildRecentMenu();
    updateForCurrentDocument();

    const QSettings settings;
    if (const QByteArray geometry = settings.value(QStringLiteral("geometry")).toByteArray();
        !geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    if (const QByteArray splitter = settings.value(QStringLiteral("splitter")).toByteArray();
        !splitter.isEmpty()) {
        m_splitter->restoreState(splitter);
    }
    if (settings.value(QStringLiteral("darkMode"), false).toBool())
        action(QStringLiteral("toggleTheme"))->setChecked(true);
}

// --- Actions ---------------------------------------------------------------

QAction *MainWindow::makeAction(const QString &id, const QString &text, const QString &iconName,
                                const QKeySequence &shortcut)
{
    auto *entry = new QAction(text, this);
    if (!iconName.isEmpty()) {
        entry->setIcon(icon(iconName));
        entry->setProperty(iconNameProperty, iconName);
    }
    if (!shortcut.isEmpty()) {
        entry->setShortcut(shortcut);
        entry->setShortcutContext(Qt::WindowShortcut);
    }
    addAction(entry);
    m_actions.insert(id, entry);
    return entry;
}

QAction *MainWindow::makePlannedAction(const QString &id, const QString &text,
                                       const QString &iconName)
{
    QAction *entry = makeAction(id, text, iconName);
    entry->setEnabled(false);
    entry->setToolTip(tr("%1 — planned, not implemented yet").arg(text));
    entry->setStatusTip(entry->toolTip());
    return entry;
}

QAction *MainWindow::action(const QString &id) const
{
    return m_actions.value(id);
}

void MainWindow::createActions()
{
    // File
    connect(makeAction(QStringLiteral("open"), tr("Open"), QStringLiteral("open"),
                       QKeySequence::Open),
            &QAction::triggered, this, &MainWindow::openFileDialog);
    makePlannedAction(QStringLiteral("save"), tr("Save"), QStringLiteral("save"));
    makePlannedAction(QStringLiteral("saveAs"), tr("Save As"), QStringLiteral("save-as"));
    makePlannedAction(QStringLiteral("print"), tr("Print"), QStringLiteral("print"));
    makePlannedAction(QStringLiteral("export"), tr("Export"), QStringLiteral("export"));

    QAction *close = makeAction(QStringLiteral("close"), tr("Close"), QStringLiteral("close"),
                                QKeySequence::Close);
    connect(close, &QAction::triggered, this, &MainWindow::closeCurrentDocument);
    m_documentActions.append(close);

    connect(makeAction(QStringLiteral("quit"), tr("Quit"), QString(), QKeySequence::Quit),
            &QAction::triggered, this, &MainWindow::close);

    // History
    QAction *undo = makeAction(QStringLiteral("undo"), tr("Undo"), QStringLiteral("undo"),
                               QKeySequence::Undo);
    QAction *redo = makeAction(QStringLiteral("redo"), tr("Redo"), QStringLiteral("redo"),
                               QKeySequence::Redo);
    undo->setEnabled(false);
    redo->setEnabled(false);

    // Pointer and markup tools
    auto *toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);
    const struct {
        const char *id;
        QString text;
        QString iconName;
        core::Tool tool;
        QKeySequence shortcut;
    } tools[] = {
        { "toolSelect", tr("Select"), QStringLiteral("cursor"), core::Tool::Select,
          QKeySequence(Qt::Key_V) },
        { "toolHand", tr("Pan"), QStringLiteral("hand"), core::Tool::Hand,
          QKeySequence(Qt::Key_H) },
        { "toolHighlight", tr("Highlight"), QStringLiteral("highlight"), core::Tool::Highlight,
          QKeySequence() },
        { "toolUnderline", tr("Underline"), QStringLiteral("underline"), core::Tool::Underline,
          QKeySequence() },
        { "toolStrikeout", tr("Strikeout"), QStringLiteral("strikeout"), core::Tool::Strikeout,
          QKeySequence() },
        { "toolDraw", tr("Draw"), QStringLiteral("draw"), core::Tool::Draw, QKeySequence() },
        { "toolNote", tr("Note"), QStringLiteral("note"), core::Tool::Note, QKeySequence() },
    };

    for (const auto &definition : tools) {
        QAction *entry = makeAction(QString::fromLatin1(definition.id), definition.text,
                                    definition.iconName, definition.shortcut);
        entry->setCheckable(true);
        toolGroup->addAction(entry);
        m_documentActions.append(entry);
        const core::Tool tool = definition.tool;
        connect(entry, &QAction::triggered, this, [this, tool] {
            if (auto *session = currentSession())
                session->setTool(tool);
        });
    }
    action(QStringLiteral("toolSelect"))->setChecked(true);

    // Content editing, still to come
    makePlannedAction(QStringLiteral("editText"), tr("Edit Text"), QStringLiteral("edit-text"));
    makePlannedAction(QStringLiteral("editImage"), tr("Edit Image"), QStringLiteral("edit-image"));
    makePlannedAction(QStringLiteral("addText"), tr("Add Text"), QStringLiteral("add-text"));
    makePlannedAction(QStringLiteral("addImage"), tr("Add Image"), QStringLiteral("add-image"));
    makePlannedAction(QStringLiteral("addShape"), tr("Shape"), QStringLiteral("shape"));
    makePlannedAction(QStringLiteral("addLink"), tr("Link"), QStringLiteral("link"));
    makePlannedAction(QStringLiteral("ocr"), tr("OCR"), QStringLiteral("ocr"));
    makePlannedAction(QStringLiteral("merge"), tr("Merge"), QStringLiteral("merge"));
    makePlannedAction(QStringLiteral("split"), tr("Split"), QStringLiteral("split"));
    makePlannedAction(QStringLiteral("compress"), tr("Compress"), QStringLiteral("compress"));
    makePlannedAction(QStringLiteral("convert"), tr("Convert"), QStringLiteral("convert"));
    makePlannedAction(QStringLiteral("encrypt"), tr("Encrypt"), QStringLiteral("lock"));
    makePlannedAction(QStringLiteral("redact"), tr("Redact"), QStringLiteral("redact"));
    makePlannedAction(QStringLiteral("sign"), tr("Sign"), QStringLiteral("sign"));
    makePlannedAction(QStringLiteral("forms"), tr("Forms"), QStringLiteral("form"));
    makePlannedAction(QStringLiteral("permissions"), tr("Permissions"), QStringLiteral("shield"));
    makePlannedAction(QStringLiteral("insertPages"), tr("Insert"), QStringLiteral("plus"));
    makePlannedAction(QStringLiteral("deletePages"), tr("Delete"), QStringLiteral("trash"));
    makePlannedAction(QStringLiteral("extractPages"), tr("Extract"), QStringLiteral("duplicate"));
    makePlannedAction(QStringLiteral("settings"), tr("Settings"), QStringLiteral("settings"));

    // View
    const struct {
        const char *id;
        QString text;
        QString iconName;
        QKeySequence shortcut;
        void (PageView::*slot)();
    } viewCommands[] = {
        { "zoomIn", tr("Zoom In"), QStringLiteral("zoom-in"), QKeySequence::ZoomIn,
          &PageView::zoomIn },
        { "zoomOut", tr("Zoom Out"), QStringLiteral("zoom-out"), QKeySequence::ZoomOut,
          &PageView::zoomOut },
        { "actualSize", tr("100%"), QStringLiteral("actual-size"),
          QKeySequence(Qt::CTRL | Qt::Key_0), &PageView::actualSize },
        { "fitPage", tr("Fit Page"), QStringLiteral("fit-page"),
          QKeySequence(Qt::CTRL | Qt::Key_1), &PageView::fitPage },
        { "fitWidth", tr("Fit Width"), QStringLiteral("fit-width"),
          QKeySequence(Qt::CTRL | Qt::Key_2), &PageView::fitWidth },
    };
    for (const auto &command : viewCommands) {
        QAction *entry = makeAction(QString::fromLatin1(command.id), command.text,
                                    command.iconName, command.shortcut);
        m_documentActions.append(entry);
        const auto slot = command.slot;
        connect(entry, &QAction::triggered, this, [this, slot] {
            if (PageView *view = currentView())
                (view->*slot)();
        });
    }

    QAction *rotateLeft = makeAction(QStringLiteral("rotateLeft"), tr("Rotate Left"),
                                     QStringLiteral("rotate-left"));
    connect(rotateLeft, &QAction::triggered, this, [this] {
        if (auto *session = currentSession())
            session->rotateBy(-90);
    });
    m_documentActions.append(rotateLeft);

    QAction *rotateRight = makeAction(QStringLiteral("rotateRight"), tr("Rotate Right"),
                                      QStringLiteral("rotate-right"));
    connect(rotateRight, &QAction::triggered, this, [this] {
        if (auto *session = currentSession())
            session->rotateBy(90);
    });
    m_documentActions.append(rotateRight);

    auto *layoutGroup = new QActionGroup(this);
    layoutGroup->setExclusive(true);
    const struct {
        const char *id;
        QString text;
        QString iconName;
        core::PageLayout layout;
    } layouts[] = {
        { "layoutSingle", tr("Single Page"), QStringLiteral("single-page"),
          core::PageLayout::SinglePage },
        { "layoutContinuous", tr("Continuous"), QStringLiteral("continuous"),
          core::PageLayout::Continuous },
        { "layoutTwoPage", tr("Two Pages"), QStringLiteral("two-page"),
          core::PageLayout::TwoPage },
    };
    for (const auto &definition : layouts) {
        QAction *entry = makeAction(QString::fromLatin1(definition.id), definition.text,
                                    definition.iconName);
        entry->setCheckable(true);
        layoutGroup->addAction(entry);
        m_documentActions.append(entry);
        const core::PageLayout layout = definition.layout;
        connect(entry, &QAction::triggered, this, [this, layout] {
            if (auto *session = currentSession())
                session->setLayout(layout);
        });
    }
    action(QStringLiteral("layoutContinuous"))->setChecked(true);

    // Navigation
    const struct {
        const char *id;
        QString text;
        QString iconName;
        int delta; ///< 0 means "jump to an absolute edge".
        int absolute;
    } navigation[] = {
        { "firstPage", tr("First Page"), QStringLiteral("first-page"), 0, 0 },
        { "previousPage", tr("Previous Page"), QStringLiteral("chevron-left"), -1, 0 },
        { "nextPage", tr("Next Page"), QStringLiteral("chevron-right"), 1, 0 },
        { "lastPage", tr("Last Page"), QStringLiteral("last-page"), 0, -1 },
    };
    for (const auto &command : navigation) {
        QAction *entry = makeAction(QString::fromLatin1(command.id), command.text,
                                    command.iconName);
        m_documentActions.append(entry);
        const int delta = command.delta;
        const int absolute = command.absolute;
        connect(entry, &QAction::triggered, this, [this, delta, absolute] {
            auto *session = currentSession();
            if (!session)
                return;
            if (delta != 0)
                session->setCurrentPage(session->currentPage() + delta);
            else
                session->setCurrentPage(absolute < 0 ? session->pageCount() - 1 : 0);
        });
    }

    // Window chrome
    // Checked before connecting: the panels these toggles drive do not exist yet.
    QAction *toggleSidebar = makeAction(QStringLiteral("toggleSidebar"), tr("Navigation Panel"),
                                        QStringLiteral("panel-left"),
                                        QKeySequence(Qt::CTRL | Qt::Key_B));
    toggleSidebar->setCheckable(true);
    toggleSidebar->setChecked(true);
    connect(toggleSidebar, &QAction::toggled, this, [this](bool on) {
        if (m_sidebar)
            m_sidebar->setPanelVisible(on);
    });

    QAction *toggleProperties = makeAction(QStringLiteral("toggleProperties"), tr("Properties"),
                                           QStringLiteral("panel-right"),
                                           QKeySequence(Qt::CTRL | Qt::Key_J));
    toggleProperties->setCheckable(true);
    toggleProperties->setChecked(true);
    connect(toggleProperties, &QAction::toggled, this, [this](bool on) {
        if (m_properties)
            m_properties->setVisible(on);
    });

    QAction *theme = makeAction(QStringLiteral("toggleTheme"), tr("Dark Mode"),
                                QStringLiteral("moon"));
    theme->setCheckable(true);
    connect(theme, &QAction::toggled, this, [](bool dark) {
        Theme::instance().setMode(dark ? ThemeMode::Dark : ThemeMode::Light);
    });
    connect(&Theme::instance(), &Theme::changed, theme, [theme] {
        const bool dark = Theme::instance().mode() == ThemeMode::Dark;
        theme->setIcon(icon(dark ? QStringLiteral("sun") : QStringLiteral("moon")));
        theme->setProperty(iconNameProperty,
                           dark ? QStringLiteral("sun") : QStringLiteral("moon"));
        theme->setText(dark ? tr("Light Mode") : tr("Dark Mode"));
    });

    QAction *fullScreen = makeAction(QStringLiteral("fullScreen"), tr("Full Screen"), QString(),
                                     QKeySequence::FullScreen);
    fullScreen->setCheckable(true);
    connect(fullScreen, &QAction::toggled, this, [this](bool on) {
        setWindowState(on ? windowState() | Qt::WindowFullScreen
                          : windowState() & ~Qt::WindowFullScreen);
    });
}

// --- Menus -----------------------------------------------------------------

void MainWindow::createMenuBar()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(action(QStringLiteral("open")));
    m_recentMenu = file->addMenu(tr("Open &Recent"));
    file->addSeparator();
    file->addAction(action(QStringLiteral("save")));
    file->addAction(action(QStringLiteral("saveAs")));
    file->addAction(action(QStringLiteral("export")));
    file->addSeparator();
    file->addAction(action(QStringLiteral("print")));
    file->addSeparator();
    file->addAction(action(QStringLiteral("close")));
    file->addAction(action(QStringLiteral("quit")));

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(action(QStringLiteral("undo")));
    edit->addAction(action(QStringLiteral("redo")));
    edit->addSeparator();
    edit->addAction(action(QStringLiteral("editText")));
    edit->addAction(action(QStringLiteral("editImage")));
    edit->addAction(action(QStringLiteral("addText")));
    edit->addAction(action(QStringLiteral("addImage")));
    edit->addSeparator();
    edit->addAction(action(QStringLiteral("settings")));

    QMenu *view = menuBar()->addMenu(tr("&View"));
    view->addAction(action(QStringLiteral("zoomIn")));
    view->addAction(action(QStringLiteral("zoomOut")));
    view->addAction(action(QStringLiteral("actualSize")));
    view->addAction(action(QStringLiteral("fitPage")));
    view->addAction(action(QStringLiteral("fitWidth")));
    view->addSeparator();
    view->addAction(action(QStringLiteral("layoutSingle")));
    view->addAction(action(QStringLiteral("layoutContinuous")));
    view->addAction(action(QStringLiteral("layoutTwoPage")));
    view->addSeparator();
    view->addAction(action(QStringLiteral("rotateLeft")));
    view->addAction(action(QStringLiteral("rotateRight")));
    view->addSeparator();
    view->addAction(action(QStringLiteral("toggleSidebar")));
    view->addAction(action(QStringLiteral("toggleProperties")));
    view->addAction(action(QStringLiteral("toggleTheme")));
    view->addAction(action(QStringLiteral("fullScreen")));

    QMenu *page = menuBar()->addMenu(tr("&Page"));
    page->addAction(action(QStringLiteral("insertPages")));
    page->addAction(action(QStringLiteral("deletePages")));
    page->addAction(action(QStringLiteral("extractPages")));
    page->addSeparator();
    page->addAction(action(QStringLiteral("merge")));
    page->addAction(action(QStringLiteral("split")));

    QMenu *tools = menuBar()->addMenu(tr("&Tools"));
    tools->addAction(action(QStringLiteral("ocr")));
    tools->addAction(action(QStringLiteral("compress")));
    tools->addAction(action(QStringLiteral("convert")));
    tools->addSeparator();
    tools->addAction(action(QStringLiteral("encrypt")));
    tools->addAction(action(QStringLiteral("redact")));
    tools->addAction(action(QStringLiteral("sign")));

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    connect(help->addAction(tr("About SweetPDF")), &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("About SweetPDF"),
                           tr("<h3>SweetPDF %1</h3>"
                              "<p>An all-in-one, open source PDF editor.</p>"
                              "<p>Built with C++, Qt 6 and MuPDF. Licensed under the "
                              "AGPL-3.0.</p>")
                               .arg(QApplication::applicationVersion()));
    });
    connect(help->addAction(tr("About Qt")), &QAction::triggered, qApp, &QApplication::aboutQt);
}

// --- Ribbon ----------------------------------------------------------------

void MainWindow::createRibbon()
{
    m_ribbon = new RibbonBar(this);

    RibbonPage *home = m_ribbon->addPage(tr("Home"));
    RibbonGroup *fileGroup = home->addGroup(tr("File"));
    fileGroup->addAction(action(QStringLiteral("open")));
    fileGroup->addAction(action(QStringLiteral("save")));
    fileGroup->addAction(action(QStringLiteral("print")));

    RibbonGroup *historyGroup = home->addGroup(tr("History"));
    historyGroup->addAction(action(QStringLiteral("undo")));
    historyGroup->addAction(action(QStringLiteral("redo")));

    RibbonGroup *toolGroup = home->addGroup(tr("Tools"));
    toolGroup->addAction(action(QStringLiteral("toolSelect")));
    toolGroup->addAction(action(QStringLiteral("toolHand")));

    RibbonGroup *contentGroup = home->addGroup(tr("Content"));
    contentGroup->addAction(action(QStringLiteral("editText")));
    contentGroup->addAction(action(QStringLiteral("editImage")));
    contentGroup->addAction(action(QStringLiteral("addText")));
    contentGroup->addAction(action(QStringLiteral("addImage")));

    RibbonGroup *markupGroup = home->addGroup(tr("Markup"));
    markupGroup->addAction(action(QStringLiteral("toolHighlight")));
    markupGroup->addAction(action(QStringLiteral("toolUnderline")));
    markupGroup->addAction(action(QStringLiteral("toolStrikeout")));
    markupGroup->addAction(action(QStringLiteral("toolDraw")));
    markupGroup->addAction(action(QStringLiteral("toolNote")));

    RibbonGroup *organiseGroup = home->addGroup(tr("Organise"));
    organiseGroup->addAction(action(QStringLiteral("ocr")));
    organiseGroup->addAction(action(QStringLiteral("merge")));
    organiseGroup->addAction(action(QStringLiteral("split")));
    organiseGroup->addAction(action(QStringLiteral("compress")));

    RibbonPage *editPage = m_ribbon->addPage(tr("Edit"));
    RibbonGroup *editContent = editPage->addGroup(tr("Content"));
    editContent->addAction(action(QStringLiteral("editText")));
    editContent->addAction(action(QStringLiteral("editImage")));
    editContent->addAction(action(QStringLiteral("addText")));
    editContent->addAction(action(QStringLiteral("addImage")));
    RibbonGroup *editObjects = editPage->addGroup(tr("Objects"));
    editObjects->addAction(action(QStringLiteral("addShape")));
    editObjects->addAction(action(QStringLiteral("addLink")));
    RibbonGroup *editHistory = editPage->addGroup(tr("History"));
    editHistory->addAction(action(QStringLiteral("undo")));
    editHistory->addAction(action(QStringLiteral("redo")));

    RibbonPage *commentPage = m_ribbon->addPage(tr("Comment"));
    RibbonGroup *markup = commentPage->addGroup(tr("Markup"));
    markup->addAction(action(QStringLiteral("toolHighlight")));
    markup->addAction(action(QStringLiteral("toolUnderline")));
    markup->addAction(action(QStringLiteral("toolStrikeout")));
    RibbonGroup *drawing = commentPage->addGroup(tr("Drawing"));
    drawing->addAction(action(QStringLiteral("toolDraw")));
    drawing->addAction(action(QStringLiteral("toolNote")));

    RibbonPage *pagePage = m_ribbon->addPage(tr("Page"));
    RibbonGroup *organise = pagePage->addGroup(tr("Organise"));
    organise->addAction(action(QStringLiteral("insertPages")));
    organise->addAction(action(QStringLiteral("deletePages")));
    organise->addAction(action(QStringLiteral("extractPages")));
    RibbonGroup *transform = pagePage->addGroup(tr("Transform"));
    transform->addAction(action(QStringLiteral("rotateLeft")));
    transform->addAction(action(QStringLiteral("rotateRight")));
    RibbonGroup *documents = pagePage->addGroup(tr("Documents"));
    documents->addAction(action(QStringLiteral("merge")));
    documents->addAction(action(QStringLiteral("split")));

    RibbonPage *viewPage = m_ribbon->addPage(tr("View"));
    RibbonGroup *zoomGroup = viewPage->addGroup(tr("Zoom"));
    zoomGroup->addAction(action(QStringLiteral("zoomOut")));
    zoomGroup->addAction(action(QStringLiteral("zoomIn")));
    zoomGroup->addAction(action(QStringLiteral("actualSize")));
    zoomGroup->addAction(action(QStringLiteral("fitPage")));
    zoomGroup->addAction(action(QStringLiteral("fitWidth")));
    RibbonGroup *layoutGroup = viewPage->addGroup(tr("Layout"));
    layoutGroup->addAction(action(QStringLiteral("layoutSingle")));
    layoutGroup->addAction(action(QStringLiteral("layoutContinuous")));
    layoutGroup->addAction(action(QStringLiteral("layoutTwoPage")));
    RibbonGroup *panelGroup = viewPage->addGroup(tr("Panels"));
    panelGroup->addAction(action(QStringLiteral("toggleSidebar")));
    panelGroup->addAction(action(QStringLiteral("toggleProperties")));
    panelGroup->addAction(action(QStringLiteral("toggleTheme")));

    RibbonPage *protectPage = m_ribbon->addPage(tr("Protect"));
    RibbonGroup *security = protectPage->addGroup(tr("Security"));
    security->addAction(action(QStringLiteral("encrypt")));
    security->addAction(action(QStringLiteral("permissions")));
    security->addAction(action(QStringLiteral("redact")));
    RibbonGroup *signing = protectPage->addGroup(tr("Signing"));
    signing->addAction(action(QStringLiteral("sign")));

    RibbonPage *toolsPage = m_ribbon->addPage(tr("Tools"));
    RibbonGroup *recognise = toolsPage->addGroup(tr("Recognise"));
    recognise->addAction(action(QStringLiteral("ocr")));
    RibbonGroup *optimise = toolsPage->addGroup(tr("Optimise"));
    optimise->addAction(action(QStringLiteral("compress")));
    optimise->addAction(action(QStringLiteral("convert")));
    RibbonGroup *formsGroup = toolsPage->addGroup(tr("Forms"));
    formsGroup->addAction(action(QStringLiteral("forms")));

    auto *themeButton = new QToolButton(m_ribbon);
    themeButton->setDefaultAction(action(QStringLiteral("toggleTheme")));
    themeButton->setIconSize(QSize(16, 16));
    themeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_ribbon->addTabStripWidget(themeButton);

    auto *central = qobject_cast<QVBoxLayout *>(centralWidget()->layout());
    central->insertWidget(0, m_ribbon);
}

// --- Central area ----------------------------------------------------------

void MainWindow::createCentralArea()
{
    auto *central = new QWidget(this);
    auto *column = new QVBoxLayout(central);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    setCentralWidget(central);

    m_splitter = new QSplitter(Qt::Horizontal, central);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);
    column->addWidget(m_splitter, 1);

    m_sidebar = new Sidebar(m_splitter);
    m_splitter->addWidget(m_sidebar);

    m_center = new QStackedWidget(m_splitter);
    m_splitter->addWidget(m_center);

    m_tabs = new QTabWidget(m_center);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_center->addWidget(m_tabs);

    m_welcome = new QWidget(m_center);
    auto *welcomeLayout = new QVBoxLayout(m_welcome);
    welcomeLayout->setContentsMargins(48, 48, 48, 48);
    welcomeLayout->setSpacing(12);
    welcomeLayout->addStretch(1);

    auto *logo = new QLabel(m_welcome);
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(Theme::instance()
                        .icon(QStringLiteral("logo"), Theme::instance().palette().accent)
                        .pixmap(64, 64));
    welcomeLayout->addWidget(logo);

    auto *heading = new QLabel(tr("SweetPDF"), m_welcome);
    heading->setAlignment(Qt::AlignCenter);
    QFont headingFont = heading->font();
    headingFont.setPointSizeF(headingFont.pointSizeF() * 1.8);
    headingFont.setWeight(QFont::DemiBold);
    heading->setFont(headingFont);
    welcomeLayout->addWidget(heading);

    auto *subtitle = new QLabel(tr("Open a PDF to get started, or drop one anywhere in "
                                   "this window."),
                                m_welcome);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setObjectName(QStringLiteral("SectionLabel"));
    welcomeLayout->addWidget(subtitle);

    auto *openRow = new QHBoxLayout;
    openRow->addStretch(1);
    auto *openButton = new QPushButton(icon(QStringLiteral("open")), tr("Open PDF"), m_welcome);
    openButton->setProperty("accent", true);
    connect(openButton, &QPushButton::clicked, this, &MainWindow::openFileDialog);
    openRow->addWidget(openButton);
    openRow->addStretch(1);
    welcomeLayout->addSpacing(8);
    welcomeLayout->addLayout(openRow);
    welcomeLayout->addStretch(2);
    m_center->addWidget(m_welcome);

    m_properties = new PropertiesPanel(m_splitter);
    m_splitter->addWidget(m_properties);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_splitter->setSizes({ 266, 800, 262 });

    connect(m_tabs, &QTabWidget::currentChanged, this,
            [this](int) { updateForCurrentDocument(); });
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *page = m_tabs->widget(index);
        m_tabs->removeTab(index);
        page->deleteLater();
        showWelcomeIfEmpty();
        updateForCurrentDocument();
    });
    connect(m_sidebar, &Sidebar::panelVisibilityChanged, this, [this](bool visible) {
        QAction *toggle = action(QStringLiteral("toggleSidebar"));
        if (toggle->isChecked() != visible) {
            const QSignalBlocker blocker(toggle);
            toggle->setChecked(visible);
        }
    });

    m_center->setCurrentWidget(m_welcome);
}

void MainWindow::createStatusBar()
{
    auto *bar = statusBar();
    bar->setSizeGripEnabled(true);

    m_pageLabel = new QLabel(tr("No document"), bar);
    bar->addWidget(m_pageLabel);

    const auto addStatusButton = [this, bar](const QString &id) {
        auto *button = new QToolButton(bar);
        button->setDefaultAction(action(id));
        button->setIconSize(QSize(16, 16));
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        bar->addWidget(button);
        return button;
    };

    addStatusButton(QStringLiteral("previousPage"));
    addStatusButton(QStringLiteral("nextPage"));

    bar->addPermanentWidget(new QLabel(tr("Zoom"), bar));

    auto *zoomOutButton = new QToolButton(bar);
    zoomOutButton->setDefaultAction(action(QStringLiteral("zoomOut")));
    zoomOutButton->setIconSize(QSize(16, 16));
    zoomOutButton->setAutoRaise(true);
    bar->addPermanentWidget(zoomOutButton);

    m_zoomSlider = new QSlider(Qt::Horizontal, bar);
    m_zoomSlider->setRange(0, zoomSliderResolution);
    m_zoomSlider->setFixedWidth(140);
    m_zoomSlider->setValue(zoomToSlider(1.0));
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int value) {
        if (PageView *view = currentView())
            view->zoomTo(sliderToZoom(value));
    });
    bar->addPermanentWidget(m_zoomSlider);

    auto *zoomInButton = new QToolButton(bar);
    zoomInButton->setDefaultAction(action(QStringLiteral("zoomIn")));
    zoomInButton->setIconSize(QSize(16, 16));
    zoomInButton->setAutoRaise(true);
    bar->addPermanentWidget(zoomInButton);

    m_zoomLabel = new QLabel(QStringLiteral("100%"), bar);
    m_zoomLabel->setMinimumWidth(46);
    m_zoomLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bar->addPermanentWidget(m_zoomLabel);

    for (const QString &id : { QStringLiteral("fitWidth"), QStringLiteral("fitPage"),
                               QStringLiteral("toggleSidebar"),
                               QStringLiteral("toggleProperties"),
                               QStringLiteral("toggleTheme") }) {
        auto *button = new QToolButton(bar);
        button->setDefaultAction(action(id));
        button->setIconSize(QSize(16, 16));
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        bar->addPermanentWidget(button);
    }
}

// --- Documents -------------------------------------------------------------

PageView *MainWindow::viewAt(int tabIndex) const
{
    QWidget *page = m_tabs->widget(tabIndex);
    return page ? page->findChild<PageView *>() : nullptr;
}

PageView *MainWindow::currentView() const
{
    return viewAt(m_tabs->currentIndex());
}

core::DocumentSession *MainWindow::currentSession() const
{
    PageView *view = currentView();
    return view ? view->session() : nullptr;
}

int MainWindow::tabForPath(const QString &path) const
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    for (int index = 0; index < m_tabs->count(); ++index) {
        if (PageView *view = viewAt(index)) {
            if (QFileInfo(view->session()->filePath()).canonicalFilePath() == canonical)
                return index;
        }
    }
    return -1;
}

bool MainWindow::openFile(const QString &path)
{
    if (path.isEmpty())
        return false;

    if (const int existing = tabForPath(path); existing >= 0) {
        m_tabs->setCurrentIndex(existing);
        return true;
    }

    auto *session = new core::DocumentSession(this);

    QString password;
    if (core::PdfDocument::needsPassword(path)) {
        bool accepted = false;
        password = QInputDialog::getText(this, tr("Password required"),
                                        tr("Enter the password for %1:")
                                            .arg(QFileInfo(path).fileName()),
                                        QLineEdit::Password, QString(), &accepted);
        if (!accepted) {
            session->deleteLater();
            return false;
        }
    }

    QString error;
    if (!session->load(path, password, &error)) {
        session->deleteLater();
        QMessageBox::warning(this, tr("Could not open the document"),
                             tr("%1\n\n%2").arg(QFileInfo(path).fileName(), error));
        return false;
    }

    auto *page = new QWidget(m_tabs);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *view = new PageView(session, page);
    layout->addWidget(view);
    new ViewerToolbar(view);

    session->setParent(view);

    const int index = m_tabs->addTab(page, session->displayName());
    m_tabs->setTabToolTip(index, path);
    m_tabs->setCurrentIndex(index);
    m_center->setCurrentWidget(m_tabs);

    connect(session, &core::DocumentSession::modifiedChanged, this, [this, session](bool dirty) {
        for (int tab = 0; tab < m_tabs->count(); ++tab) {
            if (viewAt(tab) && viewAt(tab)->session() == session) {
                m_tabs->setTabText(tab, dirty ? session->displayName() + QLatin1String(" *")
                                              : session->displayName());
            }
        }
        updateWindowTitleFromSession();
    });

    addRecentFile(path);
    updateForCurrentDocument();
    return true;
}

void MainWindow::openFileDialog()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Open PDF"), QString(),
        tr("Documents (*.pdf *.xps *.epub *.cbz *.svg);;PDF files (*.pdf);;All files (*)"));
    for (const QString &path : paths)
        openFile(path);
}

void MainWindow::closeCurrentDocument()
{
    const int index = m_tabs->currentIndex();
    if (index < 0)
        return;

    QWidget *page = m_tabs->widget(index);
    m_tabs->removeTab(index);
    page->deleteLater();
    showWelcomeIfEmpty();
    updateForCurrentDocument();
}

void MainWindow::showWelcomeIfEmpty()
{
    m_center->setCurrentWidget(m_tabs->count() == 0 ? m_welcome : static_cast<QWidget *>(m_tabs));
}

void MainWindow::connectSessionSignals()
{
    auto *session = currentSession();
    if (!session)
        return;

    connect(session, &core::DocumentSession::currentPageChanged, this,
            &MainWindow::updateForCurrentDocument, Qt::UniqueConnection);
    connect(session, &core::DocumentSession::zoomChanged, this,
            &MainWindow::updateForCurrentDocument, Qt::UniqueConnection);
    connect(session->undoStack(), &QUndoStack::canUndoChanged,
            action(QStringLiteral("undo")), &QAction::setEnabled, Qt::UniqueConnection);
    connect(session->undoStack(), &QUndoStack::canRedoChanged,
            action(QStringLiteral("redo")), &QAction::setEnabled, Qt::UniqueConnection);
    connect(action(QStringLiteral("undo")), &QAction::triggered, session->undoStack(),
            &QUndoStack::undo, Qt::UniqueConnection);
    connect(action(QStringLiteral("redo")), &QAction::triggered, session->undoStack(),
            &QUndoStack::redo, Qt::UniqueConnection);
}

void MainWindow::updateForCurrentDocument()
{
    auto *session = currentSession();
    const bool hasDocument = session && session->isLoaded();

    for (QAction *entry : std::as_const(m_documentActions))
        entry->setEnabled(hasDocument);

    m_sidebar->setSession(session);
    m_properties->setSession(session);

    if (!hasDocument) {
        m_pageLabel->setText(tr("No document"));
        m_zoomLabel->setText(QStringLiteral("—"));
        action(QStringLiteral("undo"))->setEnabled(false);
        action(QStringLiteral("redo"))->setEnabled(false);
        setWindowTitle(tr("SweetPDF"));
        return;
    }

    connectSessionSignals();

    m_pageLabel->setText(tr("Page %1 of %2")
                             .arg(session->currentPage() + 1)
                             .arg(session->pageCount()));
    m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(session->zoom() * 100)));

    const QSignalBlocker blocker(m_zoomSlider);
    m_zoomSlider->setValue(zoomToSlider(session->zoom()));

    action(QStringLiteral("undo"))->setEnabled(session->undoStack()->canUndo());
    action(QStringLiteral("redo"))->setEnabled(session->undoStack()->canRedo());
    updateWindowTitleFromSession();
}

void MainWindow::updateWindowTitleFromSession()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded()) {
        setWindowTitle(tr("SweetPDF"));
        return;
    }
    setWindowTitle(tr("%1%2 — SweetPDF")
                       .arg(session->displayName(),
                            session->isModified() ? QStringLiteral(" *") : QString()));
}

// --- Recent files ----------------------------------------------------------

QStringList MainWindow::recentFiles() const
{
    QSettings settings;
    return settings.value(QLatin1String(recentFilesKey)).toStringList();
}

void MainWindow::addRecentFile(const QString &path)
{
    QStringList files = recentFiles();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > recentFileLimit)
        files.removeLast();

    QSettings settings;
    settings.setValue(QLatin1String(recentFilesKey), files);
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu()
{
    if (!m_recentMenu)
        return;

    m_recentMenu->clear();
    const QStringList files = recentFiles();
    if (files.isEmpty()) {
        QAction *empty = m_recentMenu->addAction(tr("No recent documents"));
        empty->setEnabled(false);
        return;
    }

    for (const QString &path : files) {
        QAction *entry = m_recentMenu->addAction(QFileInfo(path).fileName());
        entry->setToolTip(path);
        connect(entry, &QAction::triggered, this, [this, path] { openFile(path); });
    }
    m_recentMenu->addSeparator();
    connect(m_recentMenu->addAction(tr("Clear list")), &QAction::triggered, this, [this] {
        QSettings settings;
        settings.remove(QLatin1String(recentFilesKey));
        rebuildRecentMenu();
    });
}

// --- Events ----------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event)
{
    QStringList unsaved;
    for (int index = 0; index < m_tabs->count(); ++index) {
        if (PageView *view = viewAt(index)) {
            if (view->session()->isModified())
                unsaved << view->session()->displayName();
        }
    }

    if (!unsaved.isEmpty()) {
        const auto choice = QMessageBox::question(
            this, tr("Unsaved changes"),
            tr("These documents have unsaved changes:\n\n%1\n\nClose anyway?")
                .arg(unsaved.join(QLatin1String("\n"))),
            QMessageBox::Cancel | QMessageBox::Discard, QMessageBox::Cancel);
        if (choice == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
    }

    QSettings settings;
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("splitter"), m_splitter->saveState());
    settings.setValue(QStringLiteral("darkMode"), Theme::instance().mode() == ThemeMode::Dark);
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            openFile(url.toLocalFile());
    }
    event->acceptProposedAction();
}

} // namespace sweet::ui
