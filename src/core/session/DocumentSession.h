#pragma once

#include "core/pdf/PdfAnnotations.h"
#include "core/pdf/PdfContent.h"
#include "core/pdf/PdfDocument.h"
#include "core/pdf/PdfForms.h"
#include "core/render/RenderQueue.h"

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
    Draw,
    Note,
    Redact,
    Shape,
    Ellipse,
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
    void setSelectedAnnotation(int pageIndex, int objectNumber);

    /// Colour and opacity new markup is created with.
    QColor markupColor() const noexcept { return m_markupColor; }
    void setMarkupColor(const QColor &color);
    qreal markupOpacity() const noexcept { return m_markupOpacity; }
    void setMarkupOpacity(qreal opacity);
    QString author() const noexcept { return m_author; }
    void setAuthor(const QString &author);

    // --- Pictures, signatures and links -----------------------------------

    /// Places `image` on `pageIndex` inside `rect` (page points) as an undoable
    /// step. Used by Add Image and by Sign.
    bool addImage(int pageIndex, const QImage &image, const QRectF &rect,
                  const QString &commandText = QString());

    /// Adds a link over `rect`. `uri` is an external URL or an internal
    /// destination from `content::internalUri`.
    bool addLink(int pageIndex, const QRectF &rect, const QString &uri);

    // --- Forms ------------------------------------------------------------

    /// Fills in a form field, undoably. Consecutive edits of one field merge.
    bool setFormValue(int pageIndex, int objectNumber, const QString &value);

    /// Writes a field value without touching the undo stack. For the commands.
    bool writeFormValue(int pageIndex, int objectNumber, const QString &value);

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
    void savedTo(const QString &path);

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
    QString m_author;
    int m_selectedAnnotation = 0;
    int m_selectedAnnotationPage = -1;
};

} // namespace sweet::core

Q_DECLARE_METATYPE(sweet::core::Tool)
Q_DECLARE_METATYPE(sweet::core::PageLayout)
Q_DECLARE_METATYPE(sweet::core::ZoomMode)
