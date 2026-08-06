#pragma once

#include "core/pdf/PageText.h"

#include <QFrame>
#include <QHash>
#include <QList>

class QLabel;
class QLineEdit;
class QToolButton;
class QTimer;

namespace sweet::core {
class DocumentSession;
}

namespace sweet::ui {

/// The find bar that floats over the top of the page canvas.
///
/// Searching runs a few pages per timer tick rather than all at once, so a long
/// document stays responsive and results appear as they are found - the first hit
/// is usable long before the last page has been scanned.
class FindBar : public QFrame
{
    Q_OBJECT

public:
    explicit FindBar(sweet::core::DocumentSession *session, QWidget *parent = nullptr);

    /// Shows the bar, focuses the field and seeds it with `initialText` when given.
    void activate(const QString &initialText = QString());

    /// Hides the bar and drops the highlighting.
    void dismiss();

    int matchCount() const;

public Q_SLOTS:
    void findNext();
    void findPrevious();

Q_SIGNALS:
    /// New results for the whole document, keyed by page.
    void matchesChanged(const QHash<int, QList<sweet::core::TextRange>> &matches);

    /// The hit the view should scroll to and draw as current.
    void currentMatchChanged(int pageIndex, const sweet::core::TextRange &range);

    void dismissed();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void restartSearch();
    void scanNextChunk();
    void stepTo(int flatIndex);
    void updateStatus();
    void applyThemeIcons();

    struct Hit
    {
        int pageIndex = 0;
        sweet::core::TextRange range;
    };

    sweet::core::DocumentSession *m_session = nullptr;
    QLineEdit *m_field = nullptr;
    QToolButton *m_previousButton = nullptr;
    QToolButton *m_nextButton = nullptr;
    QToolButton *m_caseButton = nullptr;
    QToolButton *m_wordsButton = nullptr;
    QToolButton *m_closeButton = nullptr;
    QLabel *m_status = nullptr;

    QTimer *m_scanner = nullptr;
    QString m_needle;
    int m_nextPageToScan = 0;
    /// Every hit in reading order, which is what "next" and "previous" walk.
    QList<Hit> m_hits;
    QHash<int, QList<sweet::core::TextRange>> m_byPage;
    int m_current = -1;
    bool m_pendingJumpToFirst = false;
};

} // namespace sweet::ui
