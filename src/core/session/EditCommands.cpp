#include "core/session/EditCommands.h"

#include "core/session/DocumentSession.h"

#include <QCoreApplication>

namespace sweet::core {
namespace {

/// Rebuilds a creation spec from an existing annotation, so undoing a delete puts
/// back something equivalent.
AnnotationSpec specFrom(const Annotation &annotation)
{
    AnnotationSpec spec;
    spec.type = annotation.type;
    spec.pageIndex = annotation.pageIndex;
    spec.rect = annotation.rect;
    spec.quads = annotation.quads;
    spec.strokes = annotation.strokes;
    spec.lineStart = annotation.lineStart;
    spec.lineEnd = annotation.lineEnd;
    spec.hasLineEndpoints = annotation.type == AnnotationType::Line;
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

// --- Add -------------------------------------------------------------------

AddAnnotationCommand::AddAnnotationCommand(DocumentSession *session, AnnotationSpec spec,
                                           const QString &text)
    : QUndoCommand(text)
    , m_session(session)
    , m_spec(std::move(spec))
{
}

void AddAnnotationCommand::redo()
{
    m_objectNumber = m_session->createAnnotation(m_spec);
}

void AddAnnotationCommand::undo()
{
    if (m_objectNumber != 0)
        m_session->deleteAnnotation(m_spec.pageIndex, m_objectNumber);
    m_objectNumber = 0;
}

// --- Remove ----------------------------------------------------------------

RemoveAnnotationCommand::RemoveAnnotationCommand(DocumentSession *session,
                                                 const Annotation &annotation)
    : QUndoCommand(QCoreApplication::translate("EditCommands", "Delete %1")
                       .arg(annotations::displayName(annotation.type).toLower()))
    , m_session(session)
    , m_spec(specFrom(annotation))
    , m_objectNumber(annotation.objectNumber)
{
}

void RemoveAnnotationCommand::redo()
{
    m_session->deleteAnnotation(m_spec.pageIndex, m_objectNumber);
    m_objectNumber = 0;
}

void RemoveAnnotationCommand::undo()
{
    m_objectNumber = m_session->createAnnotation(m_spec);
}

// --- Modify ----------------------------------------------------------------

ModifyAnnotationCommand::ModifyAnnotationCommand(DocumentSession *session,
                                                 const Annotation &before, const QColor &color,
                                                 qreal opacity, const QString &contents,
                                                 const QString &text)
    : QUndoCommand(text)
    , m_session(session)
    , m_pageIndex(before.pageIndex)
    , m_objectNumber(before.objectNumber)
    , m_oldColor(before.color)
    , m_newColor(color)
    , m_oldOpacity(before.opacity)
    , m_newOpacity(opacity)
    , m_oldContents(before.contents)
    , m_newContents(contents)
{
}

void ModifyAnnotationCommand::apply(const QColor &color, qreal opacity, const QString &contents)
{
    m_session->changeAnnotation(m_pageIndex, m_objectNumber, color, opacity, contents);
}

void ModifyAnnotationCommand::redo()
{
    apply(m_newColor, m_newOpacity, m_newContents);
}

void ModifyAnnotationCommand::undo()
{
    apply(m_oldColor, m_oldOpacity, m_oldContents);
}

bool ModifyAnnotationCommand::mergeWith(const QUndoCommand *other)
{
    // Typing in a note should be one undo step, not one per keystroke.
    const auto *next = static_cast<const ModifyAnnotationCommand *>(other);
    if (next->m_pageIndex != m_pageIndex || next->m_objectNumber != m_objectNumber)
        return false;

    m_newColor = next->m_newColor;
    m_newOpacity = next->m_newOpacity;
    m_newContents = next->m_newContents;
    return true;
}

// --- Form fields -----------------------------------------------------------

SetFormValueCommand::SetFormValueCommand(DocumentSession *session, int pageIndex, int objectNumber,
                                         QString oldValue, QString newValue, const QString &text)
    : QUndoCommand(text)
    , m_session(session)
    , m_pageIndex(pageIndex)
    , m_objectNumber(objectNumber)
    , m_oldValue(std::move(oldValue))
    , m_newValue(std::move(newValue))
{
}

void SetFormValueCommand::redo()
{
    m_session->writeFormValue(m_pageIndex, m_objectNumber, m_newValue);
}

void SetFormValueCommand::undo()
{
    m_session->writeFormValue(m_pageIndex, m_objectNumber, m_oldValue);
}

bool SetFormValueCommand::mergeWith(const QUndoCommand *other)
{
    const auto *next = static_cast<const SetFormValueCommand *>(other);
    if (next->m_pageIndex != m_pageIndex || next->m_objectNumber != m_objectNumber)
        return false;

    m_newValue = next->m_newValue;
    return true;
}

// --- Structural ------------------------------------------------------------

StructuralEditCommand::StructuralEditCommand(DocumentSession *session, QByteArray before,
                                             QByteArray after, const QString &text)
    : QUndoCommand(text)
    , m_session(session)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
}

void StructuralEditCommand::redo()
{
    // QUndoStack::push calls redo immediately, but the edit that created this
    // command has already been applied to the live document.
    if (m_alreadyApplied) {
        m_alreadyApplied = false;
        return;
    }
    m_session->restoreDocumentBytes(m_after);
}

void StructuralEditCommand::undo()
{
    m_session->restoreDocumentBytes(m_before);
}

} // namespace sweet::core
