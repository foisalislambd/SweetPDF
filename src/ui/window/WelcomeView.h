#pragma once

#include <QStringList>
#include <QWidget>

class QBoxLayout;
class QGridLayout;
class QLabel;
class QLineEdit;
class QResizeEvent;
class QVBoxLayout;

namespace sweet::ui {

/// The home screen shown when no document is open.
///
/// Modelled on what desktop PDF suites put on their start page, because that is
/// what someone arriving here expects: pick a task, or pick up a file you had
/// open before. It is four blocks - a header with the primary action, a grid of
/// task tiles, the recent-files list (which doubles as the drop target), and a
/// short guide so a first-time user is never left guessing.
///
/// MainWindow hides the ribbon and the side panels while this is showing: none of
/// that chrome does anything without a document, and hiding it leaves the home
/// screen the whole window.
class WelcomeView : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeView(QWidget *parent = nullptr);

    /// Newest first. Entries that no longer exist on disk are skipped.
    void setRecentFiles(const QStringList &paths);

Q_SIGNALS:
    /// The user wants the file dialog.
    void openRequested();

    /// The user picked a specific document.
    void fileRequested(const QString &path);

    /// One entry should leave the recent list.
    void forgetRequested(const QString &path);

    /// The whole recent list should go.
    void clearRecentRequested();

    /// A task tile was pressed. The id is either an action id owned by
    /// MainWindow or one of the composite tasks it handles itself.
    void toolRequested(const QString &toolId);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildHeader(QVBoxLayout *column);
    void buildTiles(QVBoxLayout *column);
    QWidget *buildRecentSection();
    QWidget *buildGuideSection();
    void rebuildRecentRows();
    void relayout();
    void placeTiles(int columns);

    /// Re-applies palette colours after a theme change. Labels are coloured
    /// individually, which the global stylesheet cannot reach.
    void retint();

    QWidget *m_column = nullptr;
    QBoxLayout *m_lower = nullptr;
    QWidget *m_guide = nullptr;
    QGridLayout *m_tileGrid = nullptr;
    QLineEdit *m_filter = nullptr;
    QWidget *m_recentRows = nullptr;
    QVBoxLayout *m_recentLayout = nullptr;
    QWidget *m_dropZone = nullptr;
    QLabel *m_recentSummary = nullptr;
    QStringList m_recentFiles;
    int m_tileColumns = 4;
};

} // namespace sweet::ui
