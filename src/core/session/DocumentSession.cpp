#include "core/session/DocumentSession.h"

#include "core/pdf/MuContext.h"
#include "core/pdf/PdfContent.h"
#include "core/pdf/PdfForms.h"
#include "core/session/DocumentCommand.h"
#include "core/session/EditCommands.h"
#include "core/util/Logging.h"

#include <QFileInfo>

namespace sweet::core {

void DocumentCommand::notifyPageChanged(int pageIndex)
{
    if (!m_session)
        return;
    m_session->setModified(true);
    m_session->invalidatePage(pageIndex);
}

DocumentSession::DocumentSession(QObject *parent)
    : QObject(parent)
    , m_renderQueue(new RenderQueue(this))
    , m_undoStack(new QUndoStack(this))
{
    connect(m_undoStack, &QUndoStack::cleanChanged, this,
            [this](bool clean) { setModified(!clean); });
}

DocumentSession::~DocumentSession() = default;

bool DocumentSession::load(const QString &path, const QString &password, QString *errorMessage)
{
    std::shared_ptr<PdfDocument> document;
    try {
        document = PdfDocument::open(path, password);
    } catch (const MuError &failure) {
        if (errorMessage)
            *errorMessage = failure.details();
        return false;
    } catch (const std::exception &failure) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(failure.what());
        return false;
    }

    m_currentPage = 0;
    m_rotation = 0;
    adoptDocument(std::move(document), path, true);

    qCInfo(lcPdf) << "Opened" << path << "with" << m_document->pageCount() << "pages";
    return true;
}

void DocumentSession::takeDocument(std::shared_ptr<PdfDocument> document, const QString &path)
{
    adoptDocument(std::move(document), path, true);
    if (path.isEmpty())
        setModified(true);
}

bool DocumentSession::convertToGrayscale(QString *errorMessage)
{
    if (!m_document || !m_document->isPdf()) {
        if (errorMessage)
            *errorMessage = tr("Only PDF files can be converted to grayscale.");
        return false;
    }
    return applyStructuralEdit(tr("Grayscale"),
                               [](PdfDocument &document) { document.convertToGrayscale(); },
                               errorMessage);
}

void DocumentSession::adoptDocument(std::shared_ptr<PdfDocument> document, const QString &path,
                                    bool resetHistory)
{
    m_document = std::move(document);
    m_filePath = path;
    m_selectedAnnotation = 0;
    m_selectedAnnotationPage = -1;
    m_structureNotified = false;

    if (resetHistory) {
        m_undoStack->clear();
        m_modified = false;
    }

    m_currentPage = qBound(0, m_currentPage, qMax(0, pageCount() - 1));
    m_renderQueue->setDocument(m_document);

    Q_EMIT documentChanged();
    Q_EMIT currentPageChanged(m_currentPage);
    Q_EMIT annotationsChanged(-1);
    if (resetHistory)
        Q_EMIT modifiedChanged(m_modified);
}

// --- Saving ----------------------------------------------------------------

bool DocumentSession::save(QString *errorMessage)
{
    if (!m_document) {
        if (errorMessage)
            *errorMessage = tr("There is no document to save.");
        return false;
    }
    if (m_filePath.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("This document has never been saved. Use Save As.");
        return false;
    }

    // Incremental where MuPDF allows it: faster on large files and leaves
    // original bytes (and signatures over them) untouched. Memory-backed or
    // structurally rewritten documents must do a full save.
    SaveOptions options;
    options.incremental = m_document->canSaveIncrementally();
    return saveAs(m_filePath, options, false, errorMessage);
}

bool DocumentSession::saveAs(const QString &path, const SaveOptions &options, bool asCopy,
                             QString *errorMessage)
{
    if (!m_document) {
        if (errorMessage)
            *errorMessage = tr("There is no document to save.");
        return false;
    }

    try {
        m_document->saveToFile(path, options);
    } catch (const MuError &failure) {
        if (errorMessage)
            *errorMessage = failure.details();
        return false;
    } catch (const std::exception &failure) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(failure.what());
        return false;
    }

    if (!asCopy) {
        // The saved file is now the document's identity, and the history so far is
        // the history of a file that matches what is on disk.
        m_filePath = path;
        m_undoStack->setClean();
        setModified(false);
    }

    publishStructureNotice();
    Q_EMIT savedTo(path);
    return true;
}

