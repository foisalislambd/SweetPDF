#include "ui/window/MainWindow.h"

#include "core/pdf/PdfDocument.h"
#include "core/pdf/PdfExport.h"
#include "core/session/DocumentSession.h"
#include "ui/dialogs/CompareDialog.h"
#include "ui/dialogs/ExportImagesDialog.h"
#include "ui/dialogs/HeaderFooterDialog.h"
#include "ui/dialogs/OcrDialog.h"
#include "ui/dialogs/PageRangeInput.h"
#include "ui/dialogs/PreferencesDialog.h"
#include "ui/dialogs/SecurityDialog.h"
#include "ui/dialogs/SignatureDialog.h"
#include "ui/dialogs/WatermarkDialog.h"
#include "ui/panels/PropertiesPanel.h"
#include "ui/panels/Sidebar.h"
#include "ui/panels/ToolOptionsPanel.h"
#include "ui/ribbon/RibbonBar.h"
#include "ui/theme/Theme.h"
#include "ui/viewer/FindBar.h"
#include "ui/viewer/PageView.h"
#include "ui/viewer/ViewerToolbar.h"
#include "ui/window/WelcomeView.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QListWidget>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
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

core::AnnotationSpec copyOf(const core::Annotation &annotation, int pageIndex, const QPointF &delta)
{
    core::AnnotationSpec spec;
    spec.type = annotation.type;
    spec.pageIndex = pageIndex;
    spec.rect = annotation.rect.translated(delta);
    for (const QRectF &quad : annotation.quads)
        spec.quads.append(quad.translated(delta));
    for (const QPolygonF &stroke : annotation.strokes) {
        QPolygonF moved;
        for (const QPointF &point : stroke)
            moved.append(point + delta);
        spec.strokes.append(moved);
    }
    spec.lineStart = annotation.lineStart + delta;
    spec.lineEnd = annotation.lineEnd + delta;
    spec.hasLineEndpoints = annotation.type == core::AnnotationType::Line;
    spec.lineStartStyle = annotation.lineStartStyle;
    spec.lineEndStyle = annotation.lineEndStyle;
    spec.color = annotation.color;
    spec.opacity = annotation.opacity;
    spec.borderWidth = annotation.borderWidth;
    spec.contents = annotation.contents;
    spec.author = annotation.author;
    spec.iconName = annotation.iconName;
    return spec;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("SweetPDF"));
    setAcceptDrops(true);
    // Narrower than this and the page area disappears under chrome; the layout
    // modes below keep it usable from here up to a large monitor.
    setMinimumSize(1024, 640);
    resize(1360, 880);

    createActions();
    createCentralArea();
    createRibbon();
    createAppMenu();
    createStatusBar();
    showWelcomeIfEmpty();

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
        clampSplitterSizes();
    }
    if (settings.value(QStringLiteral("darkMode"), false).toBool())
        action(QStringLiteral("toggleTheme"))->setChecked(true);

    // Geometry may have been restored above; pick the chrome density for the
    // actual width once, before any queued openFile calls run.
    applyLayoutMode(true);

    if (settings.value(PreferencesDialog::restoreSessionKey(), true).toBool()) {
        // Opening is deferred so the window is on screen first: loading a large
        // document should not delay the window appearing.
        const QStringList last =
            settings.value(PreferencesDialog::openDocumentsKey()).toStringList();
        for (const QString &path : last) {
            if (QFileInfo::exists(path))
                QMetaObject::invokeMethod(this, [this, path] { openFile(path); },
                                          Qt::QueuedConnection);
        }
    }
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
    connect(makeAction(QStringLiteral("newDocument"), tr("New"), QStringLiteral("file-pdf"),
                       QKeySequence::New),
            &QAction::triggered, this, &MainWindow::newDocument);
    connect(makeAction(QStringLiteral("fromImages"), tr("PDF from Images"), QStringLiteral("add-image"),
                       QKeySequence()),
            &QAction::triggered, this, &MainWindow::newDocumentFromImages);

    // Every command that needs an open document is registered the same way, so
    // enabling and disabling them stays in one place.
    const auto addDocumentAction = [this](const QString &id, const QString &text,
                                          const QString &iconName, const QKeySequence &shortcut,
                                          const std::function<void()> &run) {
        QAction *entry = makeAction(id, text, iconName, shortcut);
        m_documentActions.append(entry);
        connect(entry, &QAction::triggered, this, run);
        return entry;
    };

    addDocumentAction(QStringLiteral("save"), tr("Save"), QStringLiteral("save"),
                      QKeySequence::Save, [this] { saveDocument(); });
    addDocumentAction(QStringLiteral("saveAs"), tr("Save As"), QStringLiteral("save-as"),
                      QKeySequence::SaveAs, [this] { saveDocumentAs(false); });
    addDocumentAction(QStringLiteral("saveCopy"), tr("Save a Copy"), QStringLiteral("duplicate"),
                      QKeySequence(), [this] { saveDocumentAs(true); });
    addDocumentAction(QStringLiteral("print"), tr("Print"), QStringLiteral("print"),
                      QKeySequence::Print, [this] { printDocument(); });
    addDocumentAction(QStringLiteral("exportImages"), tr("Images"), QStringLiteral("export"),
                      QKeySequence(), [this] { exportImages(); });
    addDocumentAction(QStringLiteral("exportText"), tr("Plain text"), QStringLiteral("text-select"),
                      QKeySequence(), [this] { exportText(); });
    addDocumentAction(QStringLiteral("exportHtml"), tr("HTML"), QStringLiteral("convert"),
                      QKeySequence(), [this] { exportHtml(); });

    // Find and clipboard
    addDocumentAction(QStringLiteral("find"), tr("Find"), QStringLiteral("search"),
                      QKeySequence::Find, [this] { showFindBar(); });
    addDocumentAction(QStringLiteral("findNext"), tr("Find Next"), QStringLiteral("chevron-down"),
                      QKeySequence::FindNext, [this] {
                          if (FindBar *bar = currentFindBar())
                              bar->findNext();
                      });
    addDocumentAction(QStringLiteral("findPrevious"), tr("Find Previous"),
                      QStringLiteral("chevron-up"), QKeySequence::FindPrevious, [this] {
                          if (FindBar *bar = currentFindBar())
                              bar->findPrevious();
                      });
    addDocumentAction(QStringLiteral("copy"), tr("Copy"), QStringLiteral("copy"),
                      QKeySequence::Copy, [this] {
                          if (PageView *view = currentView())
                              view->copySelection();
                      });
    addDocumentAction(QStringLiteral("selectAll"), tr("Select All"),
                      QStringLiteral("text-select"), QKeySequence::SelectAll, [this] {
                          if (PageView *view = currentView())
                              view->selectAllOnCurrentPage();
                      });

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
        { "toolSquiggly", tr("Squiggly"), QStringLiteral("squiggly"), core::Tool::Squiggly,
          QKeySequence() },
        { "toolDraw", tr("Draw"), QStringLiteral("draw"), core::Tool::Draw, QKeySequence() },
        { "toolNote", tr("Note"), QStringLiteral("note"), core::Tool::Note, QKeySequence() },
        { "addText", tr("Add Text"), QStringLiteral("add-text"), core::Tool::AddText,
          QKeySequence() },
        { "addShape", tr("Rectangle"), QStringLiteral("shape"), core::Tool::Shape,
          QKeySequence() },
        { "addEllipse", tr("Ellipse"), QStringLiteral("ellipse"), core::Tool::Ellipse,
          QKeySequence() },
        { "addLine", tr("Line"), QStringLiteral("line"), core::Tool::Line, QKeySequence() },
        { "addArrow", tr("Arrow"), QStringLiteral("arrow"), core::Tool::Arrow, QKeySequence() },
        { "toolStamp", tr("Stamp"), QStringLiteral("stamp"), core::Tool::Stamp, QKeySequence() },
        { "toolRedact", tr("Mark Redaction"), QStringLiteral("redact"), core::Tool::Redact,
          QKeySequence() },
        { "toolEditText", tr("Edit Text"), QStringLiteral("edit-text"), core::Tool::EditText,
          QKeySequence(Qt::Key_T) },
    };

    for (const auto &definition : tools) {
        QAction *entry = makeAction(QString::fromLatin1(definition.id), definition.text,
                                    definition.iconName, definition.shortcut);
        entry->setCheckable(true);
        toolGroup->addAction(entry);
        m_documentActions.append(entry);
        const core::Tool tool = definition.tool;
        connect(entry, &QAction::triggered, this, [this, tool] {
            if (auto *session = currentSession()) {
                session->setTool(tool);
                revealToolOptions();
            }
        });
    }
    action(QStringLiteral("toolSelect"))->setChecked(true);

    QAction *deleteAnnotation = makeAction(QStringLiteral("deleteAnnotation"), tr("Delete"),
                                          QStringLiteral("trash"));
    m_documentActions.append(deleteAnnotation);
    connect(deleteAnnotation, &QAction::triggered, this, [this] {
        if (PageView *view = currentView(); view && !view->deleteSelectedAnnotation())
            report(tr("Select a comment on the page first."));
    });

    // Pages
    addDocumentAction(QStringLiteral("insertPages"), tr("Insert"), QStringLiteral("page-insert"),
                      QKeySequence(), [this] { insertBlankPage(); });
    addDocumentAction(QStringLiteral("insertFromFile"), tr("From File"), QStringLiteral("open"),
                      QKeySequence(), [this] {
                          if (auto *session = currentSession())
                              insertPagesFromFile(session->currentPage());
                      });
    addDocumentAction(QStringLiteral("deletePages"), tr("Delete"), QStringLiteral("page-delete"),
                      QKeySequence(), [this] { deleteCurrentPages(); });
    addDocumentAction(QStringLiteral("extractPages"), tr("Extract"),
                      QStringLiteral("page-extract"), QKeySequence(), [this] {
                          if (auto *session = currentSession())
                              extractPages({ session->currentPage() });
                      });
    addDocumentAction(QStringLiteral("duplicatePage"), tr("Duplicate"),
                      QStringLiteral("duplicate"), QKeySequence(), [this] {
                          auto *session = currentSession();
                          if (!session || !requireEditableDocument(true))
                              return;
                          const int page = session->currentPage();
                          QString error;
                          const bool copied = session->applyStructuralEdit(
                              tr("Duplicate page"),
                              [page](core::PdfDocument &document) { document.duplicatePage(page); },
                              &error);
                          report(copied ? tr("Duplicated page %1.").arg(page + 1) : error);
                      });
    addDocumentAction(QStringLiteral("merge"), tr("Merge"), QStringLiteral("merge"),
                      QKeySequence(), [this] { mergeDocuments(); });
    addDocumentAction(QStringLiteral("split"), tr("Split"), QStringLiteral("split"),
                      QKeySequence(), [this] { splitDocument(); });
    addDocumentAction(QStringLiteral("reversePages"), tr("Reverse Pages"), QStringLiteral("pages"),
                      QKeySequence(), [this] { reversePages(); });
    addDocumentAction(QStringLiteral("rotateAllPages"), tr("Rotate All Pages"),
                      QStringLiteral("rotate-right"), QKeySequence(), [this] { rotateAllPages(); });
    addDocumentAction(QStringLiteral("goToPage"), tr("Go to Page"), QStringLiteral("search"),
                      QKeySequence(Qt::CTRL | Qt::Key_G), [this] { goToPage(); });
    addDocumentAction(QStringLiteral("copyPageImage"), tr("Copy Page Image"), QStringLiteral("copy"),
                      QKeySequence(), [this] { copyPageImage(); });
    addDocumentAction(QStringLiteral("presentation"), tr("Presentation"), QStringLiteral("eye"),
                      QKeySequence(Qt::Key_F5), [this] { setPresentation(!m_presenting); });
    addDocumentAction(QStringLiteral("duplicateSelection"), tr("Duplicate Selection"),
                      QStringLiteral("duplicate"), QKeySequence(),
                      [this] { duplicateSelection(); });
    addDocumentAction(QStringLiteral("copySelectionPages"), tr("Copy to All Pages"),
                      QStringLiteral("pages"), QKeySequence(),
                      [this] { copySelectionAcrossPages(); });
    addDocumentAction(QStringLiteral("clearDescription"), tr("Clear Description"),
                      QStringLiteral("trash"), QKeySequence(), [this] { clearDescription(); });
    addDocumentAction(QStringLiteral("deleteOddEven"), tr("Delete Odd or Even"),
                      QStringLiteral("page-delete"), QKeySequence(),
                      [this] { deleteOddOrEvenPages(); });
    addDocumentAction(QStringLiteral("grayscale"), tr("Grayscale"), QStringLiteral("layers"),
                      QKeySequence(), [this] { convertToGrayscale(); });
    addDocumentAction(QStringLiteral("removeComments"), tr("Remove Comments"),
                      QStringLiteral("trash"), QKeySequence(), [this] { removeAllComments(); });
    addDocumentAction(QStringLiteral("exportComments"), tr("Export Comments"),
                      QStringLiteral("export"), QKeySequence(), [this] { exportComments(); });
    addDocumentAction(QStringLiteral("documentStats"), tr("Document Info"), QStringLiteral("info"),
                      QKeySequence(), [this] { showDocumentStats(); });
    addDocumentAction(QStringLiteral("reload"), tr("Reload"), QStringLiteral("open"),
                      QKeySequence(), [this] { reloadDocument(); });
    addDocumentAction(QStringLiteral("revealFile"), tr("Show in Folder"), QStringLiteral("folder"),
                      QKeySequence(), [this] { revealDocument(); });

    // Page rotation acts on the document, unlike the view rotation above.
    addDocumentAction(QStringLiteral("rotatePagesLeft"), tr("Rotate Page Left"),
                      QStringLiteral("rotate-left"), QKeySequence(), [this] {
                          auto *session = currentSession();
                          if (!session || !requireEditableDocument(true))
                              return;
                          const int page = session->currentPage();
                          QString error;
                          session->applyStructuralEdit(
                              tr("Rotate page left"),
                              [page](core::PdfDocument &document) {
                                  document.setPageRotation(page, document.pageRotation(page) - 90);
                              },
                              &error);
                          if (!error.isEmpty())
                              report(error);
                      });
    addDocumentAction(QStringLiteral("rotatePagesRight"), tr("Rotate Page Right"),
                      QStringLiteral("rotate-right"), QKeySequence(), [this] {
                          auto *session = currentSession();
                          if (!session || !requireEditableDocument(true))
                              return;
                          const int page = session->currentPage();
                          QString error;
                          session->applyStructuralEdit(
                              tr("Rotate page right"),
                              [page](core::PdfDocument &document) {
                                  document.setPageRotation(page, document.pageRotation(page) + 90);
                              },
                              &error);
                          if (!error.isEmpty())
                              report(error);
                      });

    // Tools
    addDocumentAction(QStringLiteral("ocr"), tr("OCR"), QStringLiteral("ocr"), QKeySequence(),
                      [this] { recogniseText(); });
    addDocumentAction(QStringLiteral("compress"), tr("Compress"), QStringLiteral("compress"),
                      QKeySequence(), [this] { compressDocument(); });
    addDocumentAction(QStringLiteral("watermark"), tr("Watermark"), QStringLiteral("watermark"),
                      QKeySequence(), [this] { addWatermark(); });
    addDocumentAction(QStringLiteral("headerFooter"), tr("Header and Footer"),
                      QStringLiteral("text-select"), QKeySequence(), [this] { addHeaderFooter(); });
    addDocumentAction(QStringLiteral("cropPage"), tr("Crop Page"), QStringLiteral("crop"),
                      QKeySequence(), [this] { cropCurrentPage(); });
    addDocumentAction(QStringLiteral("compare"), tr("Compare"), QStringLiteral("pages"),
                      QKeySequence(), [this] { compareDocument(); });
    addDocumentAction(QStringLiteral("encrypt"), tr("Protect"), QStringLiteral("lock"),
                      QKeySequence(), [this] { protectDocument(); });
    addDocumentAction(QStringLiteral("applyRedactions"), tr("Apply Redactions"),
                      QStringLiteral("shield"), QKeySequence(), [this] { applyRedactions(); });

    addDocumentAction(QStringLiteral("flatten"), tr("Flatten"), QStringLiteral("layers"),
                      QKeySequence(), [this] { flattenDocument(); });

    // Content
    addDocumentAction(QStringLiteral("editText"), tr("Edit Text"),
                      QStringLiteral("edit-text"), QKeySequence(), [this] {
                          // Arm the edit tool so a click on the page opens the
                          // inline editor. If text is already selected, edit it
                          // straight away - that is what the menu item means.
                          if (PageView *view = currentView(); view && view->hasSelection()) {
                              replaceSelectedText();
                              return;
                          }
                          if (QAction *tool = action(QStringLiteral("toolEditText")))
                              tool->trigger();
                      });
    addDocumentAction(QStringLiteral("editImage"), tr("Replace Image"),
                      QStringLiteral("edit-image"), QKeySequence(),
                      [this] { replaceSelectedImage(); });
    addDocumentAction(QStringLiteral("addImage"), tr("Add Image"), QStringLiteral("add-image"),
                      QKeySequence(), [this] { addImageToPage(); });
    addDocumentAction(QStringLiteral("addLink"), tr("Link"), QStringLiteral("link"), QKeySequence(),
                      [this] { addLinkToPage(); });
    addDocumentAction(QStringLiteral("manageLinks"), tr("Edit Links"), QStringLiteral("link"),
                      QKeySequence(), [this] { manageLinks(); });
    addDocumentAction(QStringLiteral("replaceAll"), tr("Replace All"), QStringLiteral("edit-text"),
                      QKeySequence(), [this] { replaceAllText(); });
    addDocumentAction(QStringLiteral("addFormField"), tr("New Field"), QStringLiteral("form"),
                      QKeySequence(), [this] { addFormField(); });
    addDocumentAction(QStringLiteral("customStamp"), tr("Image Stamp"), QStringLiteral("stamp"),
                      QKeySequence(), [this] { chooseCustomStamp(); });
    addDocumentAction(QStringLiteral("replyAnnotation"), tr("Reply"), QStringLiteral("comment"),
                      QKeySequence(), [this] { replyToSelection(); });
    addDocumentAction(QStringLiteral("sign"), tr("Sign"), QStringLiteral("sign"), QKeySequence(),
                      [this] { signDocument(); });
    addDocumentAction(QStringLiteral("forms"), tr("Form Fields"), QStringLiteral("form"),
                      QKeySequence(), [this] {
                          m_sidebar->showPanel(Sidebar::Panel::Forms);
                          m_sidebar->setPanelVisible(true);
                      });

    connect(makeAction(QStringLiteral("settings"), tr("Settings"), QStringLiteral("settings"),
                       QKeySequence::Preferences),
            &QAction::triggered, this, &MainWindow::showPreferences);

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
    connect(toggleProperties, &QAction::toggled, this, [this](bool) {
        updateInspectorVisibility();
    });

    QAction *night = makeAction(QStringLiteral("nightMode"), tr("Night Reading"),
                                QStringLiteral("moon"));
    night->setCheckable(true);
    connect(night, &QAction::toggled, this, [this](bool on) {
        if (auto *session = currentSession())
            session->setNightMode(on);
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

    const auto presentKey = [this](const QKeySequence &key, int delta) {
        auto *entry = new QAction(this);
        entry->setShortcut(key);
        entry->setShortcutContext(Qt::ApplicationShortcut);
        entry->setEnabled(false);
        addAction(entry);
        connect(entry, &QAction::triggered, this, [this, delta] {
            if (!m_presenting)
                return;
            if (delta == 0)
                setPresentation(false);
            else if (auto *session = currentSession())
                session->setCurrentPage(session->currentPage() + delta);
        });
        m_presentKeys.append(entry);
    };
    presentKey(QKeySequence(Qt::Key_Right), 1);
    presentKey(QKeySequence(Qt::Key_Down), 1);
    presentKey(QKeySequence(Qt::Key_Space), 1);
    presentKey(QKeySequence(Qt::Key_PageDown), 1);
    presentKey(QKeySequence(Qt::Key_Left), -1);
    presentKey(QKeySequence(Qt::Key_Up), -1);
    presentKey(QKeySequence(Qt::Key_PageUp), -1);
    presentKey(QKeySequence(Qt::Key_Escape), 0);

    // Short captions for the ribbon cells. Menus and tooltips keep the full text;
    // long names like "Rotate Page Left" are hard to scan under an icon.
    const auto shortCaption = [this](const QString &id, const QString &caption) {
        if (QAction *entry = action(id))
            entry->setIconText(caption);
    };
    shortCaption(QStringLiteral("toolStrikeout"), tr("Strike"));
    shortCaption(QStringLiteral("toolHighlight"), tr("Highlight"));
    shortCaption(QStringLiteral("toolUnderline"), tr("Underline"));
    shortCaption(QStringLiteral("toolSquiggly"), tr("Squiggly"));
    shortCaption(QStringLiteral("addText"), tr("Text"));
    shortCaption(QStringLiteral("toolEditText"), tr("Edit"));
    shortCaption(QStringLiteral("addLine"), tr("Line"));
    shortCaption(QStringLiteral("addArrow"), tr("Arrow"));
    shortCaption(QStringLiteral("toolStamp"), tr("Stamp"));
    shortCaption(QStringLiteral("toolRedact"), tr("Redact"));
    shortCaption(QStringLiteral("applyRedactions"), tr("Apply"));
    shortCaption(QStringLiteral("rotatePagesLeft"), tr("Left"));
    shortCaption(QStringLiteral("rotatePagesRight"), tr("Right"));
    shortCaption(QStringLiteral("rotateLeft"), tr("Left"));
    shortCaption(QStringLiteral("rotateRight"), tr("Right"));
    shortCaption(QStringLiteral("editImage"), tr("Replace"));
    shortCaption(QStringLiteral("addImage"), tr("Image"));
    shortCaption(QStringLiteral("exportText"), tr("Text"));
    shortCaption(QStringLiteral("exportImages"), tr("Images"));
    shortCaption(QStringLiteral("forms"), tr("Forms"));
    shortCaption(QStringLiteral("layoutSingle"), tr("Single"));
    shortCaption(QStringLiteral("layoutContinuous"), tr("Scroll"));
    shortCaption(QStringLiteral("layoutTwoPage"), tr("Two"));
    shortCaption(QStringLiteral("fitPage"), tr("Page"));
    shortCaption(QStringLiteral("fitWidth"), tr("Width"));
    shortCaption(QStringLiteral("toggleSidebar"), tr("Nav"));
    shortCaption(QStringLiteral("toggleProperties"), tr("Props"));
    shortCaption(QStringLiteral("headerFooter"), tr("Header"));
    shortCaption(QStringLiteral("cropPage"), tr("Crop"));
    shortCaption(QStringLiteral("replaceAll"), tr("Replace"));
    shortCaption(QStringLiteral("addFormField"), tr("Field"));
    shortCaption(QStringLiteral("manageLinks"), tr("Links"));
    shortCaption(QStringLiteral("compare"), tr("Compare"));
    shortCaption(QStringLiteral("nightMode"), tr("Night"));
    shortCaption(QStringLiteral("reversePages"), tr("Reverse"));
    shortCaption(QStringLiteral("rotateAllPages"), tr("All"));
    shortCaption(QStringLiteral("goToPage"), tr("Go to"));
    shortCaption(QStringLiteral("copyPageImage"), tr("Copy"));
    shortCaption(QStringLiteral("presentation"), tr("Present"));
    shortCaption(QStringLiteral("duplicateSelection"), tr("Copy"));
    shortCaption(QStringLiteral("copySelectionPages"), tr("All pages"));
    shortCaption(QStringLiteral("clearDescription"), tr("Clear"));
    shortCaption(QStringLiteral("newDocument"), tr("New"));
    shortCaption(QStringLiteral("fromImages"), tr("Images"));
    shortCaption(QStringLiteral("grayscale"), tr("Gray"));
    shortCaption(QStringLiteral("documentStats"), tr("Info"));
}

// --- Menus -----------------------------------------------------------------

void MainWindow::createAppMenu()
{
    m_appMenu = new QMenu(this);

    QMenu *file = m_appMenu->addMenu(tr("&File"));
    file->setIcon(icon(QStringLiteral("file-pdf")));
    file->addAction(action(QStringLiteral("newDocument")));
    file->addAction(action(QStringLiteral("fromImages")));
    file->addAction(action(QStringLiteral("open")));
    m_recentMenu = file->addMenu(tr("Open &Recent"));
    file->addSeparator();
    file->addAction(action(QStringLiteral("save")));
    file->addAction(action(QStringLiteral("saveAs")));
    file->addAction(action(QStringLiteral("saveCopy")));
    file->addAction(action(QStringLiteral("reload")));
    file->addAction(action(QStringLiteral("revealFile")));

    QMenu *exportMenu = file->addMenu(tr("&Export As"));
    exportMenu->setIcon(icon(QStringLiteral("export")));
    exportMenu->addAction(action(QStringLiteral("exportImages")));
    exportMenu->addAction(action(QStringLiteral("exportText")));
    exportMenu->addAction(action(QStringLiteral("exportHtml")));
    exportMenu->addAction(action(QStringLiteral("exportComments")));
    file->addSeparator();
    file->addAction(action(QStringLiteral("print")));
    file->addSeparator();
    file->addAction(action(QStringLiteral("close")));
    file->addAction(action(QStringLiteral("quit")));

    QMenu *edit = m_appMenu->addMenu(tr("&Edit"));
    edit->setIcon(icon(QStringLiteral("edit-text")));
    edit->addAction(action(QStringLiteral("undo")));
    edit->addAction(action(QStringLiteral("redo")));
    edit->addSeparator();
    edit->addAction(action(QStringLiteral("copy")));
    edit->addAction(action(QStringLiteral("selectAll")));
    edit->addAction(action(QStringLiteral("find")));
    edit->addAction(action(QStringLiteral("findNext")));
    edit->addAction(action(QStringLiteral("findPrevious")));
    edit->addSeparator();
    edit->addAction(action(QStringLiteral("editText")));
    edit->addAction(action(QStringLiteral("editImage")));
    edit->addAction(action(QStringLiteral("addText")));
    edit->addAction(action(QStringLiteral("addImage")));
    edit->addAction(action(QStringLiteral("addLink")));
    edit->addAction(action(QStringLiteral("manageLinks")));
    edit->addAction(action(QStringLiteral("replaceAll")));
    edit->addSeparator();
    edit->addAction(action(QStringLiteral("settings")));

    QMenu *view = m_appMenu->addMenu(tr("&View"));
    view->setIcon(icon(QStringLiteral("eye")));
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
    view->addAction(action(QStringLiteral("goToPage")));
    view->addAction(action(QStringLiteral("presentation")));
    view->addAction(action(QStringLiteral("copyPageImage")));
    view->addAction(action(QStringLiteral("fullScreen")));

    QMenu *page = m_appMenu->addMenu(tr("&Page"));
    page->setIcon(icon(QStringLiteral("pages")));
    page->addAction(action(QStringLiteral("insertPages")));
    page->addAction(action(QStringLiteral("insertFromFile")));
    page->addAction(action(QStringLiteral("duplicatePage")));
    page->addAction(action(QStringLiteral("deletePages")));
    page->addAction(action(QStringLiteral("extractPages")));
    page->addSeparator();
    page->addAction(action(QStringLiteral("rotatePagesLeft")));
    page->addAction(action(QStringLiteral("rotatePagesRight")));
    page->addAction(action(QStringLiteral("rotateAllPages")));
    page->addAction(action(QStringLiteral("reversePages")));
    page->addAction(action(QStringLiteral("deleteOddEven")));
    page->addAction(action(QStringLiteral("grayscale")));
    page->addSeparator();
    page->addAction(action(QStringLiteral("merge")));
    page->addAction(action(QStringLiteral("split")));

    QMenu *tools = m_appMenu->addMenu(tr("&Tools"));
    tools->setIcon(icon(QStringLiteral("settings")));
    tools->addAction(action(QStringLiteral("ocr")));
    tools->addAction(action(QStringLiteral("compress")));
    tools->addAction(action(QStringLiteral("watermark")));
    tools->addSeparator();
    tools->addAction(action(QStringLiteral("encrypt")));
    tools->addAction(action(QStringLiteral("toolRedact")));
    tools->addAction(action(QStringLiteral("applyRedactions")));
    tools->addAction(action(QStringLiteral("sign")));
    tools->addSeparator();
    tools->addAction(action(QStringLiteral("forms")));
    tools->addAction(action(QStringLiteral("flatten")));

    m_appMenu->addSeparator();

    QMenu *help = m_appMenu->addMenu(tr("&Help"));
    help->setIcon(icon(QStringLiteral("info")));
    connect(help->addAction(tr("About SweetPDF")), &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("About SweetPDF"),
                           tr("<h3>SweetPDF %1</h3>"
                              "<p>An all-in-one, open source PDF editor.</p>"
                              "<p>Built with C++, Qt 6 and MuPDF. Licensed under the "
                              "AGPL-3.0.</p>")
                               .arg(QApplication::applicationVersion()));
    });
    connect(help->addAction(tr("About Qt")), &QAction::triggered, qApp, &QApplication::aboutQt);

    auto *menuButton = new QToolButton(m_ribbon);
    menuButton->setObjectName(QStringLiteral("AppMenu"));
    menuButton->setIcon(icon(QStringLiteral("menu")));
    menuButton->setText(tr("Menu"));
    menuButton->setToolTip(tr("Application menu"));
    menuButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    menuButton->setIconSize(QSize(16, 16));
    menuButton->setPopupMode(QToolButton::InstantPopup);
    menuButton->setMenu(m_appMenu);
    m_ribbon->addTabStripLeadingWidget(menuButton);

    auto *separator = new QFrame(m_ribbon);
    separator->setObjectName(QStringLiteral("Divider"));
    separator->setFixedWidth(1);
    separator->setFrameShape(QFrame::NoFrame);
    m_ribbon->addTabStripLeadingWidget(separator);

    connect(&Theme::instance(), &Theme::changed, menuButton,
            [menuButton] { menuButton->setIcon(icon(QStringLiteral("menu"))); });
}

