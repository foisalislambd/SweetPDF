#include "ui/panels/OutlinePanel.h"

#include "core/pdf/PdfDocument.h"
#include "core/session/DocumentSession.h"
#include "ui/theme/Theme.h"

#include <QAction>
#include <QFont>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

constexpr int pageIndexRole = Qt::UserRole + 1;
constexpr int pathRole = Qt::UserRole + 2;

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
                                  tr("Bookmark the page you are reading. Entries you add are "
                                     "saved with the PDF."),
                                  stack);
    stack->addWidget(m_emptyState);
    stack->setCurrentWidget(m_emptyState);

    m_addButton = new QPushButton(tr("Bookmark this page"), this);
    m_addButton->setIcon(icon(QStringLiteral("bookmark")));
    layout->addWidget(m_addButton);
    connect(m_addButton, &QPushButton::clicked, this, &OutlinePanel::addBookmark);

    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &at) {
        QTreeWidgetItem *item = m_tree->itemAt(at);
        if (!item || !m_session || !m_session->isLoaded() || !m_session->document()->isPdf())
            return;
        // Right-click does not change the current row. Use the row under the pointer.
        m_tree->setCurrentItem(item);
        QMenu menu(this);
        QAction *child = menu.addAction(tr("Add sub-bookmark"));
        QAction *rename = menu.addAction(tr("Rename"));
        QAction *remove = menu.addAction(tr("Delete"));
        QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(at));
        if (chosen == child)
            addChildBookmark();
        else if (chosen == rename)
            renameBookmark();
        else if (chosen == remove)
            removeBookmark();
    });

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
        connect(m_session, &core::DocumentSession::outlineChanged, this, &OutlinePanel::rebuild);
    }
    rebuild();
}

QList<int> OutlinePanel::pathOf(QTreeWidgetItem *item) const
{
    QList<int> path;
    if (!item)
        return path;
    const QVariantList stored = item->data(0, pathRole).toList();
    for (const QVariant &entry : stored)
        path.append(entry.toInt());
    return path;
}

void OutlinePanel::addBookmark()
{
    if (!m_session || !m_session->isLoaded() || !m_session->document()->isPdf())
        return;
    const int page = m_session->currentPage();
    bool accepted = false;
    const QString title = QInputDialog::getText(
        this, tr("Add bookmark"), tr("Name for page %1:").arg(page + 1), QLineEdit::Normal,
        tr("Page %1").arg(page + 1), &accepted);
    if (!accepted || title.trimmed().isEmpty())
        return;
    if (!m_session->addBookmark(title.trimmed(), page))
        return;
}

void OutlinePanel::addChildBookmark()
{
    if (!m_session || !m_session->isLoaded() || !m_session->document()->isPdf())
        return;
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item)
        return;
    const int page = m_session->currentPage();
    bool accepted = false;
    const QString title = QInputDialog::getText(
        this, tr("Add sub-bookmark"), tr("Name for page %1:").arg(page + 1), QLineEdit::Normal,
        tr("Page %1").arg(page + 1), &accepted);
    if (!accepted || title.trimmed().isEmpty())
        return;
    m_session->addChildBookmark(pathOf(item), title.trimmed(), page);
}

void OutlinePanel::renameBookmark()
{
    if (!m_session || !m_session->isLoaded() || !m_session->document()->isPdf())
        return;
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item)
        return;
    bool accepted = false;
    const QString title = QInputDialog::getText(this, tr("Rename bookmark"), tr("Name:"),
                                                QLineEdit::Normal, item->text(0), &accepted);
    if (!accepted || title.trimmed().isEmpty())
        return;
    m_session->renameBookmark(pathOf(item), title.trimmed());
}

void OutlinePanel::removeBookmark()
{
    if (!m_session || !m_session->isLoaded() || !m_session->document()->isPdf())
        return;
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item)
        return;
    m_session->removeBookmark(pathOf(item));
}

void OutlinePanel::appendNodes(QTreeWidgetItem *parent, const QList<core::OutlineNode> &nodes,
                               const QList<int> &prefix)
{
    for (int index = 0; index < nodes.size(); ++index) {
        const core::OutlineNode &node = nodes.at(index);
        auto *item = new QTreeWidgetItem;
        item->setText(0, node.title.isEmpty() ? tr("Untitled") : node.title);
        item->setData(0, pageIndexRole, node.pageIndex);
        QVariantList path;
        for (int step : prefix)
            path.append(step);
        path.append(index);
        item->setData(0, pathRole, path);

        QFont font = item->font(0);
        font.setBold(node.bold);
        font.setItalic(node.italic);
        item->setFont(0, font);
        if (node.color.isValid())
            item->setForeground(0, node.color);

        if (node.pageIndex >= 0)
            item->setToolTip(0, tr("Page %1").arg(node.pageIndex + 1));

        if (parent)
            parent->addChild(item);
        else
            m_tree->addTopLevelItem(item);

        appendNodes(item, node.children, pathOf(item));
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
        appendNodes(nullptr, m_session->document()->outline(), {});

    if (stack)
        stack->setCurrentWidget(hasOutline ? static_cast<QWidget *>(m_tree) : m_emptyState);

    const bool pdf = m_session && m_session->isLoaded() && m_session->document()->isPdf();
    m_addButton->setEnabled(pdf);
}

} // namespace sweet::ui
