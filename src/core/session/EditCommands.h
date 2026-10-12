#pragma once

#include "core/pdf/PdfAnnotations.h"
#include "core/pdf/PdfDocument.h"

#include <QByteArray>
#include <QUndoCommand>

#include <functional>

namespace sweet::core {

class DocumentSession;

/// Adds one annotation. Undo deletes it again; redo re-creates it from the same
/// specification, which is why the specification, not the created object, is what
/// the command stores.
class AddAnnotationCommand : public QUndoCommand
{
public:
    AddAnnotationCommand(DocumentSession *session, AnnotationSpec spec, const QString &text);

    void redo() override;
    void undo() override;

private:
    DocumentSession *m_session = nullptr;
    AnnotationSpec m_spec;
    int m_objectNumber = 0;
};

/// Deletes one annotation, keeping enough of it to put it back.
class RemoveAnnotationCommand : public QUndoCommand
{
public:
    RemoveAnnotationCommand(DocumentSession *session, const Annotation &annotation);

    void redo() override;
    void undo() override;

private:
    DocumentSession *m_session = nullptr;
    AnnotationSpec m_spec;
    int m_objectNumber = 0;
};

/// Changes the colour, opacity or text of an annotation.
class ModifyAnnotationCommand : public QUndoCommand
{
public:
    ModifyAnnotationCommand(DocumentSession *session, const Annotation &before,
                            const QColor &color, qreal opacity, const QString &contents,
                            const QString &text);

    void redo() override;
    void undo() override;

    int id() const override { return 0x5A11; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    void apply(const QColor &color, qreal opacity, const QString &contents);

    DocumentSession *m_session = nullptr;
    int m_pageIndex = -1;
    int m_objectNumber = 0;
    QColor m_oldColor;
    QColor m_newColor;
    qreal m_oldOpacity = 1.0;
    qreal m_newOpacity = 1.0;
    QString m_oldContents;
    QString m_newContents;
};

class MoveAnnotationCommand : public QUndoCommand
{
public:
    MoveAnnotationCommand(DocumentSession *session, int pageIndex, int objectNumber, QRectF before,
                          QRectF after, const QString &text);

    void redo() override;
    void undo() override;

private:
    DocumentSession *m_session = nullptr;
    int m_pageIndex = -1;
    int m_objectNumber = 0;
    QRectF m_before;
    QRectF m_after;
};

/// Rewrites the document description. Consecutive edits of the same file merge.
class SetInfoCommand : public QUndoCommand
{
public:
    SetInfoCommand(DocumentSession *session, DocumentInfo before, DocumentInfo after,
                   const QString &text);

    void redo() override;
    void undo() override;

    int id() const override { return 0x5A13; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    DocumentSession *m_session = nullptr;
    DocumentInfo m_before;
    DocumentInfo m_after;
};

/// Types a value into a form field. Filling in a form is an edit like any other,
/// so it belongs on the undo stack; consecutive edits of the same field merge so
/// that typing a name is one step rather than one per commit.
class SetFormValueCommand : public QUndoCommand
{
public:
    SetFormValueCommand(DocumentSession *session, int pageIndex, int objectNumber,
                        QString oldValue, QString newValue, const QString &text);

    void redo() override;
    void undo() override;

    int id() const override { return 0x5A12; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    DocumentSession *m_session = nullptr;
    int m_pageIndex = -1;
    int m_objectNumber = 0;
    QString m_oldValue;
    QString m_newValue;
};

/// Anything that changes the page tree: pages inserted, deleted, reordered,
/// merged in, or redactions burnt into the content.
///
/// These are undone by keeping the serialised document from before and after the
/// edit and re-opening it. Re-opening costs a copy of the file, but it is the
/// only way to guarantee that no cached page, display list or queued render still
/// points at an object that has been removed - and it makes undo exact rather
/// than an attempt to reverse the operation.
class StructuralEditCommand : public QUndoCommand
{
public:
    /// `before` and `after` are complete documents. The edit has already been
    /// applied when this command is constructed, so the first redo() does nothing.
    StructuralEditCommand(DocumentSession *session, QByteArray before, QByteArray after,
                          const QString &text);

    void redo() override;
    void undo() override;

private:
    DocumentSession *m_session = nullptr;
    QByteArray m_before;
    QByteArray m_after;
    bool m_alreadyApplied = true;
};

} // namespace sweet::core
