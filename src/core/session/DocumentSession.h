#pragma once

#include "core/pdf/PdfAnnotations.h"
#include "core/pdf/PdfContent.h"
#include "core/pdf/PdfDocument.h"
#include "core/pdf/PdfForms.h"
#include "core/render/RenderQueue.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QUndoStack>

#include <functional>
#include <memory>

namespace sweet::core {

/// What the pointer does on the page.
enum class Tool {
    Select,
    Hand,
    EditText,
    EditImage,
    AddText,
    AddImage,
    Highlight,
    Underline,
    Strikeout,
    Squiggly,
    Draw,
    Note,
    Redact,
    Shape,
    Ellipse,
    Line,
    Arrow,
    Callout,
    Stamp,
};

/// How pages are laid out in the viewport.
enum class PageLayout {
    SinglePage,
    Continuous,
    TwoPage,
    TwoPageContinuous,
};

/// How the zoom follows the viewport size.
enum class ZoomMode {
    Manual,
    FitPage,
    FitWidth,
};

/// Everything that belongs to one open tab: the document, its render pipeline,
/// the undo history and the view state. The UI holds no document state of its
/// own, so several views (page area, thumbnails, outline) always agree.
class DocumentSession : public QObject
{
    Q_OBJECT

public:
    explicit DocumentSession(QObject *parent = nullptr);
    ~DocumentSession() override;

    /// Opens `path` into this session. Returns false and reports the reason
    /// through `errorMessage` instead of throwing.
    bool load(const QString &path, const QString &password, QString *errorMessage);

    /// Replaces the session contents with a document created in memory, such as
    /// a new blank file. An empty path is shown as Untitled.
    void takeDocument(std::shared_ptr<PdfDocument> document, const QString &path);

    bool convertToGrayscale(QString *errorMessage = nullptr);

    const std::shared_ptr<PdfDocument> &document() const noexcept { return m_document; }
    RenderQueue *renderQueue() noexcept { return m_renderQueue; }
    QUndoStack *undoStack() noexcept { return m_undoStack; }

    bool isLoaded() const noexcept { return m_document != nullptr; }
    QString filePath() const;
    QString displayName() const; ///< File name, or "Untitled" when unsaved.
    bool isModified() const noexcept { return m_modified; }

    int pageCount() const;
    int currentPage() const noexcept { return m_currentPage; }
    qreal zoom() const noexcept { return m_zoom; }
    int rotation() const noexcept { return m_rotation; }
    Tool tool() const noexcept { return m_tool; }
    PageLayout layout() const noexcept { return m_layout; }
    ZoomMode zoomMode() const noexcept { return m_zoomMode; }

    static constexpr qreal minZoom = 0.08;
    static constexpr qreal maxZoom = 16.0;

    // --- Saving -----------------------------------------------------------

    /// Writes back to the file the document came from. Falls back to reporting
    /// false with a reason when there is nowhere to save to.
    bool save(QString *errorMessage);

    /// Writes to `path` and, unless `asCopy` is set, makes that the document's
    /// file from now on.
    bool saveAs(const QString &path, const SaveOptions &options, bool asCopy,
                QString *errorMessage);

    // --- Annotations ------------------------------------------------------

    /// Pushes an undoable "add annotation" step. Returns false when the document
    /// is not a PDF or does not allow annotating.
    bool addAnnotation(const AnnotationSpec &spec, const QString &commandText = QString());

    bool removeAnnotation(const Annotation &annotation);

    /// Changes colour, opacity or text. Consecutive edits of the same annotation
    /// merge into one undo step.
    bool modifyAnnotation(const Annotation &before, const QColor &color, qreal opacity,
                          const QString &contents);

    QList<Annotation> annotationsOnPage(int pageIndex) const;

    /// Currently selected annotation, or an object number of 0 for none.
    int selectedAnnotation() const noexcept { return m_selectedAnnotation; }
    int selectedAnnotationPage() const noexcept { return m_selectedAnnotationPage; }
    void setSelectedAnnotation(int pageIndex, int objectNumber);