// --- Ribbon ----------------------------------------------------------------

void MainWindow::createRibbon()
{
    m_ribbon = new RibbonBar(this);

    RibbonPage *home = m_ribbon->addPage(tr("Home"));
    // Open / Save / Print only: Save As, Save a Copy and Images sit in the Menu
    // instead. Stacking them beside the large buttons forced the whole ribbon to
    // grow taller than the commands that actually need to be one click away.
    RibbonGroup *fileGroup = home->addGroup(tr("File"));
    fileGroup->addAction(action(QStringLiteral("newDocument")));
    fileGroup->addAction(action(QStringLiteral("open")));
    fileGroup->addAction(action(QStringLiteral("save")));
    fileGroup->addAction(action(QStringLiteral("print")));

    RibbonGroup *historyGroup = home->addGroup(tr("History"));
    historyGroup->addAction(action(QStringLiteral("undo")));
    historyGroup->addAction(action(QStringLiteral("redo")));

    RibbonGroup *toolGroup = home->addGroup(tr("Tools"));
    toolGroup->addAction(action(QStringLiteral("toolSelect")));
    toolGroup->addAction(action(QStringLiteral("toolHand")));
    toolGroup->addAction(action(QStringLiteral("find")));

    RibbonGroup *markupGroup = home->addGroup(tr("Markup"));
    markupGroup->addAction(action(QStringLiteral("toolHighlight")));
    markupGroup->addAction(action(QStringLiteral("toolUnderline")));
    markupGroup->addAction(action(QStringLiteral("toolStrikeout")));
    markupGroup->addAction(action(QStringLiteral("toolSquiggly")));
    markupGroup->addAction(action(QStringLiteral("toolDraw")));
    markupGroup->addAction(action(QStringLiteral("toolNote")));
    markupGroup->addAction(action(QStringLiteral("toolStamp")));

    RibbonGroup *organiseGroup = home->addGroup(tr("Organise"));
    organiseGroup->addAction(action(QStringLiteral("ocr")));
    organiseGroup->addAction(action(QStringLiteral("merge")));
    organiseGroup->addAction(action(QStringLiteral("split")));
    organiseGroup->addAction(action(QStringLiteral("compress")));

    RibbonPage *editPage = m_ribbon->addPage(tr("Edit"));
    RibbonGroup *editContent = editPage->addGroup(tr("Content"));
    editContent->addAction(action(QStringLiteral("addText")));
    editContent->addAction(action(QStringLiteral("toolEditText")));
    editContent->addCompactAction(action(QStringLiteral("editImage")));
    editContent->addCompactAction(action(QStringLiteral("addImage")));
    RibbonGroup *editShapes = editPage->addGroup(tr("Shapes"));
    editShapes->addAction(action(QStringLiteral("addShape")));
    editShapes->addAction(action(QStringLiteral("addEllipse")));
    editShapes->addAction(action(QStringLiteral("addLine")));
    editShapes->addAction(action(QStringLiteral("addArrow")));
    RibbonGroup *editLinks = editPage->addGroup(tr("Links"));
    editLinks->addAction(action(QStringLiteral("addLink")));
    editLinks->addAction(action(QStringLiteral("manageLinks")));
    RibbonGroup *editReplace = editPage->addGroup(tr("Replace"));
    editReplace->addAction(action(QStringLiteral("replaceAll")));
    RibbonGroup *editClipboard = editPage->addGroup(tr("Clipboard"));
    editClipboard->addAction(action(QStringLiteral("copy")));
    editClipboard->addAction(action(QStringLiteral("selectAll")));
    RibbonGroup *editHistory = editPage->addGroup(tr("History"));
    editHistory->addAction(action(QStringLiteral("undo")));
    editHistory->addAction(action(QStringLiteral("redo")));

    RibbonPage *commentPage = m_ribbon->addPage(tr("Comment"));
    RibbonGroup *markup = commentPage->addGroup(tr("Markup"));
    markup->addAction(action(QStringLiteral("toolHighlight")));
    markup->addAction(action(QStringLiteral("toolUnderline")));
    markup->addAction(action(QStringLiteral("toolStrikeout")));
    markup->addAction(action(QStringLiteral("toolSquiggly")));
    RibbonGroup *drawing = commentPage->addGroup(tr("Drawing"));
    drawing->addAction(action(QStringLiteral("toolDraw")));
    drawing->addAction(action(QStringLiteral("toolNote")));
    drawing->addAction(action(QStringLiteral("toolStamp")));
    drawing->addAction(action(QStringLiteral("addText")));
    RibbonGroup *commentManage = commentPage->addGroup(tr("Manage"));
    commentManage->addAction(action(QStringLiteral("deleteAnnotation")));
    commentManage->addAction(action(QStringLiteral("duplicateSelection")));
    commentManage->addAction(action(QStringLiteral("copySelectionPages")));
    commentManage->addAction(action(QStringLiteral("removeComments")));

    RibbonPage *pagePage = m_ribbon->addPage(tr("Page"));
    RibbonGroup *organise = pagePage->addGroup(tr("Organise"));
    organise->addAction(action(QStringLiteral("insertPages")));
    organise->addAction(action(QStringLiteral("deletePages")));
    organise->addAction(action(QStringLiteral("extractPages")));
    organise->addCompactAction(action(QStringLiteral("insertFromFile")));
    organise->addCompactAction(action(QStringLiteral("duplicatePage")));
    organise->addCompactAction(action(QStringLiteral("cropPage")));
    organise->addCompactAction(action(QStringLiteral("headerFooter")));
    organise->addCompactAction(action(QStringLiteral("reversePages")));
    RibbonGroup *transform = pagePage->addGroup(tr("Rotate Pages"));
    transform->addAction(action(QStringLiteral("rotatePagesLeft")));
    transform->addAction(action(QStringLiteral("rotatePagesRight")));
    transform->addCompactAction(action(QStringLiteral("rotateAllPages")));
    RibbonGroup *viewRotate = pagePage->addGroup(tr("Rotate View"));
    viewRotate->addCompactAction(action(QStringLiteral("rotateLeft")));
    viewRotate->addCompactAction(action(QStringLiteral("rotateRight")));
    RibbonGroup *documents = pagePage->addGroup(tr("Documents"));
    documents->addAction(action(QStringLiteral("merge")));
    documents->addAction(action(QStringLiteral("split")));
    documents->addAction(action(QStringLiteral("grayscale")));
    documents->addAction(action(QStringLiteral("documentStats")));

    RibbonPage *viewPage = m_ribbon->addPage(tr("View"));
    RibbonGroup *zoomGroup = viewPage->addGroup(tr("Zoom"));
    zoomGroup->addAction(action(QStringLiteral("zoomOut")));
    zoomGroup->addAction(action(QStringLiteral("zoomIn")));
    zoomGroup->addAction(action(QStringLiteral("actualSize")));
    zoomGroup->addAction(action(QStringLiteral("fitPage")));
    zoomGroup->addAction(action(QStringLiteral("fitWidth")));
    zoomGroup->addAction(action(QStringLiteral("goToPage")));
    zoomGroup->addAction(action(QStringLiteral("presentation")));
    zoomGroup->addAction(action(QStringLiteral("copyPageImage")));
    RibbonGroup *layoutGroup = viewPage->addGroup(tr("Layout"));
    layoutGroup->addAction(action(QStringLiteral("layoutSingle")));
    layoutGroup->addAction(action(QStringLiteral("layoutContinuous")));
    layoutGroup->addAction(action(QStringLiteral("layoutTwoPage")));
    RibbonGroup *panelGroup = viewPage->addGroup(tr("Panels"));
    panelGroup->addAction(action(QStringLiteral("toggleSidebar")));
    panelGroup->addAction(action(QStringLiteral("toggleProperties")));
    panelGroup->addAction(action(QStringLiteral("compare")));
    panelGroup->addAction(action(QStringLiteral("nightMode")));
    panelGroup->addAction(action(QStringLiteral("toggleTheme")));

    RibbonPage *protectPage = m_ribbon->addPage(tr("Protect"));
    RibbonGroup *security = protectPage->addGroup(tr("Security"));
    security->addAction(action(QStringLiteral("encrypt")));
    security->addAction(action(QStringLiteral("watermark")));
    RibbonGroup *redaction = protectPage->addGroup(tr("Redaction"));
    redaction->addAction(action(QStringLiteral("toolRedact")));
    redaction->addAction(action(QStringLiteral("applyRedactions")));
    RibbonGroup *signing = protectPage->addGroup(tr("Signing"));
    signing->addAction(action(QStringLiteral("sign")));

    RibbonPage *toolsPage = m_ribbon->addPage(tr("Tools"));
    RibbonGroup *recognise = toolsPage->addGroup(tr("Recognise"));
    recognise->addAction(action(QStringLiteral("ocr")));
    RibbonGroup *optimise = toolsPage->addGroup(tr("Optimise"));
    optimise->addAction(action(QStringLiteral("compress")));
    RibbonGroup *exportGroup = toolsPage->addGroup(tr("Export"));
    exportGroup->addAction(action(QStringLiteral("exportImages")));
    exportGroup->addAction(action(QStringLiteral("exportText")));
    exportGroup->addAction(action(QStringLiteral("exportHtml")));
    RibbonGroup *formsGroup = toolsPage->addGroup(tr("Forms"));
    formsGroup->addAction(action(QStringLiteral("forms")));
    formsGroup->addAction(action(QStringLiteral("addFormField")));
    formsGroup->addAction(action(QStringLiteral("flatten")));
    RibbonGroup *settingsGroup = toolsPage->addGroup(tr("Application"));
    settingsGroup->addAction(action(QStringLiteral("settings")));
    settingsGroup->addAction(action(QStringLiteral("clearDescription")));

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
    // Panes can be dragged shut on a small window; the centre keeps a floor so
    // the page never vanishes under the chrome.
    m_splitter->setChildrenCollapsible(true);
    m_splitter->setHandleWidth(1);
    column->addWidget(m_splitter, 1);

    m_sidebar = new Sidebar(m_splitter);
    m_splitter->addWidget(m_sidebar);

    m_center = new QStackedWidget(m_splitter);
    m_center->setMinimumWidth(360);
    m_splitter->addWidget(m_center);

    m_tabs = new QTabWidget(m_center);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_center->addWidget(m_tabs);

    m_welcome = new WelcomeView(m_center);
    m_center->addWidget(m_welcome);
    connect(m_welcome, &WelcomeView::openRequested, this, &MainWindow::openFileDialog);
    connect(m_welcome, &WelcomeView::fileRequested, this,
            [this](const QString &path) { openFile(path); });
    connect(m_welcome, &WelcomeView::clearRecentRequested, this, [this] {
        QSettings settings;
        settings.remove(QLatin1String(recentFilesKey));
        rebuildRecentMenu();
    });
    connect(m_welcome, &WelcomeView::forgetRequested, this, &MainWindow::forgetRecentFile);
    connect(m_welcome, &WelcomeView::toolRequested, this, &MainWindow::runHomeTask);

    // The inspector stacks the current tool's settings above the document's
    // properties: picking a tool and adjusting it then happen in the same place,
    // and the ribbon does not need a style group that is meaningless for most
    // tools.
    m_inspector = new QWidget(m_splitter);
    m_inspector->setObjectName(QStringLiteral("Inspector"));
    auto *inspectorColumn = new QVBoxLayout(m_inspector);
    inspectorColumn->setContentsMargins(0, 0, 0, 0);
    inspectorColumn->setSpacing(0);

    m_toolOptions = new ToolOptionsPanel(m_inspector);
    m_toolOptions->setVisible(false);
    inspectorColumn->addWidget(m_toolOptions);
    connect(m_toolOptions, &ToolOptionsPanel::commandRequested, this,
            [this](const QString &actionId) {
                if (QAction *command = action(actionId); command && command->isEnabled())
                    command->trigger();
            });
    connect(m_toolOptions, &ToolOptionsPanel::optionsChanged, this,
            &MainWindow::updateInspectorVisibility);

    m_properties = new PropertiesPanel(m_inspector);
    inspectorColumn->addWidget(m_properties, 1);
    connect(m_properties, &PropertiesPanel::revealRequested, this, [this] {
        QAction *toggle = action(QStringLiteral("toggleProperties"));
        if (toggle && !toggle->isChecked())
            toggle->setChecked(true);
    });
    connect(m_properties, &PropertiesPanel::commandRequested, this,
            [this](const QString &actionId) {
                if (QAction *command = action(actionId); command && command->isEnabled())
                    command->trigger();
            });

    m_inspector->setMinimumWidth(272);
    m_splitter->addWidget(m_inspector);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_splitter->setSizes({ 248, 760, 300 });

    connect(m_tabs, &QTabWidget::currentChanged, this,
            [this](int) { updateForCurrentDocument(); });
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_sidebar, &Sidebar::annotationActivated, this, [this](int pageIndex, int objectNumber) {
        if (PageView *view = currentView())
            view->revealAnnotation(pageIndex, objectNumber);
    });
    connect(m_sidebar, &Sidebar::extractRequested, this, &MainWindow::extractPages);
    connect(m_sidebar, &Sidebar::insertFileRequested, this, &MainWindow::insertPagesFromFile);
    connect(m_sidebar, &Sidebar::statusMessage, this, &MainWindow::report);
    connect(m_sidebar, &Sidebar::saveAttachmentRequested, this, &MainWindow::saveEmbeddedFile);
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
    bar->setContentsMargins(6, 0, 6, 0);

    const auto addButton = [this, bar](const QString &id, bool permanent) {
        auto *button = new QToolButton(bar);
        button->setDefaultAction(action(id));
        button->setIconSize(QSize(16, 16));
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        if (permanent)
            bar->addPermanentWidget(button);
        else
            bar->addWidget(button);
        return button;
    };

    const auto addDivider = [bar](bool permanent) {
        auto *line = new QFrame(bar);
        line->setObjectName(QStringLiteral("Divider"));
        line->setFrameShape(QFrame::NoFrame);
        line->setFixedSize(1, 16);
        if (permanent)
            bar->addPermanentWidget(line);
        else
            bar->addWidget(line);
    };

    addButton(QStringLiteral("previousPage"), false);

    // Typing a page number is the fastest way through a long document, so the
    // page indicator is an input rather than a label.
    m_pageField = new QLineEdit(bar);
    m_pageField->setFixedWidth(46);
    m_pageField->setAlignment(Qt::AlignCenter);
    m_pageField->setPlaceholderText(QStringLiteral("–"));
    m_pageField->setToolTip(tr("Go to page"));
    connect(m_pageField, &QLineEdit::returnPressed, this, [this] {
        core::DocumentSession *session = currentSession();
        if (!session)
            return;
        bool ok = false;
        const int page = m_pageField->text().toInt(&ok);
        if (ok)
            session->setCurrentPage(page - 1);
        updateForCurrentDocument();
    });
    bar->addWidget(m_pageField);

    m_pageCountLabel = new QLabel(tr("of –"), bar);
    bar->addWidget(m_pageCountLabel);
    addButton(QStringLiteral("nextPage"), false);

    addButton(QStringLiteral("fitWidth"), true);
    addButton(QStringLiteral("fitPage"), true);
    addDivider(true);

    addButton(QStringLiteral("zoomOut"), true);

    m_zoomSlider = new QSlider(Qt::Horizontal, bar);
    m_zoomSlider->setRange(0, zoomSliderResolution);
    m_zoomSlider->setFixedWidth(132);
    m_zoomSlider->setValue(zoomToSlider(1.0));
    m_zoomSlider->setToolTip(tr("Zoom"));
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int value) {
        if (PageView *view = currentView())
            view->zoomTo(sliderToZoom(value));
    });
    bar->addPermanentWidget(m_zoomSlider);

    addButton(QStringLiteral("zoomIn"), true);

    m_zoomButton = new QToolButton(bar);
    m_zoomButton->setObjectName(QStringLiteral("StatusValue"));
    m_zoomButton->setText(QStringLiteral("100%"));
    m_zoomButton->setToolTip(tr("Zoom level"));
    m_zoomButton->setPopupMode(QToolButton::InstantPopup);
    m_zoomButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_zoomButton->setMinimumWidth(54);

    auto *zoomMenu = new QMenu(m_zoomButton);
    for (const int percent : { 50, 75, 100, 125, 150, 200, 400 }) {
        connect(zoomMenu->addAction(QStringLiteral("%1%").arg(percent)), &QAction::triggered,
                this, [this, percent] {
                    if (PageView *view = currentView())
                        view->zoomTo(percent / 100.0);
                });
    }
    zoomMenu->addSeparator();
    zoomMenu->addAction(action(QStringLiteral("fitPage")));
    zoomMenu->addAction(action(QStringLiteral("fitWidth")));
    zoomMenu->addAction(action(QStringLiteral("actualSize")));
    m_zoomButton->setMenu(zoomMenu);
    bar->addPermanentWidget(m_zoomButton);

    addDivider(true);
    addButton(QStringLiteral("toggleSidebar"), true);
    addButton(QStringLiteral("toggleProperties"), true);
    addButton(QStringLiteral("toggleTheme"), true);
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

    // The document's author name goes on every annotation it creates.
    const QSettings settings;
    session->setAuthor(settings.value(PreferencesDialog::authorKey(),
                                     qEnvironmentVariable("USER", tr("SweetPDF user")))
                           .toString());

    const QString startZoom =
        settings.value(PreferencesDialog::defaultZoomKey(), QStringLiteral("fitWidth")).toString();
    if (startZoom == QLatin1String("fitPage"))
        session->setZoomMode(core::ZoomMode::FitPage);
    else if (startZoom == QLatin1String("actualSize"))
        session->setZoom(1.0);
    else
        session->setZoomMode(core::ZoomMode::FitWidth);

    finishOpening(session, path, true);
    return true;
}

