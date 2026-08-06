#pragma once

#include <QWidget>

class QFormLayout;
class QLabel;

namespace sweet::core {
class DocumentSession;
}

namespace sweet::ui {

/// Right-hand inspector.
///
/// With nothing selected it shows the document's own properties, which is the
/// information people look for most often. Once object selection lands, the
/// appearance section below takes over for the selected annotation or text box.
class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget *parent = nullptr);

    void setSession(sweet::core::DocumentSession *session);

private:
    void refresh();
    QLabel *addField(QFormLayout *form, const QString &label);

    sweet::core::DocumentSession *m_session = nullptr;

    QLabel *m_fileName = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_author = nullptr;
    QLabel *m_producer = nullptr;
    QLabel *m_format = nullptr;
    QLabel *m_pageCount = nullptr;
    QLabel *m_pageSize = nullptr;
    QLabel *m_fileSize = nullptr;
    QLabel *m_security = nullptr;
    QLabel *m_permissions = nullptr;
};

} // namespace sweet::ui
