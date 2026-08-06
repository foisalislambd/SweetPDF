#include "ui/ThumbnailPanel.h"

#include "core/DocumentSession.h"
#include "core/PdfDocument.h"
#include "ui/Theme.h"

#include <QListWidget>
#include <QPainter>
#include <QScrollBar>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

constexpr int thumbnailWidth = 132;
constexpr int labelHeight = 20;

QPixmap placeholder(const QSizeF &pageSize, const QColor &fill, const QColor &border)
{
    const qreal aspect = pageSize.height() / qMax(1.0, pageSize.width());
    const QSize size(thumbnailWidth, qMax(24, qRound(thumbnailWidth * aspect)));

    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setPen(QPen(border, 1));
    painter.setBrush(fill);
    painter.drawRect(QRect(QPoint(0, 0), size - QSize(1, 1)));
    return pixmap;
}

} // namespace

ThumbnailPanel::ThumbnailPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::IconMode);
    m_list->setFlow(QListView::TopToBottom);
    m_list->setWrapping(false);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(6);
    m_list->setUniformItemSizes(false);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setIconSize(QSize(thumbnailWidth, thumbnailWidth * 3));
    layout->addWidget(m_list);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_syncing || !m_session || row < 0)
            return;
        m_session->setCurrentPage(row);
    });
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &ThumbnailPanel::requestVisibleThumbnails);
    connect(&Theme::instance(), &Theme::changed, this, &ThumbnailPanel::rebuild);
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
    const Palette &colors = Theme::instance().palette();

    for (int index = 0; index < document->pageCount(); ++index) {
        QSizeF size = document->pageSizePoints(index);
        if (rotation == 90 || rotation == 270)
            size.transpose();

        const qreal scale = thumbnailWidth / qMax(1.0, size.width());
        m_scales.append(scale);

        auto *item = new QListWidgetItem(QString::number(index + 1), m_list);
        item->setTextAlignment(Qt::AlignHCenter);
        item->setIcon(QIcon(placeholder(size, colors.surface, colors.border)));
        const int height = qRound(size.height() * scale);
        item->setSizeHint(QSize(thumbnailWidth + 12, height + labelHeight));
    }

    syncSelection(m_session->currentPage());
    requestVisibleThumbnails();
}

void ThumbnailPanel::requestVisibleThumbnails()
{
    if (!m_session || !m_session->document())
        return;

    const QRect viewportRect = m_list->viewport()->rect().adjusted(0, -400, 0, 400);
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
        return; // A canvas-sized bitmap; not the thumbnail we asked for.

    m_list->item(key.pageIndex)->setIcon(QIcon(QPixmap::fromImage(image)));
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
