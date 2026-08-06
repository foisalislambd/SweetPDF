#include "ui/ribbon/RibbonBar.h"

#include "ui/theme/Theme.h"

#include <QAction>
#include <QButtonGroup>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

constexpr int largeIconSize = 18;
constexpr int compactIconSize = 14;
constexpr int largeButtonWidth = 62;

QToolButton *makeButton(QAction *action, bool large)
{
    auto *button = new QToolButton;
    button->setObjectName(QStringLiteral("RibbonAction"));
    button->setDefaultAction(action);
    button->setFocusPolicy(Qt::TabFocus);
    button->setCursor(Qt::ArrowCursor);

    if (large) {
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(largeIconSize, largeIconSize));
        // Wide enough for the label: a truncated command name is worse than a
        // ribbon that grows by a few pixels.
        const int textWidth = button->fontMetrics().horizontalAdvance(action->text());
        button->setMinimumWidth(qMax(largeButtonWidth, textWidth + 18));
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    } else {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(compactIconSize, compactIconSize));
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }

    if (action->menu())
        button->setPopupMode(QToolButton::MenuButtonPopup);
    return button;
}

} // namespace

RibbonGroup::RibbonGroup(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(4, 2, 4, 1);
    column->setSpacing(1);

    auto *content = new QWidget(this);
    m_row = new QHBoxLayout(content);
    m_row->setContentsMargins(0, 0, 0, 0);
    m_row->setSpacing(2);
    column->addWidget(content, 1);

    auto *label = new QLabel(title, this);
    label->setObjectName(QStringLiteral("RibbonGroupTitle"));
    label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    column->addWidget(label);
}

QToolButton *RibbonGroup::addAction(QAction *action)
{
    m_compactColumn = nullptr;
    QToolButton *button = makeButton(action, true);
    m_row->addWidget(button);
    return button;
}

void RibbonGroup::addCompactColumn()
{
    m_compactColumn = new QVBoxLayout;
    m_compactColumn->setContentsMargins(0, 0, 0, 0);
    m_compactColumn->setSpacing(2);
    m_compactColumn->addStretch(1);
    m_row->addLayout(m_compactColumn);
}

QToolButton *RibbonGroup::addCompactAction(QAction *action)
{
    if (!m_compactColumn)
        addCompactColumn();

    QToolButton *button = makeButton(action, false);
    m_compactColumn->insertWidget(m_compactColumn->count() - 1, button);
    return button;
}

void RibbonGroup::addSeparator()
{
    m_compactColumn = nullptr;
    auto *line = new QFrame(this);
    line->setObjectName(QStringLiteral("RibbonGroupSeparator"));
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedWidth(1);
    m_row->addSpacing(4);
    m_row->addWidget(line);
    m_row->addSpacing(4);
}

void RibbonGroup::addWidget(QWidget *widget)
{
    m_compactColumn = nullptr;
    m_row->addWidget(widget);
}

RibbonPage::RibbonPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("RibbonPage"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Wide Home tabs outgrow a compact window; scrolling keeps every group
    // reachable instead of clipping the right-hand ones.
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("RibbonPageScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFocusPolicy(Qt::NoFocus);
    outer->addWidget(scroll);

    auto *content = new QWidget(scroll);
    content->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    m_row = new QHBoxLayout(content);
    m_row->setContentsMargins(6, 2, 6, 2);
    m_row->setSpacing(0);

    // The trailing stretch is created up front and everything is inserted before
    // it, so groups always start at the left edge however many there are.
    m_row->addStretch(1);
    scroll->setWidget(content);
}

RibbonGroup *RibbonPage::addGroup(const QString &title)
{
    if (groupCount() > 0)
        insertBeforeStretch(makeSeparator());

    auto *group = new RibbonGroup(title, this);
    insertBeforeStretch(group);
    return group;
}

int RibbonPage::groupCount() const
{
    // Everything except the trailing stretch.
    return m_row->count() - 1;
}

QWidget *RibbonPage::makeSeparator()
{
    auto *line = new QFrame(this);
    line->setObjectName(QStringLiteral("RibbonGroupSeparator"));
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedWidth(1);
    return line;
}

void RibbonPage::insertBeforeStretch(QWidget *widget)
{
    m_row->insertWidget(m_row->count() - 1, widget);
}

RibbonBar::RibbonBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("RibbonBar"));

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    auto *tabStrip = new QWidget(this);
    tabStrip->setObjectName(QStringLiteral("RibbonTabStrip"));
    auto *stripLayout = new QHBoxLayout(tabStrip);
    stripLayout->setContentsMargins(8, 3, 8, 0);
    stripLayout->setSpacing(4);
    column->addWidget(tabStrip);

    // Three slots: the application menu on the left, the tabs, and the
    // always-available toggles on the right.
    const auto addSlot = [stripLayout](int stretch) {
        auto *host = new QWidget;
        auto *layout = new QHBoxLayout(host);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        stripLayout->addWidget(host, stretch);
        return layout;
    };

    m_leadingLayout = addSlot(0);
    m_tabsLayout = addSlot(0);
    stripLayout->addStretch(1);
    m_trailingLayout = addSlot(0);

    m_pages = new QStackedWidget(this);
    column->addWidget(m_pages);

    m_tabs = new QButtonGroup(this);
    m_tabs->setExclusive(true);
    connect(m_tabs, &QButtonGroup::idClicked, this, [this](int index) {
        if (m_collapsed)
            setCollapsed(false);
        setCurrentIndex(index);
    });
}

bool RibbonBar::eventFilter(QObject *watched, QEvent *event)
{
    // Double-clicking a tab collapses or expands the command area, the same
    // gesture Office uses, independent of the window's auto layout mode.
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (qobject_cast<QPushButton *>(watched)) {
            setCollapsed(!m_collapsed);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

RibbonPage *RibbonBar::addPage(const QString &title)
{
    auto *page = new RibbonPage(m_pages);
    const int index = m_pages->addWidget(page);

    auto *tab = new QPushButton(title, this);
    tab->setObjectName(QStringLiteral("RibbonTab"));
    tab->setCheckable(true);
    tab->setCursor(Qt::ArrowCursor);
    tab->setFocusPolicy(Qt::TabFocus);
    tab->installEventFilter(this);
    m_tabs->addButton(tab, index);
    m_tabsLayout->addWidget(tab);

    if (index == 0) {
        tab->setChecked(true);
        m_pages->setCurrentIndex(0);
    }
    return page;
}

void RibbonBar::addTabStripLeadingWidget(QWidget *widget)
{
    m_leadingLayout->addWidget(widget);
}

void RibbonBar::addTabStripWidget(QWidget *widget)
{
    m_trailingLayout->addWidget(widget);
}

int RibbonBar::currentIndex() const
{
    return m_pages->currentIndex();
}

void RibbonBar::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_pages->count() || index == m_pages->currentIndex())
        return;

    m_pages->setCurrentIndex(index);
    if (auto *button = m_tabs->button(index))
        button->setChecked(true);
    Q_EMIT currentIndexChanged(index);
}

void RibbonBar::setCollapsed(bool collapsed)
{
    if (collapsed == m_collapsed)
        return;
    m_collapsed = collapsed;
    m_pages->setVisible(!collapsed);
    Q_EMIT collapsedChanged(collapsed);
}

} // namespace sweet::ui
