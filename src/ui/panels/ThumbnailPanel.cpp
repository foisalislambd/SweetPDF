#include "ui/panels/ThumbnailPanel.h"

#include "core/pdf/PdfDocument.h"
#include "core/session/DocumentSession.h"
#include "ui/theme/Theme.h"

#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

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
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
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
