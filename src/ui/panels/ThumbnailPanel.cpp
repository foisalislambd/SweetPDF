#include "ui/panels/ThumbnailPanel.h"

#include "core/pdf/PdfDocument.h"
#include "core/session/DocumentSession.h"
#include "ui/theme/Theme.h"

#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>

namespace sweet::ui {
namespace {

constexpr int thumbnailWidth = 124;
constexpr int labelHeight = 18;
constexpr int itemPadding = 7;
constexpr qreal pageRadius = 3.0;

constexpr int pageSizeRole = Qt::UserRole + 1;
constexpr int thumbnailRole = Qt::UserRole + 2;

/// Height a page of `pageSize` gets when drawn `thumbnailWidth` wide.
int scaledHeight(const QSizeF &pageSize)
{
    if (pageSize.width() <= 0.0)
        return thumbnailWidth;
    return qMax(24, qRound(thumbnailWidth * pageSize.height() / pageSize.width()));
}

/// Paints each entry as a small page: shadow, rounded bitmap, page number, and
/// an accent ring on the current page.
class ThumbnailDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const override
    {
        const auto pageSize = index.data(pageSizeRole).toSizeF();
        return QSize(thumbnailWidth + itemPadding * 2,
                     scaledHeight(pageSize) + labelHeight + itemPadding * 2);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const Palette &colors = themePalette();
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRectF row = QRectF(option.rect).adjusted(2, 1, -2, -1);
        if (selected || hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? colors.accentSoft : colors.hover);
            painter->drawRoundedRect(row, 9, 9);
        }

        const auto pageSize = index.data(pageSizeRole).toSizeF();
        const int height = scaledHeight(pageSize);
        const QRectF page(row.center().x() - thumbnailWidth / 2.0,
                          row.top() + itemPadding, thumbnailWidth, height);

        painter->setPen(Qt::NoPen);
        for (int layer = 3; layer >= 1; --layer) {
            QColor tint = colors.shadow;
            tint.setAlpha(qMax(5, colors.shadow.alpha() / (layer + 3)));
            painter->setBrush(tint);
            const qreal grow = layer * 1.2;
            painter->drawRoundedRect(page.adjusted(-grow, -grow + 1, grow, grow + 1.5),
                                     pageRadius + grow, pageRadius + grow);
        }

        QPainterPath clip;
        clip.addRoundedRect(page, pageRadius, pageRadius);
        painter->setClipPath(clip);
        painter->setBrush(Qt::white);
        painter->drawRect(page);

        const auto preview = index.data(thumbnailRole).value<QPixmap>();
        if (!preview.isNull()) {
            painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter->drawPixmap(page, preview, QRectF(preview.rect()));
        }
        painter->setClipping(false);

        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(selected ? colors.accent : colors.border, selected ? 1.6 : 1.0));
        painter->drawRoundedRect(page, pageRadius, pageRadius);

        painter->setFont(themeFont(TextRole::Caption));
        painter->setPen(selected ? colors.accent : colors.textMuted);
        const QRectF label(row.left(), page.bottom() + 1, row.width(), labelHeight);
        painter->drawText(label, Qt::AlignCenter, index.data(Qt::DisplayRole).toString());

        painter->restore();
    }
};

} // namespace

ThumbnailPanel::ThumbnailPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_list = new QListWidget(this);
    m_list->setItemDelegate(new ThumbnailDelegate(m_list));
    m_list->setViewMode(QListView::ListMode);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(0);
    m_list->setUniformItemSizes(false);
    // Extended selection so page operations can act on a range, and internal
    // move so pages can be reordered by dragging them.
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setMouseTracking(true);
    m_list->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_list);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_syncing || !m_session || row < 0)
            return;
        m_session->setCurrentPage(row);
    });
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &ThumbnailPanel::requestVisibleThumbnails);
    connect(&Theme::instance(), &Theme::changed, this, [this] { m_list->viewport()->update(); });
    connect(m_list, &QListWidget::customContextMenuRequested, this,
            &ThumbnailPanel::showContextMenu);
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this,
            [this](const QModelIndex &, int start, int end, const QModelIndex &, int destination) {
                applyReorder(start, end, destination);
            });
}

QList<int> ThumbnailPanel::selectedPages() const
{
    QList<int> pages;
    const QList<QListWidgetItem *> selected = m_list->selectedItems();
    for (const QListWidgetItem *item : selected)
        pages.append(m_list->row(item));

    if (pages.isEmpty() && m_session)
        pages.append(m_session->currentPage());

    std::sort(pages.begin(), pages.end());
    return pages;
}

