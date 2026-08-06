#include "ui/window/WelcomeView.h"

#include "ui/theme/Theme.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

constexpr int contentWidth = 560;
constexpr int recentLimit = 6;

QLabel *makeLabel(const QString &text, TextRole role, const QColor &color, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setFont(themeFont(role));
    label->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
    label->setAlignment(Qt::AlignCenter);
    return label;
}

} // namespace

WelcomeView::WelcomeView(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(32, 32, 32, 32);
    outer->addStretch(1);

    auto *column = new QWidget(this);
    column->setFixedWidth(contentWidth);
    auto *layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    outer->addWidget(column, 0, Qt::AlignVCenter);
    outer->addStretch(1);

    layout->addStretch(1);

    auto *logo = new QLabel(column);
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(Theme::instance()
                        .icon(QStringLiteral("logo"), themePalette().accent)
                        .pixmap(56, 56));
    layout->addWidget(logo);

    layout->addWidget(makeLabel(tr("SweetPDF"), TextRole::Display, themePalette().text, column));
    layout->addWidget(makeLabel(tr("View, annotate and organise PDF documents."),
                                TextRole::Body, themePalette().textMuted, column));
    layout->addSpacing(10);

    // Drop zone, which is also where the open button lives so the two ways of
    // getting a document in sit together.
    auto *dropZone = new QWidget(column);
    dropZone->setObjectName(QStringLiteral("WelcomeDropZone"));
    dropZone->setAttribute(Qt::WA_StyledBackground, true);
    auto *dropLayout = new QVBoxLayout(dropZone);
    dropLayout->setContentsMargins(24, 22, 24, 22);
    dropLayout->setSpacing(12);

    dropLayout->addWidget(makeLabel(tr("Drop a PDF here"), TextRole::Title,
                                    themePalette().text, dropZone));
    dropLayout->addWidget(makeLabel(tr("or pick one from your computer"), TextRole::Small,
                                    themePalette().textMuted, dropZone));

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    auto *openButton = new QPushButton(icon(QStringLiteral("open")), tr("Open PDF"), dropZone);
    openButton->setProperty("accent", true);
    openButton->setMinimumWidth(150);
    connect(openButton, &QPushButton::clicked, this, &WelcomeView::openRequested);
    buttonRow->addWidget(openButton);
    buttonRow->addStretch(1);
    dropLayout->addLayout(buttonRow);

    layout->addWidget(dropZone);
    layout->addSpacing(14);

    m_recentSection = new QWidget(column);
    m_recentLayout = new QVBoxLayout(m_recentSection);
    m_recentLayout->setContentsMargins(0, 0, 0, 0);
    m_recentLayout->setSpacing(5);
    layout->addWidget(m_recentSection);
    m_recentSection->hide();

    layout->addStretch(2);

    connect(&Theme::instance(), &Theme::changed, this, [this, logo] {
        logo->setPixmap(Theme::instance()
                            .icon(QStringLiteral("logo"), themePalette().accent)
                            .pixmap(56, 56));
        rebuildRecentCards();
    });
}

void WelcomeView::setRecentFiles(const QStringList &paths)
{
    m_recentFiles = paths;
    rebuildRecentCards();
}

void WelcomeView::rebuildRecentCards()
{
    QLayoutItem *item = nullptr;
    while ((item = m_recentLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    const QStringList existing = [this] {
        QStringList result;
        for (const QString &path : m_recentFiles) {
            if (QFileInfo::exists(path))
                result.append(path);
            if (result.size() == recentLimit)
                break;
        }
        return result;
    }();

    m_recentSection->setVisible(!existing.isEmpty());
    if (existing.isEmpty())
        return;

    auto *header = new QHBoxLayout;
    auto *caption = new QLabel(tr("Recent"), m_recentSection);
    caption->setFont(themeFont(TextRole::SectionLabel));
    caption->setStyleSheet(QStringLiteral("color: %1;").arg(themePalette().textMuted.name()));
    header->addWidget(caption, 1);

    auto *clear = new QPushButton(tr("Clear"), m_recentSection);
    clear->setProperty("flat", true);
    clear->setFont(themeFont(TextRole::Small));
    connect(clear, &QPushButton::clicked, this, &WelcomeView::clearRecentRequested);
    header->addWidget(clear);
    m_recentLayout->addLayout(header);

    for (const QString &path : existing) {
        const QFileInfo info(path);
        auto *card = new QPushButton(m_recentSection);
        card->setObjectName(QStringLiteral("WelcomeCard"));
        card->setIcon(icon(QStringLiteral("file-pdf")));
        card->setIconSize(QSize(20, 20));
        card->setText(info.fileName());
        card->setToolTip(path);
        card->setCursor(Qt::PointingHandCursor);
        card->setStyleSheet(QStringLiteral("text-align: left; padding: 10px 12px;"));
        connect(card, &QPushButton::clicked, this,
                [this, path] { Q_EMIT fileRequested(path); });
        m_recentLayout->addWidget(card);
    }
}

} // namespace sweet::ui