void MainWindow::finishOpening(core::DocumentSession *session, const QString &path, bool remember)
{
    auto *page = new QWidget(m_tabs);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *findBar = new FindBar(session, page);
    layout->addWidget(findBar);

    auto *view = new PageView(session, page);
    layout->addWidget(view, 1);
    new ViewerToolbar(view);

    session->setParent(view);

    connect(findBar, &FindBar::matchesChanged, view, &PageView::showMatches);
    connect(findBar, &FindBar::currentMatchChanged, view, &PageView::focusMatch);
    connect(findBar, &FindBar::dismissed, view, [view] {
        view->clearMatches();
        view->setFocus();
    });
    connect(view, &PageView::statusMessage, this, &MainWindow::report);
    connect(view, &PageView::textReplaceRequested, this,
            [this, view](const QString &replacement, int pageIndex, const QList<QRectF> &boxes) {
                if (view == currentView())
                    replaceSelectedTextWith(replacement, pageIndex, boxes);
            });

    const int index = m_tabs->addTab(page, session->displayName());
    m_tabs->setTabToolTip(index, path.isEmpty() ? session->displayName() : path);

    // Qt's built-in close button ignores the stylesheet, so use a themed one.
    auto *closeButton = new QToolButton(m_tabs);
    closeButton->setIcon(icon(QStringLiteral("close")));
    closeButton->setIconSize(QSize(12, 12));
    closeButton->setAutoRaise(true);
    closeButton->setFocusPolicy(Qt::NoFocus);
    closeButton->setToolTip(tr("Close document"));
    connect(closeButton, &QToolButton::clicked, this,
            [this, page] { closeTab(m_tabs->indexOf(page)); });
    connect(&Theme::instance(), &Theme::changed, closeButton,
            [closeButton] { closeButton->setIcon(icon(QStringLiteral("close"))); });
    m_tabs->tabBar()->setTabButton(index, QTabBar::RightSide, closeButton);

    m_tabs->setCurrentIndex(index);
    showWelcomeIfEmpty();

    connect(session, &core::DocumentSession::userNotice, this, &MainWindow::report);
    connect(session, &core::DocumentSession::modifiedChanged, this, [this, session](bool dirty) {
        for (int tab = 0; tab < m_tabs->count(); ++tab) {
            if (viewAt(tab) && viewAt(tab)->session() == session) {
                m_tabs->setTabText(tab, dirty ? session->displayName() + QLatin1String(" *")
                                              : session->displayName());
            }
        }
        updateWindowTitleFromSession();
    });

    if (remember && !path.isEmpty())
        addRecentFile(path);
    updateForCurrentDocument();
}

