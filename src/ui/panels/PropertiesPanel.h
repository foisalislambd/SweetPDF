#pragma once

#include <QList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QStackedWidget;
class QToolButton;

namespace sweet::core {
class DocumentSession;
}

namespace sweet::ui {

/// Right-hand inspector, in the three panes a desktop PDF editor uses.
///
/// File holds the description and security of the document. Page describes the
/// page on screen. Selection edits the comment, shape or text box that is
/// picked on the page: colour, opacity and the note, which are what the
/// properties bar in Acrobat and Foxit is for.
class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget *parent = nullptr);

    void setSession(sweet::core::DocumentSession *session);

Q_SIGNALS:
    /// The user picked something on the page, so the inspector should be open
    /// on the Selection pane.
    void revealRequested();

    /// A command offered for the selection, by action id.
    void commandRequested(const QString &actionId);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Pane { File, Page, Selection };

    void refresh();
    void showPane(Pane pane);
    void commitMetadata();
    void commitSelection();

    void setValue(QLabel *label, const QString &text);
    void setEditor(QLineEdit *editor, const QString &text);

    sweet::core::DocumentSession *m_session = nullptr;
    bool m_writing = false;
    int m_shownObject = 0;

    QStackedWidget *m_panes = nullptr;
    QList<QToolButton *> m_tabs;

    QLineEdit *m_title = nullptr;
    QLineEdit *m_author = nullptr;
    QLineEdit *m_subject = nullptr;
    QLineEdit *m_keywords = nullptr;
    QLabel *m_fileName = nullptr;
    QLabel *m_location = nullptr;
    QLabel *m_fileSize = nullptr;
    QLabel *m_format = nullptr;
    QLabel *m_pageCount = nullptr;
    QLabel *m_creator = nullptr;
    QLabel *m_producer = nullptr;
    QLabel *m_created = nullptr;
    QLabel *m_modified = nullptr;
    QLabel *m_saved = nullptr;
    QLabel *m_encryption = nullptr;
    QLabel *m_permPrint = nullptr;
    QLabel *m_permEdit = nullptr;
    QLabel *m_permCopy = nullptr;
    QLabel *m_permComment = nullptr;
    QLabel *m_permForm = nullptr;
    QLabel *m_permAssemble = nullptr;
    QLabel *m_metaStatus = nullptr;
    QWidget *m_metaEditors = nullptr;

    QLabel *m_pageNumber = nullptr;
    QLabel *m_paper = nullptr;
    QLabel *m_pageMillimetres = nullptr;
    QLabel *m_pagePoints = nullptr;
    QLabel *m_pageRotation = nullptr;
    QLabel *m_viewRotation = nullptr;

    QLabel *m_selectionEmpty = nullptr;
    QWidget *m_selectionBody = nullptr;
    QLabel *m_selType = nullptr;
    QLabel *m_selPage = nullptr;
    QLabel *m_selAuthor = nullptr;
    QLabel *m_selModified = nullptr;
    QLabel *m_selGeometry = nullptr;
    QLabel *m_selExtra = nullptr;
    QPlainTextEdit *m_selContents = nullptr;
    QToolButton *m_selColor = nullptr;
    QSlider *m_selOpacity = nullptr;
    QLabel *m_selOpacityValue = nullptr;
    QLabel *m_selBorder = nullptr;
    QPushButton *m_selDelete = nullptr;
    QLabel *m_selStatus = nullptr;
};

} // namespace sweet::ui
