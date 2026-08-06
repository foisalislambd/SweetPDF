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

void DocumentSession::adoptDocument(std::shared_ptr<PdfDocument> document, const QString &path,
                                    bool resetHistory)
{
    m_document = std::move(document);
    m_filePath = path;
    m_selectedAnnotation = 0;
    m_selectedAnnotationPage = -1;

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

    // Incremental where possible: it is much faster on a large file and leaves
    // the original bytes, and any signatures over them, untouched.
    SaveOptions options;
    options.incremental = !m_document->isEncrypted();
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

// --- Pictures, signatures and links ----------------------------------------

bool DocumentSession::addImage(int pageIndex, const QImage &image, const QRectF &rect,
                               const QString &commandText)
{
    if (!m_document || !m_document->isPdf() || image.isNull() || rect.isEmpty())
        return false;

    // A picture is a stamp annotation, but its pixels are not something an
    // AnnotationSpec can carry, so undo goes through a document snapshot.
    const QString text = commandText.isEmpty() ? tr("Add image") : commandText;
    QString error;
    return applyStructuralEdit(
        text,
        [&](PdfDocument &document) { content::addImage(document, pageIndex, image, rect); },
        &error);
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

void DocumentSession::setModified(bool modified)
{
    if (modified == m_modified)
        return;
    m_modified = modified;
    Q_EMIT modifiedChanged(m_modified);
}

void DocumentSession::invalidatePage(int pageIndex)
{
    if (pageIndex < 0)
        m_renderQueue->cache().clear();
    else
        m_renderQueue->cache().invalidatePage(pageIndex);
    Q_EMIT pageInvalidated(pageIndex);
}

} // namespace sweet::core