void MainWindow::openFileDialog()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Open PDF"), QString(),
        tr("Documents (*.pdf *.xps *.epub *.cbz *.svg *.md *.markdown *.html *.htm *.xhtml *.txt *.fb2);;"
           "PDF files (*.pdf);;Markdown (*.md *.markdown);;All files (*)"));
    for (const QString &path : paths)
        openFile(path);
}

void MainWindow::prepareNewSession(core::DocumentSession *session)
{
    const QSettings settings;
    session->setAuthor(settings.value(PreferencesDialog::authorKey(),
                                     qEnvironmentVariable("USER", tr("SweetPDF user")))
                           .toString());
    const QString startZoom =
        settings.value(PreferencesDialog::defaultZoomKey(), QStringLiteral("fitWidth")).toString();
    if (startZoom == QLatin1String("fitPage"))
        session->setZoomMode(core::ZoomMode::FitPage);
    else if (startZoom == QLatin1String("actualSize"))
        session->setZoom(1.0);
    else
        session->setZoomMode(core::ZoomMode::FitWidth);
}

void MainWindow::newDocument()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("New document"));
    auto *form = new QFormLayout;
    auto *paper = new QComboBox(&dialog);
    paper->addItem(tr("A4"), QSizeF(595.28, 841.89));
    paper->addItem(tr("Letter"), QSizeF(612, 792));
    paper->addItem(tr("Legal"), QSizeF(612, 1008));
    paper->addItem(tr("A3"), QSizeF(841.89, 1190.55));
    auto *countBox = new QSpinBox(&dialog);
    countBox->setRange(1, 200);
    countBox->setValue(1);
    form->addRow(tr("Size"), paper);
    form->addRow(tr("Pages"), countBox);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *column = new QVBoxLayout(&dialog);
    column->addLayout(form);
    column->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QSizeF size = paper->currentData().toSizeF();
    const int count = countBox->value();
    std::shared_ptr<core::PdfDocument> document;
    try {
        document = core::PdfDocument::createEmpty(size);
    } catch (const std::exception &failure) {
        reportError(tr("Could not create a document"), QString::fromUtf8(failure.what()));
        return;
    }

    auto *session = new core::DocumentSession(this);
    session->takeDocument(std::move(document), QString());
    prepareNewSession(session);
    if (count > 1) {
        QString error;
        if (!session->applyStructuralEdit(
                tr("Insert blank page"),
                [size, count](core::PdfDocument &created) {
                    for (int index = 1; index < count; ++index)
                        created.insertBlankPage(index, size);
                },
                &error)) {
            reportError(tr("Could not add every page"), error);
        }
    }
    finishOpening(session, QString(), false);
}

void MainWindow::newDocumentFromImages()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Images to turn into a PDF"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)"));
    if (paths.isEmpty())
        return;

    QList<QImage> images;
    QStringList skipped;
    for (const QString &path : paths) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (image.isNull())
            skipped.append(QFileInfo(path).fileName());
        else
            images.append(image);
    }
    if (images.isEmpty()) {
        reportError(tr("Could not read the images"), tr("None of the files could be opened."));
        return;
    }

    // Honour the file's own resolution. A 300 DPI scan stays letter-sized;
    // a picture with no resolution is treated as 96 DPI.
    const auto pageSize = [](const QImage &image) {
        const double dpiX = image.dotsPerMeterX() > 0 ? image.dotsPerMeterX() * 0.0254 : 96.0;
        const double dpiY = image.dotsPerMeterY() > 0 ? image.dotsPerMeterY() * 0.0254 : 96.0;
        return QSizeF(qMax(36.0, image.width() * 72.0 / dpiX),
                      qMax(36.0, image.height() * 72.0 / dpiY));
    };

    std::shared_ptr<core::PdfDocument> document;
    try {
        document = core::PdfDocument::createEmpty(pageSize(images.first()));
    } catch (const std::exception &failure) {
        reportError(tr("Could not create a document"), QString::fromUtf8(failure.what()));
        return;
    }

    auto *session = new core::DocumentSession(this);
    session->takeDocument(std::move(document), QString());
    prepareNewSession(session);
    finishOpening(session, QString(), false);

    session->undoStack()->beginMacro(tr("PDF from images"));
    QString error;
    const QSizeF first = pageSize(images.first());
    bool placed = session->addImage(0, images.first(), QRectF(QPointF(0, 0), first),
                                    tr("Place image"), &error);
    for (int index = 1; placed && index < images.size(); ++index) {
        const QSizeF size = pageSize(images.at(index));
        placed = session->applyStructuralEdit(
            tr("Insert blank page"),
            [index, size](core::PdfDocument &created) { created.insertBlankPage(index, size); },
            &error);
        if (placed) {
            placed = session->addImage(index, images.at(index), QRectF(QPointF(0, 0), size),
                                       tr("Place image"), &error);
        }
    }
    session->undoStack()->endMacro();
    if (!placed) {
        reportError(tr("Could not build the PDF"), error);
        return;
    }
    if (skipped.isEmpty())
        report(tr("Created a PDF from %n image(s).", nullptr, images.size()));
    else
        report(tr("Created a PDF from %1 image(s). Skipped: %2")
                   .arg(images.size())
                   .arg(skipped.join(QStringLiteral(", "))));
}

void MainWindow::deleteOddOrEvenPages()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument(true))
        return;
    if (session->pageCount() < 2) {
        report(tr("There is only one page."));
        return;
    }

    bool accepted = false;
    const QString which = QInputDialog::getItem(this, tr("Delete pages"), tr("Remove:"),
                                                { tr("Odd pages"), tr("Even pages") }, 0, false,
                                                &accepted);
    if (!accepted)
        return;

    const bool odd = which == tr("Odd pages");
    QList<int> pages;
    for (int index = 0; index < session->pageCount(); ++index) {
        const bool isOdd = (index % 2) == 0;
        if (isOdd == odd)
            pages.append(index);
    }
    if (pages.size() >= session->pageCount()) {
        report(tr("A document has to keep at least one page."));
        return;
    }

    QString error;
    if (session->applyStructuralEdit(
            tr("Delete pages"),
            [pages](core::PdfDocument &document) { document.deletePages(pages); }, &error))
        report(tr("Removed %n page(s).", nullptr, pages.size()));
    else
        reportError(tr("Could not delete the pages"), error);
}

void MainWindow::convertToGrayscale()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument(true))
        return;
    QString error;
    if (session->convertToGrayscale(&error))
        report(tr("Converted the document to grayscale."));
    else
        reportError(tr("Could not convert to grayscale"), error);
}

void MainWindow::removeAllComments()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument())
        return;
    const QList<core::Annotation> comments = core::annotations::all(*session->document());
    QList<core::Annotation> removable;
    for (const core::Annotation &annotation : comments) {
        if (annotation.editable)
            removable.append(annotation);
    }
    if (removable.isEmpty()) {
        report(tr("There are no comments to remove."));
        return;
    }
    const auto choice = QMessageBox::question(
        this, tr("Remove comments"),
        tr("Delete %n comment(s), drawing(s) and stamp(s)?", nullptr, removable.size()),
        QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
    if (choice != QMessageBox::Yes)
        return;

    session->undoStack()->beginMacro(tr("Remove comments"));
    int removed = 0;
    for (const core::Annotation &annotation : removable) {
        if (session->removeAnnotation(annotation))
            ++removed;
    }
    session->undoStack()->endMacro();
    report(tr("Removed %n comment(s).", nullptr, removed));
}

void MainWindow::exportComments()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded() || !session->document()->isPdf())
        return;
    QList<core::Annotation> comments;
    for (const core::Annotation &annotation : core::annotations::all(*session->document())) {
        if (annotation.editable)
            comments.append(annotation);
    }
    if (comments.isEmpty()) {
        report(tr("This document has no comments."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this, tr("Export comments"),
                                                      suggestedPath(tr("-comments"), QStringLiteral("txt")),
                                                      tr("Text files (*.txt)"));
    if (path.isEmpty())
        return;

    QString text;
    for (const core::Annotation &annotation : comments) {
        text += tr("Page %1 — %2")
                    .arg(annotation.pageIndex + 1)
                    .arg(core::annotations::displayName(annotation.type));
        if (!annotation.author.isEmpty())
            text += tr(" — %1").arg(annotation.author);
        text += QLatin1Char('\n');
        if (!annotation.contents.isEmpty())
            text += annotation.contents + QLatin1Char('\n');
        text += QLatin1Char('\n');
    }

    QSaveFile file(path);
    const QByteArray bytes = text.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        reportError(tr("Could not write the file"), file.errorString());
        return;
    }
    report(tr("Exported %n comment(s).", nullptr, comments.size()));
}

void MainWindow::showDocumentStats()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return;

    int words = 0;
    for (int index = 0; index < session->pageCount(); ++index) {
        auto page = session->document()->page(index);
        if (!page || !page->textLayer())
            continue;
        words += page->textLayer()->plainText().split(QRegularExpression(QStringLiteral("\\s+")),
                                                      Qt::SkipEmptyParts)
                     .size();
    }
    int comments = 0;
    if (session->document()->isPdf()) {
        for (const core::Annotation &annotation : core::annotations::all(*session->document())) {
            if (annotation.editable)
                ++comments;
        }
    }
    const core::DocumentInfo info = session->document()->info();
    const QString fileSize = session->filePath().isEmpty()
        ? tr("Not saved yet")
        : QLocale().formattedDataSize(info.fileSize);
    QMessageBox::information(
        this, tr("Document info"),
        tr("Pages: %1\nWords: %2\nComments: %3\nFile size: %4\nFormat: %5")
            .arg(session->pageCount())
            .arg(words)
            .arg(comments)
            .arg(fileSize)
            .arg(info.format.isEmpty() ? tr("PDF") : info.format));
}

void MainWindow::reloadDocument()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return;
    if (session->filePath().isEmpty()) {
        report(tr("This document has not been saved yet."));
        return;
    }
    if (session->isModified()) {
        const auto choice = QMessageBox::question(
            this, tr("Reload"),
            tr("Reload %1 and discard unsaved changes?").arg(session->displayName()),
            QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
        if (choice != QMessageBox::Yes)
            return;
    }

    QString password;
    if (core::PdfDocument::needsPassword(session->filePath())) {
        bool accepted = false;
        password = QInputDialog::getText(this, tr("Password required"), tr("Password:"),
                                        QLineEdit::Password, QString(), &accepted);
        if (!accepted)
            return;
    }
    QString error;
    const QString path = session->filePath();
    if (!session->load(path, password, &error))
        reportError(tr("Could not reload"), error);
    else
        report(tr("Reloaded %1.").arg(session->displayName()));
}

void MainWindow::revealDocument()
{
    auto *session = currentSession();
    if (!session || session->filePath().isEmpty()) {
        report(tr("This document has not been saved yet."));
        return;
    }
    const QString folder = QFileInfo(session->filePath()).absolutePath();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folder)))
        reportError(tr("Could not open the folder"), folder);
}

void MainWindow::closeTab(int index)
{
    QWidget *page = m_tabs->widget(index);
    if (!page)
        return;

    m_tabs->removeTab(index);
    page->deleteLater();
    showWelcomeIfEmpty();
    updateForCurrentDocument();
}

void MainWindow::closeCurrentDocument()
{
    closeTab(m_tabs->currentIndex());
}

