#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QStackedLayout;
class QVBoxLayout;

namespace sweet::core {
class DocumentSession;
}

namespace sweet::ui {

/// Checkboxes for the document's optional content groups.
class LayersPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LayersPanel(QWidget *parent = nullptr);

    void setSession(sweet::core::DocumentSession *session);

private:
    void rebuild();

    sweet::core::DocumentSession *m_session = nullptr;
    QComboBox *m_configs = nullptr;
    int m_configRow = -1;
    QStackedLayout *m_stack = nullptr;
    QWidget *m_list = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
    QLabel *m_summary = nullptr;
    bool m_writing = false;
};

} // namespace sweet::ui
