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
#include <QShowEvent>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

constexpr int largeIconSize = 22;
constexpr int compactIconSize = 16;
constexpr int largeCellWidth = 58;
// Three compact rows match one large cell. A taller column stretches every
// ribbon tab, which leaves empty bands above and below the shorter ones.
constexpr int compactRows = 3;

QToolButton *makeButton(QAction *action, bool large)
{
    auto *button = new QToolButton;
    button->setObjectName(QStringLiteral("RibbonAction"));
    button->setDefaultAction(action);
    button->setFocusPolicy(Qt::TabFocus);
    button->setCursor(Qt::ArrowCursor);
    button->setAutoRaise(true);
    // Prefer the short ribbon caption (iconText) when set; menus keep the full text.
    const QString caption =
        action->iconText().isEmpty() || action->iconText() == action->text()
            ? action->text()
            : action->iconText();
    button->setText(caption);
    if (action->toolTip().isEmpty())
        button->setToolTip(action->text());

    if (large) {
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(largeIconSize, largeIconSize));
        const int textWidth = button->fontMetrics().horizontalAdvance(caption);
        button->setMinimumWidth(qMax(largeCellWidth, textWidth + 8));
        button->setFixedHeight(52);
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    } else {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(compactIconSize, compactIconSize));
        button->setFixedHeight(26);
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }

    if (action->menu())
        button->setPopupMode(QToolButton::MenuButtonPopup);
    return button;
}

QFrame *makeGroupSeparator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("RibbonGroupSeparator"));
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedWidth(1);
    line->setMinimumHeight(44);
    line->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    return line;
}

} // namespace

RibbonGroup::RibbonGroup(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    if (!title.isEmpty())
        setAccessibleName(title);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(4, 1, 4, 0);
    column->setSpacing(0);

    auto *content = new QWidget(this);
    m_row = new QHBoxLayout(content);
    m_row->setContentsMargins(0, 0, 0, 0);
    m_row->setSpacing(2);
    m_row->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    column->addWidget(content, 1);

    // Small section caption under the tools — the main cue for what each cluster does.
    auto *label = new QLabel(title, this);
    label->setObjectName(QStringLiteral("RibbonGroupTitle"));
    label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    label->setVisible(!title.isEmpty());
    column->addWidget(label);
}

QToolButton *RibbonGroup::addAction(QAction *action)
{
    m_compactColumn = nullptr;
    m_compactCount = 0;
    QToolButton *button = makeButton(action, true);
    m_row->addWidget(button, 0, Qt::AlignVCenter);
    return button;
}

void RibbonGroup::addCompactColumn()
{
    m_compactColumn = new QVBoxLayout;
    m_compactColumn->setContentsMargins(0, 0, 0, 0);
    m_compactColumn->setSpacing(0);
    m_compactCount = 0;
    m_row->addLayout(m_compactColumn);
}

QToolButton *RibbonGroup::addCompactAction(QAction *action)
{
    if (!m_compactColumn || m_compactCount >= compactRows)
        addCompactColumn();
    ++m_compactCount;

    QToolButton *button = makeButton(action, false);
    m_compactColumn->addWidget(button);
    return button;
}

void RibbonGroup::addSeparator()
{
    m_compactColumn = nullptr;
    m_compactCount = 0;
    m_row->addSpacing(4);
    m_row->addWidget(makeGroupSeparator(this));
    m_row->addSpacing(4);
}

void RibbonGroup::addWidget(QWidget *widget)
{
    m_compactColumn = nullptr;
    m_compactCount = 0;
    m_row->addWidget(widget, 0, Qt::AlignVCenter);
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
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    outer->addWidget(scroll);

    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("RibbonPageContent"));
    content->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_row = new QHBoxLayout(content);
    m_row->setContentsMargins(6, 1, 6, 0);
    m_row->setSpacing(0);
    m_row->setAlignment(Qt::AlignVCenter);

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
    return makeGroupSeparator(this);
}

void RibbonPage::insertBeforeStretch(QWidget *widget)
{
    m_row->insertWidget(m_row->count() - 1, widget, 0, Qt::AlignVCenter);
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
    stripLayout->setContentsMargins(8, 0, 8, 0);
    stripLayout->setSpacing(2);
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
    m_pages->setObjectName(QStringLiteral("RibbonPages"));
    m_pages->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    column->addWidget(m_pages);
    connect(&Theme::instance(), &Theme::changed, this, [this] { fitToCurrentPage(); });

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
    tab->setFlat(true);
    tab->installEventFilter(this);
    m_tabs->addButton(tab, index);
    m_tabsLayout->addWidget(tab);

    if (index == 0) {
        tab->setChecked(true);
        m_pages->setCurrentIndex(0);
    }
    fitToCurrentPage();
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
    fitToCurrentPage();
    Q_EMIT currentIndexChanged(index);
}

void RibbonBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    fitToCurrentPage();
}

void RibbonBar::fitToCurrentPage()
{
    QWidget *page = m_pages->currentWidget();
    if (!page || m_collapsed)
        return;
    page->ensurePolished();
    const int height = page->sizeHint().height();
    if (height > 0 && m_pages->height() != height)
        m_pages->setFixedHeight(height);
}

void RibbonBar::setCollapsed(bool collapsed)
{
    if (collapsed == m_collapsed)
        return;
    m_collapsed = collapsed;
    m_pages->setVisible(!collapsed);
    if (!collapsed)
        fitToCurrentPage();
    Q_EMIT collapsedChanged(collapsed);
}

} // namespace sweet::ui
