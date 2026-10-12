#pragma once

#include <QWidget>

class QLabel;
class QListWidget;
class QStackedLayout;

namespace sweet::core {
class DocumentSession;
}

namespace sweet::ui {

/// Embedded files from the PDF name tree, with a way to save one out.
class AttachmentsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AttachmentsPanel(QWidget *parent = nullptr);

    void setSession(sweet::core::DocumentSession *session);

Q_SIGNALS:
    void saveRequested(int index);
    void statusMessage(const QString &message);

private:
    void rebuild();

    sweet::core::DocumentSession *m_session = nullptr;
    QStackedLayout *m_stack = nullptr;
    QListWidget *m_files = nullptr;
    QLabel *m_summary = nullptr;
};

} // namespace sweet::ui