// --- Annotations -----------------------------------------------------------

bool DocumentSession::addAnnotation(const AnnotationSpec &spec, const QString &commandText)
{
    if (!m_document || !m_document->isPdf())
        return false;

    const QString text = commandText.isEmpty()
        ? tr("Add %1").arg(annotations::displayName(spec.type).toLower())
        : commandText;
    m_undoStack->push(new AddAnnotationCommand(this, spec, text));
    return true;
}

bool DocumentSession::removeAnnotation(const Annotation &annotation)
{
    if (!m_document || !m_document->isPdf() || annotation.objectNumber == 0)
        return false;

    m_undoStack->push(new RemoveAnnotationCommand(this, annotation));
    return true;
}

bool DocumentSession::modifyAnnotation(const Annotation &before, const QColor &color,
                                       qreal opacity, const QString &contents)
{
    if (!m_document || !m_document->isPdf() || before.objectNumber == 0)
        return false;
    if (before.color == color && qFuzzyCompare(before.opacity, opacity)
        && before.contents == contents) {
        return false;
    }

    m_undoStack->push(new ModifyAnnotationCommand(
        this, before, color, opacity, contents,
        tr("Edit %1").arg(annotations::displayName(before.type).toLower())));
    return true;
}

QList<Annotation> DocumentSession::annotationsOnPage(int pageIndex) const
{
    if (!m_document || !m_document->isPdf())
        return {};
    return annotations::onPage(*m_document, pageIndex);
}

bool DocumentSession::setDocumentInfo(const DocumentInfo &info, QString *errorMessage)
{
    if (!m_document || !m_document->isPdf()) {
        if (errorMessage)
            *errorMessage = tr("Only PDF files store a title, author and subject.");
        return false;
    }

    const DocumentInfo &current = m_document->info();
    if (current.title == info.title && current.author == info.author
        && current.subject == info.subject && current.keywords == info.keywords
        && current.creator == info.creator && current.producer == info.producer
        && current.creationDate == info.creationDate
        && current.modificationDate == info.modificationDate) {
        return true;
    }

    try {
        m_undoStack->push(new SetInfoCommand(this, current, info, tr("Edit description")));
    } catch (const MuError &failure) {
        if (errorMessage)
            *errorMessage = failure.details();
        return false;
    }
    return true;
}

void DocumentSession::setSelectedAnnotation(int pageIndex, int objectNumber)
{
    if (m_selectedAnnotationPage == pageIndex && m_selectedAnnotation == objectNumber)
        return;

    m_selectedAnnotationPage = pageIndex;
    m_selectedAnnotation = objectNumber;
    Q_EMIT selectedAnnotationChanged(pageIndex, objectNumber);
}

void DocumentSession::setMarkupColor(const QColor &color)
{
    if (!color.isValid() || color == m_markupColor)
        return;
    m_markupColor = color;
    Q_EMIT markupStyleChanged();
}

void DocumentSession::setMarkupOpacity(qreal opacity)
{
    const qreal clamped = qBound(0.05, opacity, 1.0);
    if (qFuzzyCompare(clamped, m_markupOpacity))
        return;
    m_markupOpacity = clamped;
    Q_EMIT markupStyleChanged();
}

void DocumentSession::setMarkupBorderWidth(qreal width)
{
    const qreal clamped = qBound(0.5, width, 12.0);
    if (qFuzzyCompare(clamped, m_markupBorderWidth))
        return;
    m_markupBorderWidth = clamped;
    Q_EMIT markupStyleChanged();
}

void DocumentSession::setMarkupFill(const QColor &color)
{
    if (color == m_markupFill)
        return;
    m_markupFill = color;
    Q_EMIT markupStyleChanged();
}

void DocumentSession::setMarkupFontSize(qreal points)
{
    const qreal clamped = qBound(6.0, points, 72.0);
    if (qFuzzyCompare(clamped, m_markupFontSize))
        return;
    m_markupFontSize = clamped;
    Q_EMIT markupStyleChanged();
}

void DocumentSession::setCustomStamp(const QImage &image)
{
    m_customStamp = image;
    Q_EMIT markupStyleChanged();
}