void ThumbnailPanel::applyReorder(int first, int last, int destination)
{
    if (m_reordering || m_syncing || !m_session || !m_session->isLoaded())
        return;

    const int pages = m_session->pageCount();
    if (pages <= 1 || first < 0 || last < first)
        return;

    // The view has already moved the rows; translate that into the page order the
    // document should have and let the session apply it as one undoable edit.
    QList<int> order;
    order.reserve(pages);
    for (int index = 0; index < pages; ++index)
        order.append(index);

    QList<int> block = order.mid(first, last - first + 1);
    order.remove(first, block.size());
    const int insertAt = qBound(0, destination > last ? destination - int(block.size()) : destination,
                                int(order.size()));
    for (int offset = 0; offset < block.size(); ++offset)
        order.insert(insertAt + offset, block.at(offset));

    m_reordering = true;
    QString error;
    const bool moved = m_session->applyStructuralEdit(
        tr("Reorder pages"),
        [&order](core::PdfDocument &document) { document.reorderPages(order); }, &error);
    m_reordering = false;

    if (!moved) {
        Q_EMIT statusMessage(error);
        rebuild(); // Put the rows back where the document says they are.
        return;
    }

    m_session->setCurrentPage(insertAt);
    Q_EMIT statusMessage(tr("Moved %n page(s).", nullptr, int(block.size())));
}

void ThumbnailPanel::showContextMenu(const QPoint &at)
{
    if (!m_session || !m_session->isLoaded())
        return;

    const QListWidgetItem *underCursor = m_list->itemAt(at);
    if (underCursor && !underCursor->isSelected())
        m_list->setCurrentRow(m_list->row(underCursor));

    const QList<int> pages = selectedPages();
    if (pages.isEmpty())
        return;

    const int anchor = pages.first();
    const int count = int(pages.size());

    QMenu menu(this);
    menu.addAction(icon(QStringLiteral("rotate-left")), tr("Rotate left"), this,
                   [this] { rotateSelection(-90); });
    menu.addAction(icon(QStringLiteral("rotate-right")), tr("Rotate right"), this,
                   [this] { rotateSelection(90); });
    menu.addSeparator();
    menu.addAction(icon(QStringLiteral("page-insert")), tr("Insert blank page before"), this,
                   [this, anchor] { insertBlank(anchor); });
    menu.addAction(icon(QStringLiteral("page-insert")), tr("Insert blank page after"), this,
                   [this, pages] { insertBlank(pages.last() + 1); });
    menu.addAction(icon(QStringLiteral("open")), tr("Insert pages from file…"), this,
                   [this, anchor] { Q_EMIT insertFileRequested(anchor); });
    menu.addSeparator();
    menu.addAction(icon(QStringLiteral("duplicate")), tr("Duplicate %n page(s)", nullptr, count),
                   this, &ThumbnailPanel::duplicateSelection);
    menu.addAction(icon(QStringLiteral("page-extract")),
                   tr("Extract %n page(s) to a new file…", nullptr, count), this,
                   [this, pages] { Q_EMIT extractRequested(pages); });

    QAction *remove = menu.addAction(icon(QStringLiteral("page-delete")),
                                    tr("Delete %n page(s)", nullptr, count), this,
                                    &ThumbnailPanel::deleteSelection);
    remove->setEnabled(count < m_session->pageCount());

    menu.exec(m_list->viewport()->mapToGlobal(at));
}

void ThumbnailPanel::rotateSelection(int degrees)
{
    const QList<int> pages = selectedPages();
    if (pages.isEmpty())
        return;

    QString error;
    const bool rotated = m_session->applyStructuralEdit(
        degrees < 0 ? tr("Rotate pages left") : tr("Rotate pages right"),
        [pages, degrees](core::PdfDocument &document) {
            for (const int page : pages)
                document.setPageRotation(page, document.pageRotation(page) + degrees);
        },
        &error);

    Q_EMIT statusMessage(rotated ? tr("Rotated %n page(s).", nullptr, int(pages.size())) : error);
}

void ThumbnailPanel::deleteSelection()
{
    const QList<int> pages = selectedPages();
    if (pages.isEmpty() || pages.size() >= m_session->pageCount())
        return;

    QString error;
    const bool deleted = m_session->applyStructuralEdit(
        tr("Delete pages"),
        [pages](core::PdfDocument &document) { document.deletePages(pages); }, &error);

    if (deleted)
        m_session->setCurrentPage(qMax(0, pages.first() - 1));
    Q_EMIT statusMessage(deleted ? tr("Deleted %n page(s).", nullptr, int(pages.size())) : error);
}