void MainWindow::showWelcomeIfEmpty()
{
    const bool empty = m_tabs->count() == 0;
    m_center->setCurrentWidget(empty ? m_welcome : static_cast<QWidget *>(m_tabs));

    // None of the chrome can do anything without a document, and an empty ribbon
    // over an empty sidebar is the least welcoming thing a window can show. The
    // home screen therefore gets the window to itself.
    if (m_ribbon)
        m_ribbon->setVisible(!empty);
    m_sidebar->setVisible(!empty);
    statusBar()->setVisible(!empty);

    if (empty) {
        m_inspector->setVisible(false);
    } else if (!m_layoutModeReady) {
        applyLayoutMode(true);
    } else {
        updateInspectorVisibility();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    applyLayoutMode();
}

MainWindow::LayoutMode MainWindow::layoutModeForWidth(int width) const
{
    if (width < 1100)
        return LayoutMode::Narrow;
    if (width < 1400)
        return LayoutMode::Compact;
    return LayoutMode::Comfortable;
}

void MainWindow::applyLayoutMode(bool force)
{
    if (!m_ribbon || !m_sidebar || !m_inspector || !m_zoomSlider)
        return;

    // The home screen owns the whole window; chrome density only matters once a
    // document is open.
    if (m_tabs->count() == 0)
        return;

    const LayoutMode mode = layoutModeForWidth(width());
    if (!force && m_layoutModeReady && mode == m_layoutMode)
        return;

    m_layoutMode = mode;
    m_layoutModeReady = true;

    const bool narrow = mode == LayoutMode::Narrow;
    const bool comfortable = mode == LayoutMode::Comfortable;

    m_ribbon->setCollapsed(narrow);
    m_zoomSlider->setVisible(!narrow);

    // Mode change resets the navigation panel to the mode's default. Properties
    // follow the Properties toggle (on by default). Tool options share the inspector.
    const bool showSidebarPanel = comfortable;

    {
        QAction *toggle = action(QStringLiteral("toggleSidebar"));
        const QSignalBlocker blocker(toggle);
        toggle->setChecked(showSidebarPanel);
        m_sidebar->setPanelVisible(showSidebarPanel);
    }
    updateInspectorVisibility();

    clampSplitterSizes();
}

void MainWindow::clampSplitterSizes()
{
    if (!m_splitter)
        return;

    QList<int> sizes = m_splitter->sizes();
    if (sizes.size() < 3)
        return;

    const int total = sizes[0] + sizes[1] + sizes[2];
    constexpr int minCenter = 360;
    if (sizes[1] >= minCenter || total <= minCenter)
        return;

    // Steal from the side that currently takes more space so the page area
    // keeps a usable width after a restored splitter state.
    int deficit = minCenter - sizes[1];
    sizes[1] = minCenter;
    if (sizes[2] >= sizes[0] && sizes[2] > deficit) {
        sizes[2] -= deficit;
    } else if (sizes[0] > deficit) {
        sizes[0] -= deficit;
    } else {
        const int fromLeft = qMin(sizes[0], deficit);
        sizes[0] -= fromLeft;
        deficit -= fromLeft;
        sizes[2] = qMax(0, sizes[2] - deficit);
    }
    m_splitter->setSizes(sizes);
}

void MainWindow::runHomeTask(const QString &taskId)
{
    // A couple of tasks are window-level and work with nothing open.
    if (taskId == QLatin1String("settings") || taskId == QLatin1String("toggleTheme")) {
        if (QAction *command = action(taskId))
            command->trigger();
        return;
    }

    // The rest need a document, so ask for one and then hand over to the same
    // command the ribbon would have used. That way a tile is a complete flow
    // rather than a dead end.
    if (m_tabs->count() == 0) {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Choose a document"), QString(),
            tr("Documents (*.pdf *.xps *.epub *.cbz *.svg *.md *.markdown *.html *.htm *.xhtml *.txt *.fb2);;"
           "PDF files (*.pdf);;Markdown (*.md *.markdown);;All files (*)"));
        if (path.isEmpty() || !openFile(path))
            return;
    }

    // "organise" is a panel rather than a command.
    if (taskId == QLatin1String("organise")) {
        m_sidebar->setPanelVisible(true);
        m_sidebar->showPanel(Sidebar::Panel::Pages);
        report(tr("Drag thumbnails to reorder pages, or use the Page tab for more."));
        return;
    }

    if (QAction *command = action(taskId); command && command->isEnabled())
        command->trigger();
}

void MainWindow::forgetRecentFile(const QString &path)
{
    QStringList files = recentFiles();
    if (files.removeAll(path) == 0)
        return;

    QSettings settings;
    settings.setValue(QLatin1String(recentFilesKey), files);
    rebuildRecentMenu();
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
    connect(session, &core::DocumentSession::toolChanged, this, &MainWindow::syncToolActions,
            Qt::UniqueConnection);
    connect(session->undoStack(), &QUndoStack::canUndoChanged,
            action(QStringLiteral("undo")), &QAction::setEnabled, Qt::UniqueConnection);
    connect(session->undoStack(), &QUndoStack::canRedoChanged,
            action(QStringLiteral("redo")), &QAction::setEnabled, Qt::UniqueConnection);
    connect(action(QStringLiteral("undo")), &QAction::triggered, session->undoStack(),
            &QUndoStack::undo, Qt::UniqueConnection);
    connect(action(QStringLiteral("redo")), &QAction::triggered, session->undoStack(),
            &QUndoStack::redo, Qt::UniqueConnection);

    syncToolActions();
}

void MainWindow::updateForCurrentDocument()
{
    auto *session = currentSession();
    const bool hasDocument = session && session->isLoaded();

    for (QAction *entry : std::as_const(m_documentActions))
        entry->setEnabled(hasDocument);

    m_sidebar->setSession(session);
    m_properties->setSession(session);
    m_toolOptions->setSession(session);

    if (!hasDocument) {
        m_pageField->clear();
        m_pageField->setEnabled(false);
        m_pageCountLabel->setText(tr("of –"));
        m_zoomButton->setText(QStringLiteral("—"));
        action(QStringLiteral("undo"))->setEnabled(false);
        action(QStringLiteral("redo"))->setEnabled(false);
        setWindowTitle(tr("SweetPDF"));
        return;
    }

    connectSessionSignals();

    m_pageField->setEnabled(true);
    if (!m_pageField->hasFocus())
        m_pageField->setText(QString::number(session->currentPage() + 1));
    m_pageCountLabel->setText(tr("of %1").arg(session->pageCount()));
    m_zoomButton->setText(QStringLiteral("%1%").arg(qRound(session->zoom() * 100)));

    const QSignalBlocker blocker(m_zoomSlider);
    m_zoomSlider->setValue(zoomToSlider(session->zoom()));

    action(QStringLiteral("undo"))->setEnabled(session->undoStack()->canUndo());
    action(QStringLiteral("redo"))->setEnabled(session->undoStack()->canRedo());
    if (QAction *night = action(QStringLiteral("nightMode"))) {
        const QSignalBlocker nightBlock(night);
        night->setChecked(session->nightMode());
    }
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

// --- Command plumbing ------------------------------------------------------

void MainWindow::report(const QString &message)
{
    if (!message.isEmpty())
        statusBar()->showMessage(message, 6000);
}

void MainWindow::reportError(const QString &title, const QString &details)
{
    QMessageBox::warning(this, title, details);
}

bool MainWindow::requireEditableDocument(bool needsAssembly)
{
    auto *session = currentSession();
    if (!session || !session->isLoaded()) {
        report(tr("Open a document first."));
        return false;
    }
    if (!session->document()->isPdf()) {
        reportError(tr("Cannot edit this file"),
                    tr("%1 can be viewed but not edited. Save it as a PDF first.")
                        .arg(session->displayName()));
        return false;
    }

    // Permissions are the document author's wishes rather than a hard lock, so
    // say what is being blocked instead of silently doing nothing.
    const core::DocumentPermissions &permissions = session->document()->permissions();
    if (needsAssembly && !permissions.assemble && !permissions.modify) {
        reportError(tr("Not permitted"),
                    tr("This document does not allow its pages to be changed."));
        return false;
    }
    if (!needsAssembly && !permissions.annotate && !permissions.modify) {
        reportError(tr("Not permitted"), tr("This document does not allow annotations."));
        return false;
    }
    return true;
}

QString MainWindow::suggestedPath(const QString &suffix, const QString &extension) const
{
    auto *session = currentSession();
    if (!session)
        return {};

    const QFileInfo info(session->filePath());
    const QString directory = info.absolutePath().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : info.absolutePath();
    const QString base = info.completeBaseName().isEmpty() ? tr("Untitled")
                                                           : info.completeBaseName();
    return QDir(directory).filePath(base + suffix + QLatin1Char('.') + extension);
}

void MainWindow::runWithProgress(
    const QString &title,
    const std::function<bool(const std::function<bool(int, int)> &)> &work)
{
    QProgressDialog progress(title, tr("Cancel"), 0, 100, this);
    progress.setWindowTitle(tr("SweetPDF"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(400);
    progress.setAutoClose(true);

    // The work runs on this thread, so the reporter is what keeps the dialog
    // painting and the Cancel button clickable.
    const auto reporter = [&progress](int done, int total) {
        progress.setMaximum(qMax(1, total));
        progress.setValue(qBound(0, done, total));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents
                                       | QEventLoop::AllEvents);
        return !progress.wasCanceled();
    };

    const bool finished = work(reporter);
    progress.reset();
    if (!finished)
        report(tr("Cancelled."));
}

// --- File commands ---------------------------------------------------------

bool MainWindow::saveDocument()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return false;
    if (!session->document()->isPdf() || session->filePath().isEmpty())
        return saveDocumentAs(false);

    // Appending changes is the default because it is fast and keeps signatures,
    // but the settings let people ask for a full rewrite instead.
    const QSettings settings;
    const bool appendChanges =
        settings.value(PreferencesDialog::incrementalSaveKey(), true).toBool();

    QString error;
    bool saved = false;
    if (appendChanges) {
        saved = session->save(&error);
    } else {
        core::SaveOptions options;
        options.incremental = false;
        options.garbageCollect = true;
        saved = session->saveAs(session->filePath(), options, false, &error);
    }
    if (!saved) {
        reportError(tr("Could not save"), error);
        return false;
    }
    report(tr("Saved %1.").arg(session->displayName()));
    return true;
}

bool MainWindow::saveDocumentAs(bool asCopy)
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return false;
    if (!session->document()->isPdf()) {
        reportError(tr("Cannot save as PDF"),
                    tr("Saving other formats as PDF is not supported yet."));
        return false;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, asCopy ? tr("Save a copy") : tr("Save as"),
        suggestedPath(asCopy ? tr("-copy") : QString(), QStringLiteral("pdf")),
        tr("PDF documents (*.pdf)"));
    if (path.isEmpty())
        return false;

    core::SaveOptions options;
    options.garbageCollect = true;

    QString error;
    if (!session->saveAs(path, options, asCopy, &error)) {
        reportError(tr("Could not save"), error);
        return false;
    }

    if (!asCopy) {
        addRecentFile(path);
        const int tab = m_tabs->currentIndex();
        m_tabs->setTabText(tab, session->displayName());
        m_tabs->setTabToolTip(tab, path);
        updateWindowTitleFromSession();
    }
    report(asCopy ? tr("Copy saved to %1.").arg(QFileInfo(path).fileName())
                  : tr("Saved %1.").arg(QFileInfo(path).fileName()));
    return true;
}

void MainWindow::printDocument()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return;
    if (!session->document()->permissions().print) {
        reportError(tr("Not permitted"), tr("This document does not allow printing."));
        return;
    }

    bool layoutAccepted = false;
    const QString layout = QInputDialog::getItem(
        this, tr("Print layout"), tr("Pages on each sheet:"),
        { tr("1"), tr("2 side by side"), tr("4"), tr("Booklet") }, 0, false, &layoutAccepted);
    if (!layoutAccepted)
        return;
    const bool booklet = layout == tr("Booklet");
    const int perSheet = layout == tr("2 side by side") || booklet ? 2 : layout == tr("4") ? 4 : 1;

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(session->displayName());
    QPrintDialog dialog(&printer, this);
    dialog.setOption(QAbstractPrintDialog::PrintPageRange, true);
    dialog.setMinMax(1, session->pageCount());
    if (dialog.exec() != QDialog::Accepted)
        return;

    const int from = printer.fromPage() > 0 ? printer.fromPage() - 1 : 0;
    const int to = printer.toPage() > 0 ? printer.toPage() - 1 : session->pageCount() - 1;

    QList<int> order;
    if (!booklet) {
        for (int index = from; index <= to; ++index)
            order.append(index);
    } else {
        const int count = to - from + 1;
        int padded = count;
        while (padded % 4 != 0)
            ++padded;
        const auto map = [from, count](int logical) {
            return logical >= 0 && logical < count ? from + logical : -1;
        };
        for (int sheet = 0; sheet < padded / 4; ++sheet) {
            order.append(map(padded - 1 - 2 * sheet));
            order.append(map(2 * sheet));
            order.append(map(2 * sheet + 1));
            order.append(map(padded - 2 - 2 * sheet));
        }
    }

    runWithProgress(tr("Printing %1…").arg(session->displayName()),
                    [&](const std::function<bool(int, int)> &progress) {
                        QPainter painter;
                        if (!painter.begin(&printer)) {
                            reportError(tr("Could not print"),
                                        tr("The printer could not be opened."));
                            return true;
                        }

                        const int sheets = (order.size() + perSheet - 1) / perSheet;
                        for (int sheet = 0; sheet < sheets; ++sheet) {
                            if (!progress(sheet, sheets)) {
                                painter.end();
                                return false;
                            }
                            const QRectF target = printer.pageRect(QPrinter::DevicePixel);
                            const int columns = perSheet == 4 ? 2 : perSheet;
                            const int rows = perSheet == 4 ? 2 : 1;
                            const qreal cellW = target.width() / columns;
                            const qreal cellH = target.height() / rows;
                            for (int slot = 0; slot < perSheet; ++slot) {
                                const int source = order.value(sheet * perSheet + slot, -1);
                                if (source < 0)
                                    continue;
                                auto page = session->document()->page(source);
                                if (!page)
                                    continue;
                                const QSizeF pageSize = page->sizePoints();
                                const qreal scale = qMin(cellW / pageSize.width(), cellH / pageSize.height());
                                const QImage image = page->render(scale);
                                if (image.isNull())
                                    continue;
                                const int column = slot % columns;
                                const int row = slot / columns;
                                const QRectF cell(target.left() + column * cellW, target.top() + row * cellH,
                                                  cellW, cellH);
                                const QRectF where(cell.left() + (cell.width() - image.width()) / 2.0,
                                                   cell.top() + (cell.height() - image.height()) / 2.0,
                                                   image.width(), image.height());
                                painter.drawImage(where, image);
                            }
                            if (sheet + 1 < sheets)
                                printer.newPage();
                        }

                        painter.end();
                        report(tr("Sent %n sheet(s) to the printer.", nullptr, sheets));
                        return true;
                    });
}

