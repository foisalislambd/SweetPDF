#include "ui/OutlinePanel.h"

#include "core/DocumentSession.h"
#include "core/PdfDocument.h"
#include "ui/Theme.h"

#include <QLabel>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

constexpr int pageIndexRole = Qt::UserRole + 1;

QWidget *makeEmptyState(const QString &title, const QString &hint, QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 40, 24, 24);
    layout->setSpacing(6);
    layout->addStretch(1);

    auto *heading = new QLabel(title, page);
    heading->setAlignment(Qt::AlignCenter);
    heading->setObjectName(QStringLiteral("PanelTitle"));
    layout->addWidget(heading);

    auto *body = new QLabel(hint, page);
    body->setAlignment(Qt::AlignCenter);
    body->setWordWrap(true);
    body->setObjectName(QStringLiteral("SectionLabel"));
    layout->addWidget(body);

    layout->addStretch(2);
    return page;
}

} // namespace

OutlinePanel::OutlinePanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *stack = new QStackedWidget(this);
    layout->addWidget(stack);

    m_tree = new QTreeWidget(stack);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(14);
    m_tree->setAnimated(true);
    m_tree->setExpandsOnDoubleClick(false);
    m_tree->setUniformRowHeights(true);
    stack->addWidget(m_tree);

    m_emptyState = makeEmptyState(tr("No bookmarks"),
                                  tr("This document has no outline. Bookmarks you add will "
                                     "appear here."),
                                  stack);
    stack->addWidget(m_emptyState);
    stack->setCurrentWidget(m_emptyState);

    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int) {
        if (!m_session)
            return;
        const int pageIndex = item->data(0, pageIndexRole).toInt();
        if (pageIndex >= 0)
            m_session->setCurrentPage(pageIndex);
    });
}

void OutlinePanel::setSession(core::DocumentSession *session)
{
    if (m_session == session)
        return;

    if (m_session)
        m_session->disconnect(this);

    m_session = session;
    if (m_session) {
        connect(m_session, &core::DocumentSession::documentChanged, this,
                &OutlinePanel::rebuild);
    }
    rebuild();
}

void OutlinePanel::appendNodes(QTreeWidgetItem *parent, const QList<core::OutlineNode> &nodes)
{
    for (const core::OutlineNode &node : nodes) {
        auto *item = new QTreeWidgetItem;
        item->setText(0, node.title.isEmpty() ? tr("Untitled") : node.title);
        item->setData(0, pageIndexRole, node.pageIndex);
        if (node.pageIndex >= 0)
            item->setToolTip(0, tr("Page %1").arg(node.pageIndex + 1));

        if (parent)
            parent->addChild(item);
        else
            m_tree->addTopLevelItem(item);

        appendNodes(item, node.children);
        item->setExpanded(node.openByDefault);
    }
}

void OutlinePanel::rebuild()
{
    m_tree->clear();

    auto *stack = qobject_cast<QStackedWidget *>(m_tree->parentWidget());
    const bool hasOutline = m_session && m_session->document()
        && !m_session->document()->outline().isEmpty();

    if (hasOutline)
        appendNodes(nullptr, m_session->document()->outline());

    if (stack)
        stack->setCurrentWidget(hasOutline ? static_cast<QWidget *>(m_tree) : m_emptyState);
}

} // namespace sweet::ui
