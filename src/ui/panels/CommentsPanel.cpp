#include "ui/panels/CommentsPanel.h"

#include "core/session/DocumentSession.h"
#include "ui/theme/Theme.h"

#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QStackedLayout>
#include <QTimer>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

constexpr int pageRole = Qt::UserRole + 1;
constexpr int objectRole = Qt::UserRole + 2;

/// A one-line preview: the note text if there is any, otherwise what kind of
/// markup it is, so a bare highlight still reads as something.
QString previewFor(const core::Annotation &annotation)
{
    QString text = annotation.contents.simplified();
    if (text.isEmpty())
        return core::annotations::displayName(annotation.type);
    if (text.size() > 120)
        text = text.left(117) + QStringLiteral("…");
    return text;
}

QString iconNameFor(core::AnnotationType type)
{
    switch (type) {
    case core::AnnotationType::Highlight:
        return QStringLiteral("highlight");
    case core::AnnotationType::Underline:
        return QStringLiteral("underline");
    case core::AnnotationType::StrikeOut:
        return QStringLiteral("strikeout");
    case core::AnnotationType::Squiggly:
        return QStringLiteral("squiggly");
    case core::AnnotationType::Ink:
        return QStringLiteral("draw");
    case core::AnnotationType::FreeText:
        return QStringLiteral("add-text");
    case core::AnnotationType::Square:
        return QStringLiteral("shape");
    case core::AnnotationType::Circle:
        return QStringLiteral("ellipse");
    case core::AnnotationType::Line:
        return QStringLiteral("line");
    case core::AnnotationType::Redact:
        return QStringLiteral("redact");
    case core::AnnotationType::Stamp:
        return QStringLiteral("stamp");
    default:
        return QStringLiteral("comment");
    }
}

} // namespace

CommentsPanel::CommentsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *stack = new QStackedLayout(this);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setStackingMode(QStackedLayout::StackOne);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("CommentList"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setWordWrap(true);
    m_list->setIconSize(QSize(16, 16));
    m_list->setUniformItemSizes(false);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    stack->addWidget(m_list);

    m_emptyState = new QWidget(this);
    auto *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setContentsMargins(22, 32, 22, 22);
    emptyLayout->setSpacing(10);
    emptyLayout->addStretch(1);

    auto *glyph = new QLabel(m_emptyState);
    glyph->setAlignment(Qt::AlignCenter);
    glyph->setPixmap(Theme::instance()
                         .icon(QStringLiteral("comment"), Theme::instance().palette().textMuted)
                         .pixmap(32, 32));
    emptyLayout->addWidget(glyph);

    auto *heading = new QLabel(tr("No comments yet"), m_emptyState);
    heading->setObjectName(QStringLiteral("PanelTitle"));
    heading->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(heading);

    auto *hint = new QLabel(tr("Highlight text, draw, or drop a note on the page and it will "
                               "show up here."),
                            m_emptyState);
    hint->setObjectName(QStringLiteral("SectionLabel"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    emptyLayout->addWidget(hint);
    emptyLayout->addStretch(2);
    stack->addWidget(m_emptyState);

    m_rebuildTimer = new QTimer(this);
    m_rebuildTimer->setSingleShot(true);
    m_rebuildTimer->setInterval(60);
    connect(m_rebuildTimer, &QTimer::timeout, this, &CommentsPanel::rebuild);

    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        Q_EMIT annotationActivated(item->data(pageRole).toInt(), item->data(objectRole).toInt());
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        Q_EMIT annotationActivated(item->data(pageRole).toInt(), item->data(objectRole).toInt());
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint &at) {
        QListWidgetItem *item = m_list->itemAt(at);
        if (!item)
            return;
        m_list->setCurrentItem(item);

        QMenu menu(this);
        menu.addAction(icon(QStringLiteral("comment")), tr("Edit note…"), this,
                       &CommentsPanel::editSelected);
        menu.addAction(icon(QStringLiteral("trash")), tr("Delete"), this,
                       &CommentsPanel::deleteSelected);
        menu.exec(m_list->viewport()->mapToGlobal(at));
    });

    connect(&Theme::instance(), &Theme::changed, this, &CommentsPanel::scheduleRebuild);

    rebuild();
}

