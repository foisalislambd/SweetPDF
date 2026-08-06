#include "ui/RibbonBar.h"

#include "ui/Theme.h"

#include <QAction>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

constexpr int largeIconSize = 22;
constexpr int compactIconSize = 16;
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
        button->setMinimumWidth(largeButtonWidth);
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
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
    column->setContentsMargins(6, 4, 6, 2);
    column->setSpacing(2);

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
    m_row = new QHBoxLayout(this);
    m_row->setContentsMargins(8, 4, 8, 4);
    m_row->setSpacing(0);
}

RibbonGroup *RibbonPage::addGroup(const QString &title)
{
    if (m_row->count() > 0) {
        auto *line = new QFrame(this);
        line->setObjectName(QStringLiteral("RibbonGroupSeparator"));
        line->setFrameShape(QFrame::NoFrame);
        line->setFixedWidth(1);
        m_row->addWidget(line);
    }

    auto *group = new RibbonGroup(title, this);
    m_row->addWidget(group);
    return group;
}

void RibbonPage::addStretch()
{
    if (m_stretched)
        return;
    m_row->addStretch(1);
    m_stretched = true;
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
    m_tabStripLayout = new QHBoxLayout(tabStrip);
    m_tabStripLayout->setContentsMargins(8, 4, 8, 0);
    m_tabStripLayout->setSpacing(2);
    m_tabStripLayout->addStretch(1);
    column->addWidget(tabStrip);

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

RibbonPage *RibbonBar::addPage(const QString &title)
{
    auto *page = new RibbonPage(m_pages);
    page->addStretch();
    const int index = m_pages->addWidget(page);

    auto *tab = new QPushButton(title, this);
    tab->setObjectName(QStringLiteral("RibbonTab"));
    tab->setCheckable(true);
    tab->setCursor(Qt::ArrowCursor);
    tab->setFocusPolicy(Qt::TabFocus);
    m_tabs->addButton(tab, index);
    // Keep the trailing stretch (and any pinned widgets) to the right.
    m_tabStripLayout->insertWidget(index, tab);

    if (index == 0) {
        tab->setChecked(true);
        m_pages->setCurrentIndex(0);
    }
    return page;
}

void RibbonBar::addTabStripWidget(QWidget *widget)
{
    m_tabStripLayout->addWidget(widget);
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