void DocumentSession::setNightMode(bool on)
{
    if (on == m_nightMode)
        return;
    m_nightMode = on;
    Q_EMIT nightModeChanged(m_nightMode);
}

void DocumentSession::setLineEndStyle(LineEnding style)
{
    if (style == m_lineEndStyle)
        return;
    m_lineEndStyle = style;
    Q_EMIT markupStyleChanged();
}

void DocumentSession::setStampIconName(const QString &name)
{
    const bool hadImage = !m_customStamp.isNull();
    m_customStamp = QImage();
    if (name.isEmpty() || name == m_stampIconName) {
        if (hadImage)
            Q_EMIT markupStyleChanged();
        return;
    }
    m_stampIconName = name;
    Q_EMIT markupStyleChanged();
}

void DocumentSession::setAuthor(const QString &author)
{
    m_author = author;
}

int DocumentSession::createAnnotation(const AnnotationSpec &spec)
{
    if (!m_document)
        return 0;

    const int objectNumber = annotations::add(*m_document, spec);
    if (objectNumber != 0) {
        setModified(true);
        invalidatePage(spec.pageIndex);
        Q_EMIT annotationsChanged(spec.pageIndex);
    }
    return objectNumber;
}

bool DocumentSession::deleteAnnotation(int pageIndex, int objectNumber)
{
    if (!m_document)
        return false;

    const bool removed = annotations::remove(*m_document, pageIndex, objectNumber);
    if (removed) {
        if (m_selectedAnnotation == objectNumber)
            setSelectedAnnotation(-1, 0);
        setModified(true);
        invalidatePage(pageIndex);
        Q_EMIT annotationsChanged(pageIndex);
    }
    return removed;
}

bool DocumentSession::changeAnnotation(int pageIndex, int objectNumber, const QColor &color,
                                       qreal opacity, const QString &contents)
{
    if (!m_document)
        return false;

    const bool changed =
        annotations::update(*m_document, pageIndex, objectNumber, color, opacity, contents);
    if (changed) {
        setModified(true);
        invalidatePage(pageIndex);
        Q_EMIT annotationsChanged(pageIndex);
    }
    return changed;
}

bool DocumentSession::applyDocumentInfo(const DocumentInfo &info)
{
    if (!m_document || !m_document->isPdf())
        return false;
    m_document->setInfo(info);
    setModified(true);
    Q_EMIT documentChanged();
    return true;
}

bool DocumentSession::applyAnnotationRect(int pageIndex, int objectNumber, const QRectF &rect)
{
    if (!m_document)
        return false;
    const bool changed = annotations::setRect(*m_document, pageIndex, objectNumber, rect);
    if (changed) {
        setModified(true);
        invalidatePage(pageIndex);
        Q_EMIT annotationsChanged(pageIndex);
    }
    return changed;
}

bool DocumentSession::setAnnotationRect(int pageIndex, int objectNumber, const QRectF &rect)
{
    if (!m_document || !m_document->isPdf() || objectNumber == 0 || rect.isEmpty())
        return false;

    QRectF before;
    for (const Annotation &annotation : annotationsOnPage(pageIndex)) {
        if (annotation.objectNumber == objectNumber) {
            before = annotation.rect;
            break;
        }
    }
    if (before.isNull() || before == rect)
        return false;

    m_undoStack->push(new MoveAnnotationCommand(this, pageIndex, objectNumber, before, rect,
                                                tr("Move annotation")));
    return true;
}

// --- Pictures, signatures and links ----------------------------------------

bool DocumentSession::addImage(int pageIndex, const QImage &image, const QRectF &rect,
                               const QString &commandText, QString *errorMessage)
{
    if (!m_document || !m_document->isPdf() || image.isNull() || rect.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("Could not place the image on the page.");
        return false;
    }

    // A picture is a stamp annotation, but its pixels are not something an
    // AnnotationSpec can carry, so undo goes through a document snapshot.
    const QString text = commandText.isEmpty() ? tr("Add image") : commandText;
    return applyStructuralEdit(
        text,
        [&](PdfDocument &document) {
            if (content::addImage(document, pageIndex, image, rect) == 0)
                throw MuError(tr("Could not place the image on the page."));
        },
        errorMessage);
}