void MainWindow::exportImages()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return;

    ExportImagesDialog dialog(session->pageCount(), session->currentPage(),
                              QFileInfo(session->filePath()).completeBaseName(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QList<int> pages = dialog.pages();
    if (pages.isEmpty()) {
        report(tr("No pages matched that range."));
        return;
    }

    runWithProgress(tr("Exporting %n page(s)…", nullptr, int(pages.size())),
                    [&](const std::function<bool(int, int)> &progress) {
                        try {
                            const QStringList written = core::exporter::pagesToImages(
                                *session->document(), pages, dialog.directory(),
                                dialog.baseName(), dialog.format(), dialog.dpi(), progress);
                            report(tr("Wrote %n image(s) to %1.", nullptr, int(written.size()))
                                       .arg(dialog.directory()));
                            return int(written.size()) == int(pages.size());
                        } catch (const std::exception &failure) {
                            reportError(tr("Could not export"),
                                        QString::fromUtf8(failure.what()));
                            return true;
                        }
                    });
}

void MainWindow::exportText()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return;
    if (!session->document()->permissions().copy) {
        reportError(tr("Not permitted"), tr("This document does not allow copying text."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this, tr("Export text"),
                                                      suggestedPath(QString(),
                                                                    QStringLiteral("txt")),
                                                      tr("Text files (*.txt)"));
    if (path.isEmpty())
        return;

    runWithProgress(tr("Extracting text…"), [&](const std::function<bool(int, int)> &progress) {
        try {
            core::exporter::textToFile(*session->document(), path, progress);
            report(tr("Text written to %1.").arg(QFileInfo(path).fileName()));
        } catch (const std::exception &failure) {
            reportError(tr("Could not export"), QString::fromUtf8(failure.what()));
        }
        return true;
    });
}

void MainWindow::exportHtml()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return;

    const QString path = QFileDialog::getSaveFileName(this, tr("Export HTML"),
                                                      suggestedPath(QString(),
                                                                    QStringLiteral("html")),
                                                      tr("HTML files (*.html)"));
    if (path.isEmpty())
        return;

    runWithProgress(tr("Writing HTML…"), [&](const std::function<bool(int, int)> &progress) {
        try {
            core::exporter::toHtml(*session->document(), path, progress);
            report(tr("HTML written to %1.").arg(QFileInfo(path).fileName()));
        } catch (const std::exception &failure) {
            reportError(tr("Could not export"), QString::fromUtf8(failure.what()));
        }
        return true;
    });
}

// --- Page commands ---------------------------------------------------------

void MainWindow::insertBlankPage()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument(true))
        return;

    const QSizeF current = session->document()->pageSizePoints(session->currentPage());
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Insert blank page"));
    auto *form = new QFormLayout;
    auto *paper = new QComboBox(&dialog);
    paper->addItem(tr("Same as this page"), current.isEmpty() ? QSizeF(612, 792) : current);
    paper->addItem(tr("A4"), QSizeF(595.28, 841.89));
    paper->addItem(tr("A3"), QSizeF(841.89, 1190.55));
    paper->addItem(tr("Letter"), QSizeF(612, 792));
    paper->addItem(tr("Legal"), QSizeF(612, 1008));
    paper->addItem(tr("Tabloid"), QSizeF(792, 1224));
    auto *where = new QComboBox(&dialog);
    where->addItem(tr("After the current page"), 1);
    where->addItem(tr("Before the current page"), 0);
    form->addRow(tr("Size"), paper);
    form->addRow(tr("Place"), where);
    auto *countBox = new QSpinBox(&dialog);
    countBox->setRange(1, 200);
    countBox->setValue(1);
    form->addRow(tr("How many"), countBox);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *column = new QVBoxLayout(&dialog);
    column->addLayout(form);
    column->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QSizeF size = paper->currentData().toSizeF();
    const int at = session->currentPage() + where->currentData().toInt();
    const int count = countBox->value();
    QString error;
    const bool inserted = session->applyStructuralEdit(
        tr("Insert blank page"),
        [at, size, count](core::PdfDocument &document) {
            for (int index = 0; index < count; ++index)
                document.insertBlankPage(at + index, size);
        },
        &error);

    if (inserted) {
        session->setCurrentPage(at);
        report(tr("Inserted %n blank page(s).", nullptr, count));
    } else {
        reportError(tr("Could not insert a page"), error);
    }
}

void MainWindow::reversePages()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument(true))
        return;
    if (session->pageCount() < 2) {
        report(tr("There is only one page."));
        return;
    }

    QList<int> order;
    for (int index = session->pageCount() - 1; index >= 0; --index)
        order.append(index);

    QString error;
    if (session->applyStructuralEdit(
            tr("Reverse pages"),
            [order](core::PdfDocument &document) { document.reorderPages(order); }, &error))
        report(tr("Page order reversed."));
    else
        reportError(tr("Could not reverse the pages"), error);
}

void MainWindow::rotateAllPages()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument(true))
        return;

    bool accepted = false;
    const QString direction = QInputDialog::getItem(
        this, tr("Rotate all pages"), tr("Direction:"), { tr("Right"), tr("Left") }, 0, false,
        &accepted);
    if (!accepted)
        return;
    const int degrees = direction == tr("Left") ? -90 : 90;
    const int count = session->pageCount();
    QString error;
    if (session->applyStructuralEdit(
            tr("Rotate all pages"),
            [count, degrees](core::PdfDocument &document) {
                for (int index = 0; index < count; ++index)
                    document.setPageRotation(index, document.pageRotation(index) + degrees);
            },
            &error))
        report(tr("Rotated every page."));
    else
        reportError(tr("Could not rotate the pages"), error);
}

void MainWindow::goToPage()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return;

    bool accepted = false;
    const int page = QInputDialog::getInt(this, tr("Go to page"), tr("Page number:"),
                                          session->currentPage() + 1, 1, session->pageCount(), 1,
                                          &accepted);
    if (accepted)
        session->setCurrentPage(page - 1);
}

void MainWindow::copyPageImage()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return;
    auto page = session->document()->page(session->currentPage());
    if (!page)
        return;
    const QImage image = page->render(2.0);
    if (image.isNull()) {
        reportError(tr("Could not copy the page"), tr("The page did not render."));
        return;
    }
    QApplication::clipboard()->setImage(image);
    report(tr("Page %1 copied as an image.").arg(session->currentPage() + 1));
}

void MainWindow::setPresentation(bool on)
{
    if (!currentSession() || !currentSession()->isLoaded())
        return;
    if (on == m_presenting)
        return;
    m_presenting = on;
    for (QAction *key : m_presentKeys)
        key->setEnabled(on);

    if (on) {
        if (m_ribbon)
            m_ribbon->hide();
        if (m_sidebar)
            m_sidebar->hide();
        if (m_inspector)
            m_inspector->hide();
        statusBar()->hide();
        currentSession()->setZoomMode(core::ZoomMode::FitPage);
        setWindowState(windowState() | Qt::WindowFullScreen);
        report(tr("Presentation. Arrow keys turn the page. Esc leaves."));
        return;
    }

    setWindowState(windowState() & ~Qt::WindowFullScreen);
    if (QAction *fullScreen = action(QStringLiteral("fullScreen"))) {
        const QSignalBlocker blocker(fullScreen);
        fullScreen->setChecked(false);
    }
    showWelcomeIfEmpty();
    applyLayoutMode(true);
}

void MainWindow::duplicateSelection()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument())
        return;
    if (session->selectedAnnotation() == 0) {
        report(tr("Select a comment, shape or text box first."));
        return;
    }

    for (const core::Annotation &annotation :
         session->annotationsOnPage(session->selectedAnnotationPage())) {
        if (annotation.objectNumber != session->selectedAnnotation() || !annotation.editable)
            continue;
        if (session->addAnnotation(copyOf(annotation, annotation.pageIndex, QPointF(16, 16)),
                                   tr("Duplicate selection")))
            report(tr("Duplicated the selection."));
        return;
    }
    report(tr("That selection cannot be duplicated."));
}

void MainWindow::copySelectionAcrossPages()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument())
        return;
    if (session->selectedAnnotation() == 0) {
        report(tr("Select a comment, shape or text box first."));
        return;
    }

    core::Annotation selected;
    bool found = false;
    for (const core::Annotation &annotation :
         session->annotationsOnPage(session->selectedAnnotationPage())) {
        if (annotation.objectNumber == session->selectedAnnotation() && annotation.editable) {
            selected = annotation;
            found = true;
            break;
        }
    }
    if (!found) {
        report(tr("That selection cannot be copied."));
        return;
    }

    session->undoStack()->beginMacro(tr("Copy to all pages"));
    int copied = 0;
    for (int page = 0; page < session->pageCount(); ++page) {
        if (page == selected.pageIndex)
            continue;
        if (session->addAnnotation(copyOf(selected, page, QPointF()), tr("Copy to all pages")))
            ++copied;
    }
    session->undoStack()->endMacro();
    report(tr("Copied onto %n other page(s).", nullptr, copied));
}

void MainWindow::clearDescription()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument())
        return;

    const auto choice = QMessageBox::question(
        this, tr("Clear description"),
        tr("Remove the title, author, subject and keywords stored in this PDF?"),
        QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
    if (choice != QMessageBox::Yes)
        return;

    core::DocumentInfo info = session->document()->info();
    info.title.clear();
    info.author.clear();
    info.subject.clear();
    info.keywords.clear();
    QString error;
    if (session->setDocumentInfo(info, &error))
        report(tr("Description cleared."));
    else
        reportError(tr("Could not clear the description"), error);
}

void MainWindow::insertPagesFromFile(int atPage)
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument(true))
        return;

    const QString path = QFileDialog::getOpenFileName(this, tr("Insert pages from"), QString(),
                                                      tr("PDF documents (*.pdf)"));
    if (path.isEmpty())
        return;

    QString error;
    // Grafted onto the end first, then moved into place, so one code path covers
    // "insert here" and "append".
    const int existing = session->pageCount();
    const bool inserted = session->applyStructuralEdit(
        tr("Insert pages"),
        [path, atPage, existing](core::PdfDocument &document) {
            document.appendPagesFrom(path);
            const int added = document.pageCount() - existing;
            for (int offset = 0; offset < added; ++offset)
                document.movePage(existing + offset, atPage + offset);
        },
        &error);

    if (inserted) {
        session->setCurrentPage(atPage);
        report(tr("Inserted pages from %1.").arg(QFileInfo(path).fileName()));
    } else {
        reportError(tr("Could not insert pages"), error);
    }
}

void MainWindow::deleteCurrentPages()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument(true))
        return;
    if (session->pageCount() <= 1) {
        report(tr("A document must keep at least one page."));
        return;
    }

    const QString text = QInputDialog::getText(
        this, tr("Delete pages"), tr("Pages to delete (for example 2, 5-7):"), QLineEdit::Normal,
        QString::number(session->currentPage() + 1));
    if (text.isEmpty())
        return;

    const QList<int> pages = parsePageRange(text, session->pageCount());
    if (pages.isEmpty()) {
        report(tr("No pages matched that range."));
        return;
    }
    if (pages.size() >= session->pageCount()) {
        report(tr("A document must keep at least one page."));
        return;
    }

    QString error;
    const bool deleted = session->applyStructuralEdit(
        tr("Delete pages"),
        [pages](core::PdfDocument &document) { document.deletePages(pages); }, &error);

    if (deleted) {
        session->setCurrentPage(qMax(0, pages.first() - 1));
        report(tr("Deleted %n page(s).", nullptr, int(pages.size())));
    } else {
        reportError(tr("Could not delete pages"), error);
    }
}

void MainWindow::extractPages(const QList<int> &pages)
{
    auto *session = currentSession();
    if (!session || !session->isLoaded() || !session->document()->isPdf())
        return;

    const QString text = QInputDialog::getText(this, tr("Extract pages"),
                                               tr("Pages to extract (for example 1-4, 8):"),
                                               QLineEdit::Normal, formatPageRange(pages));
    if (text.isEmpty())
        return;

    const QList<int> chosen = parsePageRange(text, session->pageCount());
    if (chosen.isEmpty()) {
        report(tr("No pages matched that range."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this, tr("Save extracted pages"),
                                                      suggestedPath(tr("-pages"),
                                                                    QStringLiteral("pdf")),
                                                      tr("PDF documents (*.pdf)"));
    if (path.isEmpty())
        return;

    try {
        const QByteArray bytes = session->document()->extractPages(chosen);
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()
            || !file.commit()) {
            reportError(tr("Could not write the file"), file.errorString());
            return;
        }
        report(tr("Extracted %n page(s) to %1.", nullptr, int(chosen.size()))
                   .arg(QFileInfo(path).fileName()));
    } catch (const std::exception &failure) {
        reportError(tr("Could not extract pages"), QString::fromUtf8(failure.what()));
    }
}

void MainWindow::saveEmbeddedFile(int index)
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return;

    const QList<core::EmbeddedFile> files = session->document()->embeddedFiles();
    if (index < 0 || index >= files.size())
        return;

    const core::EmbeddedFile &file = files.at(index);
    const QString suggested = file.name.isEmpty() ? tr("attachment") : QFileInfo(file.name).fileName();
    const QString path = QFileDialog::getSaveFileName(this, tr("Save embedded file"), suggested);
    if (path.isEmpty())
        return;

    const QByteArray bytes = session->document()->embeddedFileContents(index);
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
        reportError(tr("Could not write the file"), output.errorString());
        return;
    }
    report(tr("Saved %1.").arg(QFileInfo(path).fileName()));
}

void MainWindow::mergeDocuments()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument(true))
        return;

    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Merge into this document"),
                                                            QString(),
                                                            tr("PDF documents (*.pdf)"));
    if (paths.isEmpty())
        return;

    QString error;
    const bool merged = session->applyStructuralEdit(
        tr("Merge documents"),
        [paths](core::PdfDocument &document) {
            for (const QString &path : paths)
                document.appendPagesFrom(path);
        },
        &error);

    if (merged)
        report(tr("Merged %n document(s).", nullptr, int(paths.size())));
    else
        reportError(tr("Could not merge"), error);
}

void MainWindow::splitDocument()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded() || !session->document()->isPdf())
        return;

    bool modeAccepted = false;
    const QString mode = QInputDialog::getItem(
        this, tr("Split document"), tr("Split by:"),
        { tr("Every N pages"), tr("Bookmark"), tr("File size") }, 0, false, &modeAccepted);
    if (!modeAccepted)
        return;

    QList<QList<int>> parts;
    const int pageCount = session->pageCount();
    if (mode == tr("Bookmark")) {
        QList<int> starts;
        for (const core::OutlineNode &node : session->document()->outline()) {
            if (node.pageIndex >= 0)
                starts.append(node.pageIndex);
        }
        starts.removeAll(-1);
        std::sort(starts.begin(), starts.end());
        starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
        if (starts.isEmpty() || starts.first() != 0)
            starts.prepend(0);
        for (int i = 0; i < starts.size(); ++i) {
            const int end = i + 1 < starts.size() ? starts.at(i + 1) : pageCount;
            QList<int> pages;
            for (int page = starts.at(i); page < end; ++page)
                pages.append(page);
            if (!pages.isEmpty())
                parts.append(pages);
        }
    } else if (mode == tr("File size")) {
        bool accepted = false;
        const int megabytes = QInputDialog::getInt(this, tr("Split document"),
                                                   tr("Start a new file after about how many megabytes?"),
                                                   5, 1, 200, 1, &accepted);
        if (!accepted)
            return;
        const qint64 limit = qint64(megabytes) * 1024 * 1024;
        int first = 0;
        while (first < pageCount) {
            int last = first;
            while (last + 1 < pageCount) {
                QList<int> trial;
                for (int page = first; page <= last + 1; ++page)
                    trial.append(page);
                try {
                    if (session->document()->extractPages(trial).size() > limit)
                        break;
                } catch (const std::exception &) {
                    break;
                }
                ++last;
            }
            QList<int> pages;
            for (int page = first; page <= last; ++page)
                pages.append(page);
            parts.append(pages);
            first = last + 1;
        }
    } else {
        bool accepted = false;
        const int every = QInputDialog::getInt(this, tr("Split document"),
                                               tr("Split into files of how many pages?"), 1, 1,
                                               qMax(1, pageCount), 1, &accepted);
        if (!accepted)
            return;
        for (int first = 0; first < pageCount; first += every) {
            QList<int> pages;
            for (int index = first; index < qMin(first + every, pageCount); ++index)
                pages.append(index);
            parts.append(pages);
        }
    }

    if (parts.isEmpty()) {
        report(tr("Nothing to split."));
        return;
    }

    const QString directory = QFileDialog::getExistingDirectory(
        this, tr("Where should the parts go?"), QFileInfo(session->filePath()).absolutePath());
    if (directory.isEmpty())
        return;

    const QString base = QFileInfo(session->filePath()).completeBaseName();

    runWithProgress(tr("Splitting %1…").arg(session->displayName()),
                    [&](const std::function<bool(int, int)> &progress) {
                        int part = 1;
                        for (const QList<int> &pages : parts) {
                            if (!progress(part - 1, parts.size()))
                                return false;

                            const QString name = QStringLiteral("%1-%2.pdf")
                                                     .arg(base)
                                                     .arg(part, 3, 10, QLatin1Char('0'));
                            const QString path = QDir(directory).filePath(name);
                            try {
                                const QByteArray bytes = session->document()->extractPages(pages);
                                QSaveFile file(path);
                                if (!file.open(QIODevice::WriteOnly)
                                    || file.write(bytes) != bytes.size() || !file.commit()) {
                                    reportError(tr("Could not write the file"), file.errorString());
                                    return true;
                                }
                            } catch (const std::exception &failure) {
                                reportError(tr("Could not split"), QString::fromUtf8(failure.what()));
                                return true;
                            }
                            ++part;
                        }

                        report(tr("Split into %n file(s) in %1.", nullptr, parts.size()).arg(directory));
                        return true;
                    });
}

