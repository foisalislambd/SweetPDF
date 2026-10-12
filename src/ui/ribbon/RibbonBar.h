#pragma once

#include <QList>
#include <QWidget>

class QAction;
class QButtonGroup;
class QHBoxLayout;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

namespace sweet::ui {

/// A labelled cluster of related commands inside a ribbon page.
///
/// The section caption under the tools (File, Markup, …) is what makes the
/// header scannable; individual commands still show their own short names.
class RibbonGroup : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonGroup(const QString &title, QWidget *parent = nullptr);

    /// Primary command: icon above a short caption (Adobe-style tool cell).
    QToolButton *addAction(QAction *action);

    /// Secondary command: icon beside the label, packed in a short column.
    QToolButton *addCompactAction(QAction *action);

    /// Starts a new column of compact buttons.
    void addCompactColumn();

    void addSeparator();
    void addWidget(QWidget *widget);

private:
    QHBoxLayout *m_row = nullptr;
    QVBoxLayout *m_compactColumn = nullptr;
    int m_compactCount = 0;
};

/// One ribbon tab: a horizontal strip of groups.
class RibbonPage : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonPage(QWidget *parent = nullptr);

    RibbonGroup *addGroup(const QString &title);

private:
    int groupCount() const;
    QWidget *makeSeparator();
    void insertBeforeStretch(QWidget *widget);

    QHBoxLayout *m_row = nullptr;
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

    /// Widget pinned to the left of the tabs, before the first one. Used for the
    /// application menu, which replaces a separate menu bar.
    void addTabStripLeadingWidget(QWidget *widget);

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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QHBoxLayout *m_leadingLayout = nullptr;
    QHBoxLayout *m_tabsLayout = nullptr;
    QHBoxLayout *m_trailingLayout = nullptr;
    QStackedWidget *m_pages = nullptr;
    QButtonGroup *m_tabs = nullptr;
    bool m_collapsed = false;
};

} // namespace sweet::ui
