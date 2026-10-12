#include "ui/panels/Sidebar.h"

#include "ui/panels/AttachmentsPanel.h"
#include "ui/panels/CommentsPanel.h"
#include "ui/panels/FormsPanel.h"
#include "ui/panels/LayersPanel.h"
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

constexpr int railWidth = 46;

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
    m_railLayout->setContentsMargins(2, 4, 2, 4);
    m_railLayout->setSpacing(1);
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
    header->setFixedHeight(28);
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
    m_comments = new CommentsPanel(m_stack);
    m_forms = new FormsPanel(m_stack);
    m_stack->addWidget(m_thumbnails);
    m_stack->addWidget(m_outline);
    m_stack->addWidget(m_comments);
    m_stack->addWidget(m_forms);
    connect(m_forms, &FormsPanel::fieldActivated, this, &Sidebar::annotationActivated);
    connect(m_forms, &FormsPanel::statusMessage, this, &Sidebar::statusMessage);
    connect(m_comments, &CommentsPanel::annotationActivated, this,
            &Sidebar::annotationActivated);
    connect(m_thumbnails, &ThumbnailPanel::extractRequested, this, &Sidebar::extractRequested);
    connect(m_thumbnails, &ThumbnailPanel::insertFileRequested, this,
            &Sidebar::insertFileRequested);
    connect(m_thumbnails, &ThumbnailPanel::statusMessage, this, &Sidebar::statusMessage);
    m_attachments = new AttachmentsPanel(m_stack);
    m_layers = new LayersPanel(m_stack);
    m_stack->addWidget(m_attachments);
    m_stack->addWidget(m_layers);
    connect(m_attachments, &AttachmentsPanel::saveRequested, this,
            &Sidebar::saveAttachmentRequested);
    connect(m_attachments, &AttachmentsPanel::statusMessage, this, &Sidebar::statusMessage);

    addRailButton(Panel::Pages, QStringLiteral("pages"), tr("Pages"));
    addRailButton(Panel::Bookmarks, QStringLiteral("bookmark"), tr("Bookmarks"));
    addRailButton(Panel::Comments, QStringLiteral("comment"), tr("Notes"));
    addRailButton(Panel::Forms, QStringLiteral("form"), tr("Forms"));
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

    m_content->setMinimumWidth(160);
    showPanel(Panel::Pages);
}

void Sidebar::addRailButton(Panel panel, const QString &iconName, const QString &label)
{
    auto *button = new QToolButton(this);
    button->setObjectName(QStringLiteral("RailButton"));
    button->setCheckable(true);
    // Acrobat-style nav: icon only; the panel header and tooltip carry the name.
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIcon(icon(iconName));
    button->setIconSize(QSize(22, 22));
    button->setFixedSize(railWidth - 6, 36);
    button->setToolTip(label);
    button->setAccessibleName(label);
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
        { Panel::Comments, tr("Comments") }, { Panel::Forms, tr("Form fields") },
        { Panel::Attachments, tr("Attachments") }, { Panel::Layers, tr("Layers") },
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
    m_comments->setSession(session);
    m_forms->setSession(session);
    m_attachments->setSession(session);
    m_layers->setSession(session);
}

} // namespace sweet::ui
