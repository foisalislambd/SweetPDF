#pragma once

#include <QStringList>
#include <QWidget>

class QVBoxLayout;

namespace sweet::ui {

/// What the window shows when no document is open.
///
/// It doubles as the drop target and the recent-files list, so the first screen
/// is useful rather than an empty grey area.
class WelcomeView : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeView(QWidget *parent = nullptr);

    void setRecentFiles(const QStringList &paths);

Q_SIGNALS:
    void openRequested();
    void fileRequested(const QString &path);
    void clearRecentRequested();

private:
    void rebuildRecentCards();

    QWidget *m_recentSection = nullptr;
    QVBoxLayout *m_recentLayout = nullptr;
    QStringList m_recentFiles;
};

} // namespace sweet::ui