bool DocumentSession::addLink(int pageIndex, const QRectF &rect, const QString &uri)
{
    if (!m_document || !m_document->isPdf() || rect.isEmpty() || uri.isEmpty())
        return false;

    QString error;
    return applyStructuralEdit(
        tr("Add link"),
        [&](PdfDocument &document) { content::addLink(document, pageIndex, rect, uri); }, &error);
}

bool DocumentSession::removeLinkAt(int pageIndex, const QPointF &at)
{
    if (!m_document || !m_document->isPdf())
        return false;

    QString error;
    return applyStructuralEdit(
        tr("Remove link"),
        [&](PdfDocument &document) { content::removeLinkAt(document, pageIndex, at); }, &error);
}

bool DocumentSession::addFormField(int pageIndex, forms::NewFieldKind kind, const QString &name,
                                   const QRectF &rect, const QStringList &choices)
{
    if (!m_document || !m_document->isPdf())
        return false;

    QString error;
    const bool added = applyStructuralEdit(
        tr("Add form field"),
        [&](PdfDocument &document) {
            forms::createField(document, pageIndex, kind, name, rect, choices);
        },
        &error);
    if (added)
        Q_EMIT formsChanged(pageIndex);
    return added;
}

bool DocumentSession::cropPage(int pageIndex, qreal left, qreal top, qreal right, qreal bottom,
                               QString *errorMessage)
{
    if (!m_document || !m_document->isPdf())
        return false;

    return applyStructuralEdit(
        tr("Crop page"),
        [=](PdfDocument &document) { document.cropPage(pageIndex, left, top, right, bottom); },
        errorMessage);
}

// --- Forms -----------------------------------------------------------------

bool DocumentSession::setFormValue(int pageIndex, int objectNumber, const QString &value)
{
    if (!m_document || !m_document->isPdf() || objectNumber == 0)
        return false;

    // The old value is read back from the document so undo restores exactly what
    // was there, including a value the panel never showed.
    QString previous;
    const QList<FormField> fields = forms::onPage(*m_document, pageIndex);
    for (const FormField &field : fields) {
        if (field.objectNumber == objectNumber) {
            previous = field.value;
            break;
        }
    }
    if (previous == value)
        return false;

    m_undoStack->push(new SetFormValueCommand(this, pageIndex, objectNumber, previous, value,
                                             tr("Fill in form field")));
    return true;
}

bool DocumentSession::writeFormValue(int pageIndex, int objectNumber, const QString &value)
{
    if (!m_document)
        return false;

    const bool written = forms::setValue(*m_document, pageIndex, objectNumber, value);
    if (written) {
        setModified(true);
        invalidatePage(pageIndex);
        Q_EMIT formsChanged(pageIndex);
    }
    return written;
}

// --- Structural edits ------------------------------------------------------

bool DocumentSession::applyStructuralEdit(const QString &commandText,
                                          const std::function<void(PdfDocument &)> &edit,
                                          QString *errorMessage)
{
    if (!m_document || !m_document->isPdf()) {
        if (errorMessage)
            *errorMessage = tr("This file format can be viewed but not edited.");
        return false;
    }

    QByteArray before;
    QByteArray after;
    try {
        before = m_document->toBytes();
        edit(*m_document);
        after = m_document->toBytes();
    } catch (const MuError &failure) {
        if (errorMessage)
            *errorMessage = failure.details();
        // The in-place edit may have half-applied, so go back to known-good bytes.
        if (!before.isEmpty())
            restoreDocumentBytes(before);
        return false;
    } catch (const std::exception &failure) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(failure.what());
        if (!before.isEmpty())
            restoreDocumentBytes(before);
        return false;
    }

    // Re-open even though the edit succeeded: cached pages and queued renders
    // still refer to the page tree as it was.
    restoreDocumentBytes(after);
    m_undoStack->push(new StructuralEditCommand(this, std::move(before), std::move(after),
                                                commandText));
    setModified(true);
    return true;
}

void DocumentSession::restoreDocumentBytes(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return;

    try {
        auto reopened = PdfDocument::openFromBytes(bytes, m_filePath);
        adoptDocument(std::move(reopened), m_filePath, false);
    } catch (const std::exception &failure) {
        qCWarning(lcPdf) << "Could not re-open the document after an edit:" << failure.what();
        return;
    }

    setModified(true);
    invalidatePage(-1);
}