// --- Tool commands ---------------------------------------------------------

void MainWindow::replaceAllText()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument(true))
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Replace all"));
    auto *form = new QFormLayout;
    auto *find = new QLineEdit(&dialog);
    auto *replace = new QLineEdit(&dialog);
    auto *matchCase = new QCheckBox(tr("Match case"), &dialog);
    form->addRow(tr("Find"), find);
    form->addRow(tr("Replace with"), replace);
    form->addRow(QString(), matchCase);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *column = new QVBoxLayout(&dialog);
    column->addLayout(form);
    column->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted || find->text().isEmpty())
        return;

    struct Hit
    {
        int page = 0;
        QList<QRectF> boxes;
        QRectF area;
    };
    QList<Hit> hits;
    const auto sensitivity = matchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    for (int pageIndex = 0; pageIndex < session->pageCount(); ++pageIndex) {
        auto page = session->document()->page(pageIndex);
        if (!page || !page->textLayer())
            continue;
        for (const core::TextRange &range :
             page->textLayer()->findAll(find->text(), sensitivity, false)) {
            const QList<QRectF> boxes = page->textLayer()->boxes(range);
            if (boxes.isEmpty())
                continue;
            QRectF area = boxes.first();
            for (const QRectF &box : boxes)
                area = area.united(box);
            hits.append({ pageIndex, boxes, area });
        }
    }
    if (hits.isEmpty()) {
        report(tr("No matches for “%1”.").arg(find->text()));
        return;
    }

    const auto choice = QMessageBox::question(
        this, tr("Replace all"),
        tr("Replace %n occurrence(s)?", nullptr, hits.size()),
        QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Yes);
    if (choice != QMessageBox::Yes)
        return;

    const QString replacement = replace->text();
    session->undoStack()->beginMacro(tr("Replace all"));
    QString error;
    const bool cleared = session->applyStructuralEdit(
        tr("Remove old text"),
        [hits](core::PdfDocument &document) {
            QList<int> pages;
            for (const Hit &hit : hits) {
                core::AnnotationSpec mark;
                mark.type = core::AnnotationType::Redact;
                mark.pageIndex = hit.page;
                mark.quads = hit.boxes;
                mark.color = Qt::white;
                core::annotations::add(document, mark);
                if (!pages.contains(hit.page))
                    pages.append(hit.page);
            }
            for (int page : pages)
                document.applyRedactions(page);
        },
        &error);
    if (!cleared) {
        session->undoStack()->endMacro();
        session->undoStack()->undo();
        reportError(tr("Could not replace the text"), error);
        return;
    }

    if (!replacement.trimmed().isEmpty()) {
        for (const Hit &hit : hits) {
            core::AnnotationSpec spec;
            spec.type = core::AnnotationType::FreeText;
            spec.pageIndex = hit.page;
            spec.rect = hit.area.adjusted(-1, -1, 12, 1);
            if (spec.rect.height() < 10)
                spec.rect.setHeight(10);
            spec.contents = replacement;
            spec.fontSize = qBound(6.0, hit.area.height() * 0.82, 48.0);
            spec.color = Qt::black;
            spec.borderWidth = 0;
            spec.opacity = 1;
            spec.author = session->author();
            session->addAnnotation(spec, tr("Type replacement"));
        }
    }
    session->undoStack()->endMacro();
    report(tr("Replaced %n occurrence(s).", nullptr, hits.size()));
}

void MainWindow::addHeaderFooter()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument())
        return;

    HeaderFooterDialog dialog(session->pageCount(), session->currentPage(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (dialog.headerText().isEmpty() && dialog.footerText().isEmpty() && !dialog.pageNumbers()) {
        report(tr("Nothing to add."));
        return;
    }

    const auto expand = [](QString text, int page, int total) {
        return text.replace(QStringLiteral("{n}"), QString::number(page))
            .replace(QStringLiteral("{total}"), QString::number(total));
    };

    session->undoStack()->beginMacro(tr("Header and footer"));
    const int total = session->pageCount();
    for (int pageIndex = 0; pageIndex < total; ++pageIndex) {
        if (!dialog.allPages() && pageIndex != session->currentPage())
            continue;
        const QSizeF size = session->document()->pageSizePoints(pageIndex);
        const qreal font = dialog.fontSize();
        const auto place = [&](const QString &text, qreal y) {
            if (text.isEmpty())
                return;
            core::AnnotationSpec spec;
            spec.type = core::AnnotationType::FreeText;
            spec.pageIndex = pageIndex;
            spec.rect = QRectF(36, y, size.width() - 72, font + 8);
            spec.contents = text;
            spec.fontSize = font;
            spec.color = Qt::black;
            spec.borderWidth = 0;
            spec.opacity = 1;
            spec.author = session->author();
            session->addAnnotation(spec, tr("Header and footer"));
        };
        place(expand(dialog.headerText(), pageIndex + 1, total), 28);
        QString footer = dialog.footerText();
        if (dialog.pageNumbers()) {
            const QString number = tr("Page %1 of %2").arg(pageIndex + 1).arg(total);
            footer = footer.isEmpty() ? number : footer + QStringLiteral("  ") + number;
        }
        place(expand(footer, pageIndex + 1, total), size.height() - font - 36);
    }
    session->undoStack()->endMacro();
    report(tr("Header and footer added."));
}

void MainWindow::cropCurrentPage()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument(true))
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Crop page"));
    auto *form = new QFormLayout;
    const auto margin = [&dialog](const QString &label, QFormLayout *form) {
        auto *box = new QSpinBox(&dialog);
        box->setRange(0, 400);
        box->setSuffix(QObject::tr(" pt"));
        form->addRow(label, box);
        return box;
    };
    auto *left = margin(tr("Left"), form);
    auto *top = margin(tr("Top"), form);
    auto *right = margin(tr("Right"), form);
    auto *bottom = margin(tr("Bottom"), form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *column = new QVBoxLayout(&dialog);
    column->addLayout(form);
    column->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (left->value() + right->value() + top->value() + bottom->value() == 0) {
        report(tr("Enter a margin to crop."));
        return;
    }

    QString error;
    if (!session->cropPage(session->currentPage(), left->value(), top->value(), right->value(),
                           bottom->value(), &error))
        reportError(tr("Could not crop the page"), error);
    else
        report(tr("Cropped page %1.").arg(session->currentPage() + 1));
}

void MainWindow::manageLinks()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument())
        return;

    const int pageIndex = session->currentPage();
    const QList<core::content::Link> links =
        core::content::linksOnPage(*session->document(), pageIndex);
    if (links.isEmpty()) {
        report(tr("This page has no links."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Links on this page"));
    dialog.resize(420, 320);
    auto *list = new QListWidget(&dialog);
    for (const core::content::Link &link : links) {
        auto *item = new QListWidgetItem(link.uri, list);
        item->setData(Qt::UserRole, link.rect);
    }
    auto *edit = new QPushButton(tr("Change address"), &dialog);
    auto *remove = new QPushButton(tr("Delete"), &dialog);
    auto *close = new QPushButton(tr("Close"), &dialog);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(edit);
    buttons->addWidget(remove);
    buttons->addStretch(1);
    buttons->addWidget(close);
    auto *column = new QVBoxLayout(&dialog);
    column->addWidget(list);
    column->addLayout(buttons);
    QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(remove, &QPushButton::clicked, &dialog, [&] {
        auto *item = list->currentItem();
        if (!item)
            return;
        const QRectF rect = item->data(Qt::UserRole).toRectF();
        if (session->removeLinkAt(pageIndex, rect.center())) {
            delete item;
            report(tr("Link removed."));
        }
    });
    QObject::connect(edit, &QPushButton::clicked, &dialog, [&] {
        auto *item = list->currentItem();
        if (!item)
            return;
        bool accepted = false;
        const QString uri = QInputDialog::getText(&dialog, tr("Change link"), tr("Address:"),
                                                  QLineEdit::Normal, item->text(), &accepted);
        if (!accepted || uri.trimmed().isEmpty())
            return;
        const QRectF rect = item->data(Qt::UserRole).toRectF();
        if (session->removeLinkAt(pageIndex, rect.center()) && session->addLink(pageIndex, rect, uri.trimmed()))
            item->setText(uri.trimmed());
    });
    dialog.exec();
}

void MainWindow::addFormField()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument())
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("New form field"));
    auto *form = new QFormLayout;
    auto *kind = new QComboBox(&dialog);
    kind->addItem(tr("Text"), int(core::forms::NewFieldKind::Text));
    kind->addItem(tr("Checkbox"), int(core::forms::NewFieldKind::Checkbox));
    kind->addItem(tr("Dropdown"), int(core::forms::NewFieldKind::Combo));
    auto *name = new QLineEdit(&dialog);
    name->setPlaceholderText(tr("Field name"));
    auto *choices = new QLineEdit(&dialog);
    choices->setPlaceholderText(tr("Yes, No, Maybe"));
    form->addRow(tr("Type"), kind);
    form->addRow(tr("Name"), name);
    form->addRow(tr("Choices"), choices);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *column = new QVBoxLayout(&dialog);
    column->addLayout(form);
    column->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty())
        return;

    const auto fieldKind = static_cast<core::forms::NewFieldKind>(kind->currentData().toInt());
    QStringList options;
    if (fieldKind == core::forms::NewFieldKind::Combo) {
        for (const QString &part : choices->text().split(QLatin1Char(','))) {
            if (!part.trimmed().isEmpty())
                options.append(part.trimmed());
        }
        if (options.isEmpty()) {
            report(tr("A dropdown needs at least one choice."));
            return;
        }
    }

    const qreal height = fieldKind == core::forms::NewFieldKind::Checkbox ? 16 : 22;
    const QRectF rect(72, 72, fieldKind == core::forms::NewFieldKind::Checkbox ? 16 : 180, height);
    if (!session->addFormField(session->currentPage(), fieldKind, name->text().trimmed(), rect, options))
        reportError(tr("Could not add the field"), tr("The document did not accept a new field."));
    else
        report(tr("Added %1. It is listed in the Forms panel.").arg(name->text().trimmed()));
}

void MainWindow::compareDocument()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return;
    CompareDialog dialog(session->document(), this);
    dialog.exec();
}

void MainWindow::replyToSelection()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument())
        return;
    if (session->selectedAnnotation() == 0) {
        report(tr("Select a comment first."));
        return;
    }

    bool accepted = false;
    const QString text = QInputDialog::getMultiLineText(this, tr("Reply"), tr("Reply:"), QString(),
                                                        &accepted);
    if (!accepted || text.trimmed().isEmpty())
        return;

    core::Annotation parent;
    bool found = false;
    for (const core::Annotation &annotation :
         session->annotationsOnPage(session->selectedAnnotationPage())) {
        if (annotation.objectNumber == session->selectedAnnotation()) {
            parent = annotation;
            found = true;
            break;
        }
    }
    if (!found)
        return;

    core::AnnotationSpec spec;
    spec.type = core::AnnotationType::Note;
    spec.pageIndex = parent.pageIndex;
    spec.rect = QRectF(parent.rect.right() + 8, parent.rect.top(), 20, 20);
    spec.contents = text.trimmed();
    spec.author = session->author();
    spec.color = session->markupColor();
    spec.inReplyTo = parent.objectNumber;
    if (session->addAnnotation(spec, tr("Reply")))
        report(tr("Reply added."));
}

void MainWindow::chooseCustomStamp()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument())
        return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Stamp image"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (path.isEmpty())
        return;
    const QImage image(path);
    if (image.isNull()) {
        reportError(tr("Could not read the image"), path);
        return;
    }
    session->setCustomStamp(image);
    session->setTool(core::Tool::Stamp);
    report(tr("Click the page to place the stamp."));
}

void MainWindow::compressDocument()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded() || !session->document()->isPdf())
        return;

    const QString path = QFileDialog::getSaveFileName(this, tr("Save compressed copy"),
                                                      suggestedPath(tr("-compressed"),
                                                                    QStringLiteral("pdf")),
                                                      tr("PDF documents (*.pdf)"));
    if (path.isEmpty())
        return;

    core::SaveOptions options;
    options.incremental = false;
    options.garbageCollect = true;
    options.compressStreams = true;
    // Rewriting and sanitising the content streams is where most of the saving
    // comes from on documents produced by office software.
    options.cleanContentStreams = true;

    QString error;
    if (!session->saveAs(path, options, true, &error)) {
        reportError(tr("Could not compress"), error);
        return;
    }

    const qint64 before = QFileInfo(session->filePath()).size();
    const qint64 after = QFileInfo(path).size();
    if (before > 0 && after > 0) {
        const int percent = qRound(100.0 * (before - after) / before);
        report(tr("Compressed copy written: %1 → %2 (%3%).")
                   .arg(QLocale().formattedDataSize(before),
                        QLocale().formattedDataSize(after))
                   .arg(percent));
    } else {
        report(tr("Compressed copy written to %1.").arg(QFileInfo(path).fileName()));
    }
}

