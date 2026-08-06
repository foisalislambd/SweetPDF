#include "ui/viewer/FindBar.h"

#include "core/pdf/PdfDocument.h"
#include "core/session/DocumentSession.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QToolButton>

namespace sweet::ui {
namespace {

/// Pages scanned per tick. Small enough that typing stays smooth on a slow
/// machine, large enough that a normal document finishes in a blink.
constexpr int pagesPerChunk = 4;

QToolButton *makeToggle(const QString &tooltip)
{
    auto *button = new QToolButton;
    button->setCheckable(true);
    button->setToolTip(tooltip);
    button->setAutoRaise(true);
    button->setIconSize(QSize(16, 16));
    return button;
}

} // namespace

FindBar::FindBar(core::DocumentSession *session, QWidget *parent)
    : QFrame(parent)
    , m_session(session)
{
    setObjectName(QStringLiteral("FloatingBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    Theme::setElevation(this, Elevation::High);
    hide();

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(10, 7, 8, 7);
    row->setSpacing(4);

    m_field = new QLineEdit(this);
    m_field->setPlaceholderText(tr("Find in document"));
    m_field->setClearButtonEnabled(true);
    m_field->setMinimumWidth(140);
    row->addWidget(m_field);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("Caption"));
    m_status->setMinimumWidth(74);
    m_status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_status->setFont(themeFont(TextRole::Small));
    row->addWidget(m_status);

    m_caseButton = makeToggle(tr("Match case"));
    m_wordsButton = makeToggle(tr("Whole words only"));
    m_previousButton = makeToggle(tr("Previous match (Shift+F3)"));
    m_previousButton->setCheckable(false);
    m_nextButton = makeToggle(tr("Next match (F3)"));
    m_nextButton->setCheckable(false);
    m_closeButton = makeToggle(tr("Close (Esc)"));
    m_closeButton->setCheckable(false);

    for (QToolButton *button :
         { m_caseButton, m_wordsButton, m_previousButton, m_nextButton, m_closeButton }) {
        button->setParent(this);
        row->addWidget(button);
    }

    applyThemeIcons();
    connect(&Theme::instance(), &Theme::changed, this, [this] {
        applyThemeIcons();
        Theme::setElevation(this, Elevation::High);
    });

    m_scanner = new QTimer(this);
    m_scanner->setInterval(0);
    connect(m_scanner, &QTimer::timeout, this, &FindBar::scanNextChunk);

    connect(m_field, &QLineEdit::textChanged, this, &FindBar::restartSearch);
    connect(m_field, &QLineEdit::returnPressed, this, &FindBar::findNext);
    connect(m_caseButton, &QToolButton::toggled, this, &FindBar::restartSearch);
    connect(m_wordsButton, &QToolButton::toggled, this, &FindBar::restartSearch);
    connect(m_nextButton, &QToolButton::clicked, this, &FindBar::findNext);
    connect(m_previousButton, &QToolButton::clicked, this, &FindBar::findPrevious);
    connect(m_closeButton, &QToolButton::clicked, this, &FindBar::dismiss);

    // A new document invalidates every hit.
    connect(m_session, &core::DocumentSession::documentChanged, this, &FindBar::restartSearch);

    updateStatus();
}

void FindBar::applyThemeIcons()
{
    m_caseButton->setIcon(icon(QStringLiteral("case-sensitive")));
    m_wordsButton->setIcon(icon(QStringLiteral("whole-word")));
    m_previousButton->setIcon(icon(QStringLiteral("chevron-up")));
    m_nextButton->setIcon(icon(QStringLiteral("chevron-down")));
    m_closeButton->setIcon(icon(QStringLiteral("close")));
}

void FindBar::activate(const QString &initialText)
{
    if (!initialText.isEmpty())
        m_field->setText(initialText);
    show();
    raise();
    m_field->setFocus();
    m_field->selectAll();
    if (!m_field->text().isEmpty())
        restartSearch();
}

void FindBar::dismiss()
{
    m_scanner->stop();
    hide();
    m_hits.clear();
    m_byPage.clear();
    m_current = -1;
    Q_EMIT matchesChanged(m_byPage);
    Q_EMIT dismissed();
}

int FindBar::matchCount() const
{
    return int(m_hits.size());
}

void FindBar::restartSearch()
{
    m_scanner->stop();
    m_hits.clear();
    m_byPage.clear();
    m_current = -1;
    m_needle = m_field->text();
    m_nextPageToScan = 0;
    m_pendingJumpToFirst = true;

    Q_EMIT matchesChanged(m_byPage);

    if (m_needle.isEmpty() || !m_session->isLoaded()) {
        updateStatus();
        return;
    }

    // Start from the page being read, wrapping at the end, so the first hit found
    // is the one nearest to where the reader is.
    m_nextPageToScan = 0;
    m_scanner->start();
    updateStatus();
}

void FindBar::scanNextChunk()
{
    const auto &document = m_session->document();
    if (!document || m_needle.isEmpty()) {
        m_scanner->stop();
        return;
    }

    const int pages = document->pageCount();
    const int startPage = m_session->currentPage();
    const auto sensitivity = m_caseButton->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const bool wholeWords = m_wordsButton->isChecked();

    for (int step = 0; step < pagesPerChunk && m_nextPageToScan < pages; ++step) {
        // Scanned in reading order starting at the current page, so results come
        // back in the order the reader will meet them.
        const int pageIndex = (startPage + m_nextPageToScan) % pages;
        ++m_nextPageToScan;

        auto page = document->page(pageIndex);
        if (!page)
            continue;

        const auto text = page->textLayer();
        if (!text)
            continue;

        const QList<core::TextRange> found = text->findAll(m_needle, sensitivity, wholeWords);
        if (found.isEmpty())
            continue;

        m_byPage.insert(pageIndex, found);
        for (const core::TextRange &range : found)
            m_hits.append({ pageIndex, range });
    }

    if (!m_hits.isEmpty()) {
        Q_EMIT matchesChanged(m_byPage);
        if (m_pendingJumpToFirst) {
            m_pendingJumpToFirst = false;
            stepTo(0);
        }
    }

    if (m_nextPageToScan >= pages)
        m_scanner->stop();

    updateStatus();
}

void FindBar::stepTo(int flatIndex)
{
    if (m_hits.isEmpty())
        return;

    const int count = int(m_hits.size());
    m_current = ((flatIndex % count) + count) % count;
    const Hit &hit = m_hits.at(m_current);
    Q_EMIT currentMatchChanged(hit.pageIndex, hit.range);
    updateStatus();
}

void FindBar::findNext()
{
    if (m_hits.isEmpty()) {
        if (!m_needle.isEmpty() && !m_scanner->isActive())
            restartSearch();
        return;
    }
    stepTo(m_current + 1);
}

void FindBar::findPrevious()
{
    if (m_hits.isEmpty())
        return;
    stepTo(m_current - 1);
}

void FindBar::updateStatus()
{
    const bool enabled = !m_hits.isEmpty();
    m_nextButton->setEnabled(enabled);
    m_previousButton->setEnabled(enabled);

    if (m_needle.isEmpty()) {
        m_status->clear();
        return;
    }
    if (m_hits.isEmpty()) {
        m_status->setText(m_scanner->isActive() ? tr("Searching…") : tr("No results"));
        return;
    }

    const QString position = m_current >= 0 ? QString::number(m_current + 1) : QStringLiteral("–");
    m_status->setText(m_scanner->isActive()
                          ? tr("%1 of %2+").arg(position).arg(m_hits.size())
                          : tr("%1 of %2").arg(position).arg(m_hits.size()));
}

void FindBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        dismiss();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_F3) {
        if (event->modifiers().testFlag(Qt::ShiftModifier))
            findPrevious();
        else
            findNext();
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}

} // namespace sweet::ui