QString DocumentSession::filePath() const
{
    return m_filePath;
}

QString DocumentSession::displayName() const
{
    if (m_filePath.isEmpty())
        return tr("Untitled");
    return QFileInfo(m_filePath).fileName();
}

int DocumentSession::pageCount() const
{
    return m_document ? m_document->pageCount() : 0;
}

void DocumentSession::setCurrentPage(int pageIndex)
{
    const int clamped = qBound(0, pageIndex, qMax(0, pageCount() - 1));
    if (clamped == m_currentPage)
        return;
    m_currentPage = clamped;
    Q_EMIT currentPageChanged(m_currentPage);
}

void DocumentSession::setZoom(qreal zoom)
{
    const qreal clamped = qBound(minZoom, zoom, maxZoom);
    if (qFuzzyCompare(clamped, m_zoom))
        return;
    m_zoom = clamped;
    Q_EMIT zoomChanged(m_zoom);
}

void DocumentSession::setZoomMode(ZoomMode mode)
{
    if (mode == m_zoomMode)
        return;
    m_zoomMode = mode;
    Q_EMIT zoomModeChanged(m_zoomMode);
}

void DocumentSession::setRotation(int degrees)
{
    const int normalised = ((degrees % 360) + 360) % 360;
    if (normalised == m_rotation)
        return;
    m_rotation = normalised;
    m_renderQueue->discardQueued();
    m_renderQueue->cache().clear();
    Q_EMIT rotationChanged(m_rotation);
}

void DocumentSession::rotateBy(int degrees)
{
    setRotation(m_rotation + degrees);
}

void DocumentSession::setTool(Tool tool)
{
    if (tool == m_tool)
        return;
    m_tool = tool;
    Q_EMIT toolChanged(m_tool);
}

void DocumentSession::setLayout(PageLayout layout)
{
    if (layout == m_layout)
        return;
    m_layout = layout;
    Q_EMIT layoutChanged(m_layout);
}

void DocumentSession::publishStructureNotice()
{
    if (m_structureNotified || !m_document)
        return;

    QString message;
    if (m_document->structureRepaired()) {
        message = tr("MuPDF repaired this PDF's structure. Check the pages, then save if "
                     "they look right.");
    } else if (!m_document->structureWarning().isEmpty()) {
        message = tr("Could not check this PDF's structure: %1")
                      .arg(m_document->structureWarning());
    } else {
        return;
    }

    m_structureNotified = true;
    Q_EMIT userNotice(message);
}

void DocumentSession::setModified(bool modified)
{
    publishStructureNotice();
    if (modified == m_modified)
        return;
    m_modified = modified;
    Q_EMIT modifiedChanged(m_modified);
}

bool DocumentSession::setLayerEnabled(int index, bool enabled)
{
    if (!m_document || !m_document->setLayerEnabled(index, enabled))
        return false;
    setModified(true);
    invalidatePage(-1);
    return true;
}

bool DocumentSession::selectLayerConfig(int index)
{
    if (!m_document || !m_document->selectLayerConfig(index))
        return false;
    setModified(true);
    invalidatePage(-1);
    return true;
}

bool DocumentSession::addBookmark(const QString &title, int pageIndex)
{
    if (!m_document || !m_document->addBookmark(title, pageIndex))
        return false;
    setModified(true);
    Q_EMIT outlineChanged();
    return true;
}

bool DocumentSession::renameBookmark(const QList<int> &path, const QString &title)
{
    if (!m_document || !m_document->renameBookmark(path, title))
        return false;
    setModified(true);
    Q_EMIT outlineChanged();
    return true;
}

bool DocumentSession::removeBookmark(const QList<int> &path)
{
    if (!m_document || !m_document->removeBookmark(path))
        return false;
    setModified(true);
    Q_EMIT outlineChanged();
    return true;
}

void DocumentSession::invalidatePage(int pageIndex)
{
    if (m_document)
        m_document->discardPageCaches(pageIndex);

    if (pageIndex < 0)
        m_renderQueue->cache().clear();
    else
        m_renderQueue->cache().invalidatePage(pageIndex);
    Q_EMIT pageInvalidated(pageIndex);
}

} // namespace sweet::core
