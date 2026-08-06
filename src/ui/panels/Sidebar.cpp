#include "ui/panels/Sidebar.h"

#include "ui/panels/OutlinePanel.h"
#include "ui/panels/ThumbnailPanel.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

constexpr int railWidth = 68;
constexpr int defaultPanelWidth = 208;

QWidget *makePlaceholder(const QString &iconName, const QString &title, const QString &hint,
                         QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 32, 22, 22);
    layout->setSpacing(10);
    layout->addStretch(1);

    auto *glyph = new QLabel(page);
    glyph->setAlignment(Qt::AlignCenter);
    glyph->setPixmap(Theme::instance()
                         .icon(iconName, Theme::instance().palette().textMuted)
                         .pixmap(32, 32));
    layout->addWidget(glyph);

    auto *heading = new QLabel(title, page);
    heading->setObjectName(QStringLiteral("PanelTitle"));
    heading->setAlignment(Qt::AlignCenter);
    layout->addWidget(heading);

    auto *body = new QLabel(hint, page);
    body->setObjectName(QStringLiteral("SectionLabel"));
    body->setAlignment(Qt::AlignCenter);
    body->setWordWrap(true);
    layout->addWidget(body);

    layout->addStretch(2);
    return page;
}

} // namespace

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("Sidebar"));

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    auto *rail = new QWidget(this);
    rail->setObjectName(QStringLiteral("SidebarRail"));
    rail->setFixedWidth(railWidth);
    m_railLayout = new QVBoxLayout(rail);
    m_railLayout->setContentsMargins(6, 8, 6, 8);
    m_railLayout->setSpacing(4);
    m_railLayout->addStretch(1);
    row->addWidget(rail);

    m_rail = new QButtonGroup(this);
    m_rail->setExclusive(true);

    m_content = new QWidget(this);
    m_content->setMinimumWidth(160);
    auto *contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    row->addWidget(m_content, 1);

    auto *header = new QWidget(m_content);
    header->setObjectName(QStringLiteral("PanelHeader"));
    header->setFixedHeight(34);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 6, 0);
    headerLayout->setSpacing(4);

    m_title = new QLabel(tr("Pages"), header);
    m_title->setObjectName(QStringLiteral("PanelTitle"));
    headerLayout->addWidget(m_title, 1);

    auto *collapse = new QToolButton(header);
    collapse->setIcon(icon(QStringLiteral("chevron-left")));
    collapse->setIconSize(QSize(16, 16));
    collapse->setToolTip(tr("Hide panel"));
    connect(collapse, &QToolButton::clicked, this, [this] { setPanelVisible(false); });
    headerLayout->addWidget(collapse);
    contentLayout->addWidget(header);

    m_stack = new QStackedWidget(m_content);
    contentLayout->addWidget(m_stack, 1);

    m_thumbnails = new ThumbnailPanel(m_stack);
    m_outline = new OutlinePanel(m_stack);
    m_stack->addWidget(m_thumbnails);
    m_stack->addWidget(m_outline);
    m_stack->addWidget(makePlaceholder(QStringLiteral("comment"), tr("Comments"),
                                       tr("Comments and replies will be listed here once you "
                                          "annotate the document."),
                                       m_stack));
    m_stack->addWidget(makePlaceholder(QStringLiteral("attachment"), tr("Attachments"),
                                       tr("Files embedded in the PDF appear here."), m_stack));
    m_stack->addWidget(makePlaceholder(QStringLiteral("layers"), tr("Layers"),
                                       tr("Optional content groups appear here."), m_stack));

    addRailButton(Panel::Pages, QStringLiteral("pages"), tr("Pages"));
    addRailButton(Panel::Bookmarks, QStringLiteral("bookmark"), tr("Marks"));
    addRailButton(Panel::Comments, QStringLiteral("comment"), tr("Notes"));
    addRailButton(Panel::Attachments, QStringLiteral("attachment"), tr("Files"));
    addRailButton(Panel::Layers, QStringLiteral("layers"), tr("Layers"));

    connect(m_rail, &QButtonGroup::idClicked, this, [this](int id) {
        const auto panel = static_cast<Panel>(id);
        if (panel == m_currentPanel && isPanelVisible()) {
            setPanelVisible(false);
            return;
        }
        showPanel(panel);
        setPanelVisible(true);
    });

    connect(&Theme::instance(), &Theme::changed, this, [this, collapse] {
        collapse->setIcon(icon(QStringLiteral("chevron-left")));
    });

    m_content->setMinimumWidth(defaultPanelWidth);
    showPanel(Panel::Pages);
}

void Sidebar::addRailButton(Panel panel, const QString &iconName, const QString &label)
{
    auto *button = new QToolButton(this);
    button->setCheckable(true);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIcon(icon(iconName));
    button->setText(label);
    button->setIconSize(QSize(20, 20));
    button->setFixedSize(railWidth - 12, 52);
    button->setToolTip(label);
    button->setFocusPolicy(Qt::TabFocus);

    connect(&Theme::instance(), &Theme::changed, button,
            [button, iconName] { button->setIcon(icon(iconName)); });

    m_rail->addButton(button, static_cast<int>(panel));
    m_railLayout->insertWidget(m_railLayout->count() - 1, button);
}

bool Sidebar::isPanelVisible() const
{
    return m_content->isVisible();
}

void Sidebar::showPanel(Panel panel)
{
    m_currentPanel = panel;
    m_stack->setCurrentIndex(static_cast<int>(panel));

    if (auto *button = m_rail->button(static_cast<int>(panel)))
        button->setChecked(true);

    const QHash<Panel, QString> titles = {
        { Panel::Pages, tr("Pages") },       { Panel::Bookmarks, tr("Bookmarks") },
        { Panel::Comments, tr("Comments") }, { Panel::Attachments, tr("Attachments") },
        { Panel::Layers, tr("Layers") },
    };
    m_title->setText(titles.value(panel));
}

void Sidebar::setPanelVisible(bool visible)
{
    if (visible == m_content->isVisible())
        return;

    m_content->setVisible(visible);
    if (!visible) {
        if (auto *button = m_rail->checkedButton()) {
            m_rail->setExclusive(false);
            button->setChecked(false);
            m_rail->setExclusive(true);
        }
    } else if (auto *button = m_rail->button(static_cast<int>(m_currentPanel))) {
        button->setChecked(true);
    }

    Q_EMIT panelVisibilityChanged(visible);
}

void Sidebar::togglePanel()
{
    setPanelVisible(!isPanelVisible());
}

void Sidebar::setSession(core::DocumentSession *session)
{
    m_thumbnails->setSession(session);
    m_outline->setSession(session);
}

} // namespace sweet::ui
