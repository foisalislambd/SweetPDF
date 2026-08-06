#pragma once

#include <QString>
#include <QUndoCommand>

namespace sweet::core {

class DocumentSession;

/// Base class for every edit. Commands never touch the UI: they mutate the
/// document through the session and report which pages need to be redrawn, so
/// undo and redo repaint exactly the same area as the original edit.
class DocumentCommand : public QUndoCommand
{
public:
    DocumentCommand(DocumentSession *session, const QString &text)
        : QUndoCommand(text)
        , m_session(session)
    {
    }

    DocumentSession *session() const noexcept { return m_session; }

protected:
    /// Marks the document dirty and asks views to redraw `pageIndex`.
    /// Pass -1 when the whole document changed.
    void notifyPageChanged(int pageIndex);

private:
    DocumentSession *m_session = nullptr;
};

} // namespace sweet::core
