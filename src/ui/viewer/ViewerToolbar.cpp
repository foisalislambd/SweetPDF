#include "ui/viewer/ViewerToolbar.h"

#include "ui/theme/Theme.h"
#include "ui/viewer/PageView.h"

#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QToolButton>

namespace sweet::ui {
namespace {

constexpr int bottomMargin = 10;
constexpr int sideMargin = 16;

const QList<int> zoomPresets = { 25, 50, 75, 100, 125, 150, 200, 300, 400, 800 };

} // namespace

ViewerToolbar::ViewerToolbar(PageView *view)
    : QWidget(view->viewport())
    , m_view(view)
{
    setObjectName(QStringLiteral("FloatingBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    // Lifts the bar off the page instead of letting it look pasted on.
    Theme::setElevation(this, Elevation::High);
    connect(&Theme::instance(), &Theme::changed, this,
            [this] { Theme::setElevation(this, Elevation::High); });

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(8, 3, 8, 3);
    row->setSpacing(2);

    auto *session = view->session();

    QToolButton *select = addButton(QStringLiteral("cursor"), tr("Select (V)"), true);
    QToolButton *hand = addButton(QStringLiteral("hand"), tr("Pan (H)"), true);
    select->setChecked(session->tool() == core::Tool::Select);
    hand->setChecked(session->tool() == core::Tool::Hand);
    connect(select, &QToolButton::clicked, session,
            [session] { session->setTool(core::Tool::Select); });
    connect(hand, &QToolButton::clicked, session,
            [session] { session->setTool(core::Tool::Hand); });
    connect(session, &core::DocumentSession::toolChanged, this,
            [select, hand](core::Tool tool) {
                select->setChecked(tool == core::Tool::Select);
                hand->setChecked(tool == core::Tool::Hand);
            });

    auto *divider = new QFrame(this);
    divider->setFrameShape(QFrame::VLine);
    divider->setFixedWidth(1);
    row->addSpacing(4);
    row->addWidget(divider);
    row->addSpacing(4);

    QToolButton *zoomOut = addButton(QStringLiteral("minus"), tr("Zoom out (Ctrl+-)"), false);
    connect(zoomOut, &QToolButton::clicked, view, &PageView::zoomOut);

    m_zoomBox = new QComboBox(this);
    m_zoomBox->setEditable(true);
    m_zoomBox->setInsertPolicy(QComboBox::NoInsert);
    m_zoomBox->setFixedWidth(84);
    for (const int preset : zoomPresets)
        m_zoomBox->addItem(QStringLiteral("%1%").arg(preset), preset / 100.0);
    row->addWidget(m_zoomBox);

    connect(m_zoomBox, &QComboBox::activated, this, [this](int index) {
        const QVariant value = m_zoomBox->itemData(index);
        if (value.isValid())
            m_view->zoomTo(value.toDouble());
    });
    // Committing on Enter only. editingFinished also fires when the box loses
    // focus, which would turn a value this toolbar wrote itself into a manual
    // zoom and quietly cancel fit-width.
    connect(m_zoomBox->lineEdit(), &QLineEdit::returnPressed, this, [this] {
        if (m_syncingZoom)
            return;
        QString text = m_zoomBox->currentText();
        text.remove(QLatin1Char('%'));
        bool ok = false;
        const double percent = text.trimmed().toDouble(&ok);
        if (ok && percent > 0)
            m_view->zoomTo(percent / 100.0);
        else
            syncZoomBox(m_view->session()->zoom());
    });

    QToolButton *zoomIn = addButton(QStringLiteral("plus"), tr("Zoom in (Ctrl++)"), false);
    connect(zoomIn, &QToolButton::clicked, view, &PageView::zoomIn);

    m_fitDivider = new QFrame(this);
    m_fitDivider->setFrameShape(QFrame::VLine);
    m_fitDivider->setFixedWidth(1);
    row->addSpacing(4);
    row->addWidget(m_fitDivider);
    row->addSpacing(4);

    QToolButton *fitPage = addButton(QStringLiteral("fit-page"), tr("Fit page"), false);
    connect(fitPage, &QToolButton::clicked, view, &PageView::fitPage);
    m_fitButtons.append(fitPage);

    QToolButton *fitWidth = addButton(QStringLiteral("fit-width"), tr("Fit width"), false);
    connect(fitWidth, &QToolButton::clicked, view, &PageView::fitWidth);
    m_fitButtons.append(fitWidth);

    QToolButton *layoutButton = addButton(QStringLiteral("continuous"), tr("Page layout"), false);
    connect(layoutButton, &QToolButton::clicked, session, [session] {
        // Cycles through the layouts most people switch between.
        switch (session->layout()) {
        case core::PageLayout::Continuous:
            session->setLayout(core::PageLayout::SinglePage);
            break;
        case core::PageLayout::SinglePage:
            session->setLayout(core::PageLayout::TwoPage);
            break;
        default:
            session->setLayout(core::PageLayout::Continuous);
            break;
        }
    });
    connect(session, &core::DocumentSession::layoutChanged, this,
            [layoutButton](core::PageLayout layout) {
                switch (layout) {
                case core::PageLayout::SinglePage:
                    layoutButton->setIcon(icon(QStringLiteral("single-page")));
                    break;
                case core::PageLayout::TwoPage:
                case core::PageLayout::TwoPageContinuous:
                    layoutButton->setIcon(icon(QStringLiteral("two-page")));
                    break;
                default:
                    layoutButton->setIcon(icon(QStringLiteral("continuous")));
                    break;
                }
            });
    m_fitButtons.append(layoutButton);

    connect(session, &core::DocumentSession::zoomChanged, this, &ViewerToolbar::syncZoomBox);
    syncZoomBox(session->zoom());

    view->viewport()->installEventFilter(this);
    reposition();
}

QToolButton *ViewerToolbar::addButton(const QString &iconName, const QString &tooltip,
                                      bool checkable)
{
    auto *button = new QToolButton(this);
    button->setIcon(icon(iconName));
    button->setIconSize(QSize(18, 18));
    button->setToolTip(tooltip);
    button->setCheckable(checkable);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    connect(&Theme::instance(), &Theme::changed, button,
            [button, iconName] { button->setIcon(icon(iconName)); });
    layout()->addWidget(button);
    return button;
}

void ViewerToolbar::syncZoomBox(qreal zoom)
{
    m_syncingZoom = true;
    const QSignalBlocker blocker(m_zoomBox);
    m_zoomBox->setCurrentText(QStringLiteral("%1%").arg(qRound(zoom * 100)));
    m_syncingZoom = false;
}

bool ViewerToolbar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view->viewport() && event->type() == QEvent::Resize)
        reposition();
    return QWidget::eventFilter(watched, event);
}

void ViewerToolbar::setCompact(bool compact)
{
    if (compact == m_compact)
        return;
    m_compact = compact;
    if (m_fitDivider)
        m_fitDivider->setVisible(!compact);
    for (QToolButton *button : m_fitButtons)
        button->setVisible(!compact);
}

void ViewerToolbar::reposition()
{
    const QRect area = m_view->viewport()->rect();
    const int budget = qMax(0, area.width() - sideMargin * 2);

    // Show the full bar when it fits; otherwise drop fit/layout so the zoom
    // controls stay on screen instead of hanging off the edge.
    setCompact(false);
    QSize hint = sizeHint();
    if (hint.width() > budget) {
        setCompact(true);
        hint = sizeHint();
    }

    move(area.center().x() - hint.width() / 2, area.bottom() - hint.height() - bottomMargin);
    resize(hint);
    raise();
}

} // namespace sweet::ui
