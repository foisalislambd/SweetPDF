#pragma once

#include "core/pdf/PdfDocument.h"
#include "core/render/RenderQueue.h"

#include <QObject>
#include <QString>
#include <QUndoStack>

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

public Q_SLOTS:
    void setCurrentPage(int pageIndex);
    void setZoom(qreal zoom);
    void setZoomMode(sweet::core::ZoomMode mode);
    void setRotation(int degrees);
    void rotateBy(int degrees);
    void setTool(sweet::core::Tool tool);
    void setLayout(sweet::core::PageLayout layout);
    void setModified(bool modified);
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
    void pageInvalidated(int pageIndex);

private:
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
};

} // namespace sweet::core

Q_DECLARE_METATYPE(sweet::core::Tool)
Q_DECLARE_METATYPE(sweet::core::PageLayout)
Q_DECLARE_METATYPE(sweet::core::ZoomMode)