void MainWindow::protectDocument()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded() || !session->document()->isPdf())
        return;

    SecurityDialog dialog(session->document()->permissions(), session->document()->isEncrypted(),
                          this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString path = QFileDialog::getSaveFileName(this, tr("Save protected copy"),
                                                      suggestedPath(tr("-protected"),
                                                                    QStringLiteral("pdf")),
                                                      tr("PDF documents (*.pdf)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!session->saveAs(path, dialog.options(), true, &error)) {
        reportError(tr("Could not protect the document"), error);
        return;
    }
    report(tr("Protected copy written to %1.").arg(QFileInfo(path).fileName()));
}

void MainWindow::addWatermark()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument())
        return;

    WatermarkDialog dialog(session->pageCount(), session->currentPage(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QList<int> pages = dialog.pages();
    if (pages.isEmpty() || dialog.text().isEmpty()) {
        report(tr("Nothing to add."));
        return;
    }

    // One undo step for the whole watermark, however many pages it covers.
    session->undoStack()->beginMacro(tr("Add watermark"));
    for (const int pageIndex : pages) {
        const QSizeF size = session->document()->pageSizePoints(pageIndex);
        const qreal width = qMin(size.width() - 40.0, dialog.text().size() * dialog.fontSize() * 0.62);
        const qreal height = dialog.fontSize() * 1.6;

        qreal y = (size.height() - height) / 2.0;
        if (dialog.position() == WatermarkDialog::Position::Top)
            y = 36.0;
        else if (dialog.position() == WatermarkDialog::Position::Bottom)
            y = size.height() - height - 36.0;

        core::AnnotationSpec spec;
        spec.type = core::AnnotationType::FreeText;
        spec.pageIndex = pageIndex;
        spec.rect = QRectF((size.width() - width) / 2.0, y, width, height);
        spec.color = dialog.color();
        spec.opacity = dialog.opacity();
        spec.contents = dialog.text();
        spec.fontSize = dialog.fontSize();
        spec.author = session->author();
        session->addAnnotation(spec, tr("Add watermark"));
    }
    session->undoStack()->endMacro();

    report(tr("Watermark added to %n page(s).", nullptr, int(pages.size())));
}

void MainWindow::applyRedactions()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument(true))
        return;

    int marked = 0;
    for (int index = 0; index < session->pageCount(); ++index) {
        if (core::annotations::hasPendingRedactions(*session->document(), index))
            ++marked;
    }
    if (marked == 0) {
        report(tr("Mark something with the redaction tool first."));
        return;
    }

    const auto choice = QMessageBox::warning(
        this, tr("Apply redactions"),
        tr("This permanently removes the content under %n redaction mark(s). It cannot be "
           "undone once the document is saved.\n\nContinue?", nullptr, marked),
        QMessageBox::Cancel | QMessageBox::Apply, QMessageBox::Cancel);
    if (choice != QMessageBox::Apply)
        return;

    const int pageCount = session->pageCount();
    QString error;
    const bool applied = session->applyStructuralEdit(
        tr("Apply redactions"),
        [pageCount](core::PdfDocument &document) {
            for (int index = 0; index < pageCount; ++index)
                document.applyRedactions(index);
        },
        &error);

    if (applied)
        report(tr("Redactions applied on %n page(s).", nullptr, marked));
    else
        reportError(tr("Could not apply redactions"), error);
}

void MainWindow::recogniseText()
{
    auto *session = currentSession();
    if (!session || !session->isLoaded())
        return;

    OcrDialog dialog(suggestedPath(tr("-searchable"), QStringLiteral("pdf")), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString output = dialog.outputPath();
    if (output.isEmpty())
        return;

    runWithProgress(tr("Recognising text in %1…").arg(session->displayName()),
                    [&](const std::function<bool(int, int)> &progress) {
                        try {
                            core::exporter::toSearchablePdf(*session->document(), output,
                                                            dialog.language(), dialog.tessdataDir(),
                                                            dialog.dpi(), progress);
                        } catch (const std::exception &failure) {
                            reportError(tr("Could not recognise text"),
                                        QString::fromUtf8(failure.what()));
                            return true;
                        }

                        const auto choice = QMessageBox::question(
                            this, tr("Text recognised"),
                            tr("A searchable copy was written to %1.\n\nOpen it now?")
                                .arg(QFileInfo(output).fileName()),
                            QMessageBox::No | QMessageBox::Yes, QMessageBox::Yes);
                        if (choice == QMessageBox::Yes)
                            openFile(output);
                        return true;
                    });
}

void MainWindow::flattenDocument()
{
    auto *session = currentSession();
    if (!session || !requireEditableDocument(true))
        return;

    const auto choice = QMessageBox::question(
        this, tr("Flatten document"),
        tr("Comments, form values and signatures become part of the page and can no longer "
           "be edited or filled in.\n\nFlatten this document?"),
        QMessageBox::Cancel | QMessageBox::Ok, QMessageBox::Cancel);
    if (choice != QMessageBox::Ok)
        return;

    QString error;
    const bool done = session->applyStructuralEdit(
        tr("Flatten document"),
        [](core::PdfDocument &document) { core::forms::flatten(document, true, true); }, &error);
    if (done)
        report(tr("Everything is now part of the page content."));
    else
        reportError(tr("Could not flatten the document"), error);
}

void MainWindow::showPreferences()
{
    PreferencesDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // The theme applies itself; the author name only reaches open documents when
    // it is pushed into their sessions.
    const QSettings settings;
    const QString author = settings.value(PreferencesDialog::authorKey()).toString();
    for (int index = 0; index < m_tabs->count(); ++index) {
        if (PageView *view = viewAt(index))
            view->session()->setAuthor(author);
    }
    action(QStringLiteral("toggleTheme"))
        ->setChecked(settings.value(PreferencesDialog::darkModeKey(), false).toBool());
    report(tr("Settings saved."));
}

// --- Pictures, signatures and links -----------------------------------------

void MainWindow::addImageToPage()
{
    PageView *view = currentView();
    if (!view || !requireEditableDocument())
        return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose a picture"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff)"));
    if (path.isEmpty())
        return;

    const QImage picture(path);
    if (picture.isNull()) {
        reportError(tr("Could not open the picture"),
                    tr("%1 is not an image SweetPDF can read.").arg(QFileInfo(path).fileName()));
        return;
    }

    view->beginImagePlacement(picture, tr("Add picture"));
}

void MainWindow::replaceSelectedImage()
{
    auto *session = currentSession();
    PageView *view = currentView();
    if (!session || !view || !requireEditableDocument())
        return;

    // Only pictures SweetPDF placed itself can be swapped: they are stamp
    // annotations with their own box, so replacing one is delete plus add.
    const int objectNumber = session->selectedAnnotation();
    core::Annotation target;
    if (objectNumber != 0) {
        const QList<core::Annotation> onPage = session->annotationsOnPage(session->currentPage());
        for (const core::Annotation &annotation : onPage) {
            if (annotation.objectNumber == objectNumber
                && annotation.type == core::AnnotationType::Stamp) {
                target = annotation;
                break;
            }
        }
    }
    if (target.objectNumber == 0) {
        report(tr("Select a picture you added earlier, then choose Replace Image."));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose the replacement"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff)"));
    if (path.isEmpty())
        return;

    const QImage picture(path);
    if (picture.isNull()) {
        reportError(tr("Could not open the picture"),
                    tr("%1 is not an image SweetPDF can read.").arg(QFileInfo(path).fileName()));
        return;
    }

    session->undoStack()->beginMacro(tr("Replace picture"));
    session->removeAnnotation(target);
    session->addImage(target.pageIndex, picture,
                      core::content::fitToAspect(QSizeF(picture.size()), target.rect),
                      tr("Replace picture"));
    session->undoStack()->endMacro();
    report(tr("Picture replaced."));
}

void MainWindow::addLinkToPage()
{
    auto *session = currentSession();
    PageView *view = currentView();
    if (!session || !view || !requireEditableDocument())
        return;

    bool accepted = false;
    const QString entered = QInputDialog::getText(
        this, tr("Add link"),
        tr("Web address, or a page number in this document:"), QLineEdit::Normal,
        QStringLiteral("https://"), &accepted);
    if (!accepted || entered.trimmed().isEmpty())
        return;

    // A bare number means "jump to that page", anything else is a URL.
    const QString trimmed = entered.trimmed();
    bool isPageNumber = false;
    const int pageNumber = trimmed.toInt(&isPageNumber);
    QString uri = trimmed;
    if (isPageNumber) {
        const int pageIndex = qBound(0, pageNumber - 1, session->pageCount() - 1);
        uri = core::content::internalUri(*session->document(), pageIndex);
        if (uri.isEmpty()) {
            reportError(tr("Could not add the link"),
                        tr("This document does not allow links to its own pages."));
            return;
        }
    }

    // Text that is already selected is the obvious target, so link it straight
    // away instead of asking for a box.
    const QList<QRectF> selection = view->selectionBoxes();
    if (!selection.isEmpty()) {
        QRectF box = selection.first();
        for (const QRectF &part : selection)
            box = box.united(part);
        if (session->addLink(view->selectionPage(), box, uri)) {
            report(tr("Link added to the selected text."));
            return;
        }
    }

    view->beginLinkPlacement(uri);
}

void MainWindow::signDocument()
{
    auto *session = currentSession();
    PageView *view = currentView();
    if (!session || !view || !requireEditableDocument())
        return;

    SignatureDialog dialog(session->author(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QImage signature = dialog.signature();
    if (signature.isNull()) {
        report(tr("Draw, type or choose a signature first."));
        return;
    }

    if (!dialog.appliesToAllPages()) {
        view->beginImagePlacement(signature, tr("Sign"));
        return;
    }

    // On every page it goes bottom right, inside the margin, at a readable size.
    const qreal height = 46.0;
    const qreal width = height * signature.width() / qMax(1, signature.height());
    session->undoStack()->beginMacro(tr("Sign every page"));
    for (int index = 0; index < session->pageCount(); ++index) {
        const QSizeF size = session->document()->pageSizePoints(index);
        const QRectF box(size.width() - width - 54.0, size.height() - height - 54.0, width,
                         height);
        session->addImage(index, signature, box, tr("Sign"));
    }
    session->undoStack()->endMacro();
    report(tr("Signed %n page(s).", nullptr, session->pageCount()));
}

void MainWindow::replaceSelectedText()
{
    auto *session = currentSession();
    PageView *view = currentView();
    if (!session || !view || !requireEditableDocument(true))
        return;

    if (view->selectionBoxes().isEmpty()) {
        report(tr("Click the text you want to change, or drag across it."));
        if (QAction *tool = action(QStringLiteral("toolEditText")))
            tool->trigger();
        return;
    }

    const QString original = view->selectedText().simplified();
    bool accepted = false;
    const QString replacement = QInputDialog::getText(this, tr("Replace text"),
                                                      tr("Replace “%1” with:").arg(original),
                                                      QLineEdit::Normal, original, &accepted);
    if (!accepted)
        return;

    replaceSelectedTextWith(replacement, view->selectionPage(), view->selectionBoxes());
}

void MainWindow::replaceSelectedTextWith(const QString &replacement)
{
    PageView *view = currentView();
    if (!view)
        return;
    replaceSelectedTextWith(replacement, view->selectionPage(), view->selectionBoxes());
}

void MainWindow::replaceSelectedTextWith(const QString &replacement, int pageIndex,
                                         const QList<QRectF> &boxes)
{
    auto *session = currentSession();
    PageView *view = currentView();
    if (!session || !view || !requireEditableDocument(true))
        return;

    if (pageIndex < 0 || boxes.isEmpty()) {
        report(tr("Click the text you want to change, or drag across it."));
        return;
    }

    QRectF area = boxes.first();
    for (const QRectF &box : boxes)
        area = area.united(box);

    // PDF has no editable text runs, so replacing means removing the old glyphs
    // for real (a redaction) and typing the new ones on top as a text box. That
    // is also what keeps the old wording from being recoverable.
    session->undoStack()->beginMacro(tr("Replace text"));

    QString error;
    const bool cleared = session->applyStructuralEdit(
        tr("Remove old text"),
        [pageIndex, boxes](core::PdfDocument &document) {
            core::AnnotationSpec mark;
            mark.type = core::AnnotationType::Redact;
            mark.pageIndex = pageIndex;
            mark.quads = boxes;
            mark.color = Qt::white;
            core::annotations::add(document, mark);
            document.applyRedactions(pageIndex);
        },
        &error);

    if (!cleared) {
        session->undoStack()->endMacro();
        session->undoStack()->undo();
        reportError(tr("Could not replace the text"), error);
        return;
    }

    if (!replacement.trimmed().isEmpty()) {
        core::AnnotationSpec spec;
        spec.type = core::AnnotationType::FreeText;
        spec.pageIndex = pageIndex;
        // Sit the new text where the old text sat, at roughly its size.
        spec.rect = area.adjusted(-1, -1, 12, 1);
        if (spec.rect.height() < 10)
            spec.rect.setHeight(10);
        spec.contents = replacement;
        spec.fontSize = qBound(6.0, area.height() * 0.82, 48.0);
        spec.color = Qt::black;
        spec.borderWidth = 0.0;
        spec.opacity = 1.0;
        spec.author = session->author();
        if (!session->addAnnotation(spec, tr("Type replacement"))) {
            session->undoStack()->endMacro();
            reportError(tr("Could not replace the text"),
                        tr("The old text was removed, but the new text could not be written."));
            return;
        }
    }

    session->undoStack()->endMacro();
    view->clearSelection();
    report(tr("Text replaced on page %1.").arg(pageIndex + 1));
}

// --- Find ------------------------------------------------------------------

void MainWindow::showFindBar()
{
    PageView *view = currentView();
    FindBar *bar = currentFindBar();
    if (!view || !bar)
        return;

    // Seeded with the selection, the way browsers and editors do.
    bar->activate(view->hasSelection() ? view->selectedText().simplified() : QString());
}

FindBar *MainWindow::currentFindBar() const
{
    QWidget *page = m_tabs->widget(m_tabs->currentIndex());
    return page ? page->findChild<FindBar *>() : nullptr;
}

void MainWindow::revealToolOptions()
{
    // Picking a tool with settings opens the tool-options strip without turning
    // document Properties on — those stay behind Ctrl+J.
    updateInspectorVisibility();
}

void MainWindow::updateInspectorVisibility()
{
    if (!m_inspector || !m_toolOptions || !m_properties)
        return;

    if (m_tabs->count() == 0) {
        m_inspector->setVisible(false);
        return;
    }

    const bool showProps =
        action(QStringLiteral("toggleProperties"))
        && action(QStringLiteral("toggleProperties"))->isChecked();
    const bool showTools = m_toolOptions->hasOptions();
    m_toolOptions->setVisible(showTools);
    m_properties->setVisible(showProps);
    m_inspector->setVisible(showTools || showProps);
}

void MainWindow::syncToolActions()
{
    auto *session = currentSession();
    if (!session)
        return;

    const core::Tool tool = session->tool();
    const char *id = "toolSelect";
    switch (tool) {
    case core::Tool::Hand:
        id = "toolHand";
        break;
    case core::Tool::Highlight:
        id = "toolHighlight";
        break;
    case core::Tool::Underline:
        id = "toolUnderline";
        break;
    case core::Tool::Strikeout:
        id = "toolStrikeout";
        break;
    case core::Tool::Squiggly:
        id = "toolSquiggly";
        break;
    case core::Tool::Draw:
        id = "toolDraw";
        break;
    case core::Tool::Note:
        id = "toolNote";
        break;
    case core::Tool::AddText:
        id = "addText";
        break;
    case core::Tool::Shape:
        id = "addShape";
        break;
    case core::Tool::Ellipse:
        id = "addEllipse";
        break;
    case core::Tool::Line:
        id = "addLine";
        break;
    case core::Tool::Arrow:
        id = "addArrow";
        break;
    case core::Tool::Stamp:
        id = "toolStamp";
        break;
    case core::Tool::Redact:
        id = "toolRedact";
        break;
    case core::Tool::EditText:
        id = "toolEditText";
        break;
    case core::Tool::Select:
    case core::Tool::EditImage:
    case core::Tool::AddImage:
    default:
        id = "toolSelect";
        break;
    }

    if (QAction *entry = action(QString::fromLatin1(id)); entry && entry->isCheckable()) {
        const QSignalBlocker blocker(entry);
        entry->setChecked(true);
    }
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
    const QStringList files = recentFiles();
    if (m_welcome)
        m_welcome->setRecentFiles(files);

    if (!m_recentMenu)
        return;

    m_recentMenu->clear();
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

    QStringList open;
    for (int index = 0; index < m_tabs->count(); ++index) {
        if (PageView *view = viewAt(index)) {
            if (const QString path = view->session()->filePath(); !path.isEmpty())
                open << path;
        }
    }

    QSettings settings;
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("splitter"), m_splitter->saveState());
    settings.setValue(QStringLiteral("darkMode"), Theme::instance().mode() == ThemeMode::Dark);
    settings.setValue(PreferencesDialog::openDocumentsKey(), open);
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