void ThumbnailPanel::duplicateSelection()
{
    const QList<int> pages = selectedPages();
    if (pages.isEmpty())
        return;

    QString error;
    const bool copied = m_session->applyStructuralEdit(
        tr("Duplicate pages"),
        [pages](core::PdfDocument &document) {
            // Back to front: each copy lands right after its original, so the
            // indices of the pages still to be copied do not move.
            for (auto it = pages.crbegin(); it != pages.crend(); ++it)
                document.duplicatePage(*it);
        },
        &error);

    Q_EMIT statusMessage(copied ? tr("Duplicated %n page(s).", nullptr, int(pages.size())) : error);
}

void ThumbnailPanel::insertBlank(int atPage)
{
    const QSizeF size = m_session->document()
        ? m_session->document()->pageSizePoints(qMax(0, atPage - 1))
        : QSizeF(612, 792);

    QString error;
    const bool inserted = m_session->applyStructuralEdit(
        tr("Insert blank page"),
        [atPage, size](core::PdfDocument &document) {
            document.insertBlankPage(atPage, size.isEmpty() ? QSizeF(612, 792) : size);
        },
        &error);

    if (inserted)
        m_session->setCurrentPage(atPage);
    Q_EMIT statusMessage(inserted ? tr("Inserted a blank page.") : error);
}

void ThumbnailPanel::setSession(core::DocumentSession *session)
{
    if (m_session == session)
        return;

    if (m_session)
        m_session->disconnect(this);

    m_session = session;
    if (m_session) {
        connect(m_session, &core::DocumentSession::documentChanged, this,
                &ThumbnailPanel::rebuild);
        connect(m_session, &core::DocumentSession::rotationChanged, this,
                &ThumbnailPanel::rebuild);
        connect(m_session, &core::DocumentSession::currentPageChanged, this,
                &ThumbnailPanel::syncSelection);
        connect(m_session->renderQueue(), &core::RenderQueue::rendered, this,
                &ThumbnailPanel::onRendered);
    }
    rebuild();
}

void ThumbnailPanel::rebuild()
{
    m_syncing = true;
    m_list->clear();
    m_scales.clear();
    m_syncing = false;

    if (!m_session || !m_session->document())
        return;

    const auto &document = m_session->document();
    const int rotation = m_session->rotation();

    for (int index = 0; index < document->pageCount(); ++index) {
        QSizeF size = document->pageSizePoints(index);
        if (rotation == 90 || rotation == 270)
            size.transpose();

        m_scales.append(thumbnailWidth / qMax(1.0, size.width()));

        auto *item = new QListWidgetItem(QString::number(index + 1), m_list);
        item->setData(pageSizeRole, size);
        item->setToolTip(tr("Page %1").arg(index + 1));
    }

    syncSelection(m_session->currentPage());
    requestVisibleThumbnails();
}

void ThumbnailPanel::requestVisibleThumbnails()
{
    if (!m_session || !m_session->document())
        return;

    // A generous margin above and below keeps scrolling ahead of the user.
    const QRect viewportRect = m_list->viewport()->rect().adjusted(0, -500, 0, 500);
    for (int index = 0; index < m_list->count(); ++index) {
        const QListWidgetItem *item = m_list->item(index);
        if (!viewportRect.intersects(m_list->visualItemRect(item)))
            continue;
        m_session->renderQueue()->request(index, m_scales.value(index, 0.2),
                                         m_session->rotation(),
                                         core::RenderPriority::Thumbnail);
    }
}

void ThumbnailPanel::onRendered(const core::RenderKey &key, const QImage &image)
{
    if (key.pageIndex < 0 || key.pageIndex >= m_list->count())
        return;
    if (key.scaleMilli != core::RenderKey::quantise(m_scales.value(key.pageIndex, -1.0)))
        return; // A canvas-sized bitmap, not the thumbnail we asked for.

    m_list->item(key.pageIndex)->setData(thumbnailRole, QPixmap::fromImage(image));
}

void ThumbnailPanel::syncSelection(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= m_list->count())
        return;

    m_syncing = true;
    m_list->setCurrentRow(pageIndex);
    m_list->scrollToItem(m_list->item(pageIndex), QAbstractItemView::EnsureVisible);
    m_syncing = false;
    requestVisibleThumbnails();
}

} // namespace sweet::ui