void CommentsPanel::setSession(core::DocumentSession *session)
{
    if (m_session)
        m_session->disconnect(this);

    m_session = session;
    if (m_session) {
        connect(m_session, &core::DocumentSession::annotationsChanged, this,
                &CommentsPanel::scheduleRebuild);
        connect(m_session, &core::DocumentSession::documentChanged, this,
                &CommentsPanel::scheduleRebuild);
        connect(m_session, &core::DocumentSession::selectedAnnotationChanged, this,
                [this](int, int objectNumber) {
                    // Follow the canvas selection without emitting our own.
                    for (int row = 0; row < m_list->count(); ++row) {
                        QListWidgetItem *item = m_list->item(row);
                        if (item->data(objectRole).toInt() != objectNumber)
                            continue;
                        const QSignalBlocker blocker(m_list);
                        m_list->setCurrentItem(item);
                        m_list->scrollToItem(item);
                        return;
                    }
                });
    }
    rebuild();
}

void CommentsPanel::scheduleRebuild()
{
    m_rebuildTimer->start();
}

void CommentsPanel::rebuild()
{
    const int previous = m_list->currentItem()
        ? m_list->currentItem()->data(objectRole).toInt()
        : 0;

    m_list->clear();

    QList<core::Annotation> found;
    if (m_session && m_session->isLoaded() && m_session->document()->isPdf())
        found = core::annotations::all(*m_session->document());

    for (const core::Annotation &annotation : found) {
        if (!annotation.editable)
            continue;

        auto *item = new QListWidgetItem(m_list);
        item->setData(pageRole, annotation.pageIndex);
        item->setData(objectRole, annotation.objectNumber);

        const QString author = annotation.author.isEmpty() ? tr("You") : annotation.author;
        item->setText(QStringLiteral("%1\n%2 · %3")
                          .arg(previewFor(annotation))
                          .arg(tr("Page %1").arg(annotation.pageIndex + 1))
                          .arg(author));
        // Tinted with the annotation's own colour, so the list reads like the page.
        const QColor tint = annotation.color.isValid() ? annotation.color
                                                       : Theme::instance().palette().textMuted;
        item->setIcon(Theme::instance().icon(iconNameFor(annotation.type), tint));
        item->setToolTip(annotation.contents.isEmpty()
                             ? core::annotations::displayName(annotation.type)
                             : annotation.contents);
    }

    m_emptyState->setVisible(m_list->count() == 0);
    m_list->setVisible(m_list->count() > 0);

    if (previous != 0) {
        for (int row = 0; row < m_list->count(); ++row) {
            if (m_list->item(row)->data(objectRole).toInt() == previous) {
                const QSignalBlocker blocker(m_list);
                m_list->setCurrentRow(row);
                break;
            }
        }
    }
}

void CommentsPanel::deleteSelected()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item || !m_session)
        return;

    const int pageIndex = item->data(pageRole).toInt();
    const int objectNumber = item->data(objectRole).toInt();
    const QList<core::Annotation> found = m_session->annotationsOnPage(pageIndex);
    for (const core::Annotation &annotation : found) {
        if (annotation.objectNumber == objectNumber) {
            m_session->removeAnnotation(annotation);
            return;
        }
    }
}

void CommentsPanel::editSelected()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item || !m_session)
        return;

    const int pageIndex = item->data(pageRole).toInt();
    const int objectNumber = item->data(objectRole).toInt();
    const QList<core::Annotation> found = m_session->annotationsOnPage(pageIndex);
    for (const core::Annotation &annotation : found) {
        if (annotation.objectNumber != objectNumber)
            continue;

        bool accepted = false;
        const QString text = QInputDialog::getMultiLineText(this, tr("Edit note"), tr("Note text:"),
                                                            annotation.contents, &accepted);
        if (accepted)
            m_session->modifyAnnotation(annotation, annotation.color, annotation.opacity, text);
        return;
    }
}

} // namespace sweet::ui