    /// Writes the Info dictionary (title, author, subject, keywords). Other
    /// fields on `info` are written too, so callers should start from the
    /// document's current info and change only what the user edited.
    bool setDocumentInfo(const DocumentInfo &info, QString *errorMessage = nullptr);

    /// Colour and opacity new markup is created with.
    QColor markupColor() const noexcept { return m_markupColor; }
    void setMarkupColor(const QColor &color);
    qreal markupOpacity() const noexcept { return m_markupOpacity; }
    void setMarkupOpacity(qreal opacity);
    qreal markupBorderWidth() const noexcept { return m_markupBorderWidth; }
    void setMarkupBorderWidth(qreal width);
    /// Fill for new rectangles and ellipses. Invalid means no fill.
    QColor markupFill() const noexcept { return m_markupFill; }
    void setMarkupFill(const QColor &color);
    qreal markupFontSize() const noexcept { return m_markupFontSize; }
    void setMarkupFontSize(qreal points);
    /// Picture placed by the stamp tool. Null means a named stamp (Draft, …).
    QImage customStamp() const { return m_customStamp; }
    void setCustomStamp(const QImage &image);
    /// Darkens the page image for reading. It is not written into the file.
    bool nightMode() const noexcept { return m_nightMode; }
    void setNightMode(bool on);
    LineEnding lineEndStyle() const noexcept { return m_lineEndStyle; }
    void setLineEndStyle(LineEnding style);
    QString stampIconName() const noexcept { return m_stampIconName; }
    void setStampIconName(const QString &name);
    QString author() const noexcept { return m_author; }
    void setAuthor(const QString &author);

    // --- Pictures, signatures and links -----------------------------------

    /// Places `image` on `pageIndex` inside `rect` (page points) as an undoable
    /// step. Used by Add Image and by Sign.
    bool addImage(int pageIndex, const QImage &image, const QRectF &rect,
                  const QString &commandText = QString(), QString *errorMessage = nullptr);

    /// Adds a link over `rect`. `uri` is an external URL or an internal
    /// destination from `content::internalUri`.
    bool addLink(int pageIndex, const QRectF &rect, const QString &uri);
    bool removeLinkAt(int pageIndex, const QPointF &at);

    /// Moves or resizes an annotation, as one undo step.
    bool setAnnotationRect(int pageIndex, int objectNumber, const QRectF &rect);

    bool addFormField(int pageIndex, forms::NewFieldKind kind, const QString &name,
                      const QRectF &rect, const QStringList &choices);

    /// Crops one page by margins measured in points from its current edges.
    bool cropPage(int pageIndex, qreal left, qreal top, qreal right, qreal bottom,
                  QString *errorMessage = nullptr);

    // --- Forms ------------------------------------------------------------

    /// Fills in a form field, undoably. Consecutive edits of one field merge.
    bool setFormValue(int pageIndex, int objectNumber, const QString &value);

    /// Writes a field value without touching the undo stack. For the commands.
    bool writeFormValue(int pageIndex, int objectNumber, const QString &value);

    /// Shows or hides an optional-content layer and refreshes every page.
    bool setLayerEnabled(int index, bool enabled);

    /// Applies a named layer configuration and refreshes every page.
    bool selectLayerConfig(int index);

    bool addBookmark(const QString &title, int pageIndex);
    bool addChildBookmark(const QList<int> &parentPath, const QString &title, int pageIndex);
    bool renameBookmark(const QList<int> &path, const QString &title);
    bool removeBookmark(const QList<int> &path);

    // --- Structural edits -------------------------------------------------

    /// Runs `edit` on the document and pushes one undoable step for it, taking
    /// care of re-opening the document so nothing keeps pointing at removed
    /// objects. `edit` may throw MuError; the message is reported and the document
    /// left untouched.
    bool applyStructuralEdit(const QString &commandText,
                             const std::function<void(PdfDocument &)> &edit,
                             QString *errorMessage = nullptr);

