#pragma once

#include <QList>
#include <QWidget>

class QAction;
class QButtonGroup;
class QHBoxLayout;
class QLabel;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

namespace sweet::ui {

/// A labelled cluster of related commands inside a ribbon page.
class RibbonGroup : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonGroup(const QString &title, QWidget *parent = nullptr);

    /// Large button: icon above the label. Use for primary commands.
    QToolButton *addAction(QAction *action);

    /// Compact button: icon beside the label. Three fit in the height of one
    /// large button, which is how secondary commands are packed in.
    QToolButton *addCompactAction(QAction *action);

    /// Starts a new column of compact buttons.
    void addCompactColumn();

    void addSeparator();
    void addWidget(QWidget *widget);

private:
    QHBoxLayout *m_row = nullptr;
    QVBoxLayout *m_compactColumn = nullptr;
};

/// One ribbon tab: a horizontal strip of groups.
class RibbonPage : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonPage(QWidget *parent = nullptr);

    RibbonGroup *addGroup(const QString &title);
    void addStretch();

private:
    QHBoxLayout *m_row = nullptr;
    bool m_stretched = false;
};

/// Tab strip plus the command area below it.
///
/// A ribbon beats a flat toolbar here because an all-in-one editor has far more
/// commands than one row can hold, and grouping them by task (Home, Edit,
/// Comment, Page, Protect) keeps each screen small enough to scan.
class RibbonBar : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonBar(QWidget *parent = nullptr);

    RibbonPage *addPage(const QString &title);

    /// Widget pinned to the right of the tab strip, for theme and layout
    /// toggles that stay reachable on every tab.
    void addTabStripWidget(QWidget *widget);

    int currentIndex() const;
    void setCurrentIndex(int index);

    /// Hides the command area, leaving only the tab strip.
    void setCollapsed(bool collapsed);
    bool isCollapsed() const noexcept { return m_collapsed; }

Q_SIGNALS:
    void currentIndexChanged(int index);
    void collapsedChanged(bool collapsed);

private:
    QHBoxLayout *m_tabStripLayout = nullptr;
    QStackedWidget *m_pages = nullptr;
    QButtonGroup *m_tabs = nullptr;
    bool m_collapsed = false;
};

} // namespace sweet::ui