    // Used by the undo commands. Not for general use: they do the work without
    // touching the undo stack.
    int createAnnotation(const AnnotationSpec &spec);
    bool deleteAnnotation(int pageIndex, int objectNumber);
    bool changeAnnotation(int pageIndex, int objectNumber, const QColor &color, qreal opacity,
                          const QString &contents);
    bool applyDocumentInfo(const DocumentInfo &info);
    bool applyAnnotationRect(int pageIndex, int objectNumber, const QRectF &rect);
    void restoreDocumentBytes(const QByteArray &bytes);

public Q_SLOTS:
    void setCurrentPage(int pageIndex);
    void setZoom(qreal zoom);
    void setZoomMode(sweet::core::ZoomMode mode);
    void setRotation(int degrees);
    void rotateBy(int degrees);
    void setTool(sweet::core::Tool tool);
    void setLayout(sweet::core::PageLayout layout);
    void setModified(bool modified);

    /// Throws away the rendered bitmaps of `pageIndex`, or of every page when it
    /// is -1, and tells the views. Call this after anything that changes how a
    /// page looks. Emitting pageInvalidated on its own is not enough: the views
    /// drop their copy but the render cache still holds the bitmap from before
    /// the edit, so the next repaint gets a cache hit and the page comes back
    /// looking exactly as it did.
    void invalidatePage(int pageIndex);

Q_SIGNALS:
    void documentChanged();
    void currentPageChanged(int pageIndex);
    void zoomChanged(qreal zoom);
    void zoomModeChanged(sweet::core::ZoomMode mode);
    void rotationChanged(int degrees);
    void toolChanged(sweet::core::Tool tool);
    void layoutChanged(sweet::core::PageLayout layout);
    void modifiedChanged(bool modified);
    /// Emitted by invalidatePage(), which is the only thing that should emit it.
    void pageInvalidated(int pageIndex);

    /// The annotations of `pageIndex` changed, or every page when it is -1.
    void annotationsChanged(int pageIndex);

    /// A form field on `pageIndex` was filled in, or every page when -1.
    void formsChanged(int pageIndex);

    void selectedAnnotationChanged(int pageIndex, int objectNumber);
    void markupStyleChanged();
    void nightModeChanged(bool on);
    void savedTo(const QString &path);

    /// A one-shot note about the document, such as a structure repair.
    void userNotice(const QString &message);

    /// The bookmark tree changed. Pages did not.
    void outlineChanged();

private:
    /// Swaps in a document and tells everyone. Used by load and by structural
    /// undo, which re-opens the document from bytes.
    void adoptDocument(std::shared_ptr<PdfDocument> document, const QString &path,
                       bool resetHistory);

    std::shared_ptr<PdfDocument> m_document;
    RenderQueue *m_renderQueue = nullptr;
    QUndoStack *m_undoStack = nullptr;

    QString m_filePath;
    bool m_modified = false;
    int m_currentPage = 0;
    qreal m_zoom = 1.0;
    int m_rotation = 0;
    Tool m_tool = Tool::Select;
    PageLayout m_layout = PageLayout::Continuous;
    ZoomMode m_zoomMode = ZoomMode::FitWidth;

    QColor m_markupColor = QColor(0xFF, 0xDD, 0x4A);
    qreal m_markupOpacity = 0.45;
    qreal m_markupBorderWidth = 1.5;
    QColor m_markupFill;
    qreal m_markupFontSize = 12.0;
    QImage m_customStamp;
    bool m_nightMode = false;
    LineEnding m_lineEndStyle = LineEnding::OpenArrow;
    QString m_stampIconName = QStringLiteral("Draft");
    QString m_author;
    int m_selectedAnnotation = 0;
    int m_selectedAnnotationPage = -1;
    bool m_structureNotified = false;

    void publishStructureNotice();
};

} // namespace sweet::core

Q_DECLARE_METATYPE(sweet::core::Tool)
Q_DECLARE_METATYPE(sweet::core::PageLayout)
Q_DECLARE_METATYPE(sweet::core::ZoomMode)
