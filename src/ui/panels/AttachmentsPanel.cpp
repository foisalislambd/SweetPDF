#include "ui/panels/AttachmentsPanel.h"

#include "core/pdf/PdfDocument.h"
#include "core/session/DocumentSession.h"
#include "ui/theme/Theme.h"

#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QStackedLayout>
#include <QVBoxLayout>

namespace sweet::ui {

AttachmentsPanel::AttachmentsPanel(QWidget *parent)
    : QWidget(parent)
{
    m_stack = new QStackedLayout(this);
    m_stack->setContentsMargins(0, 0, 0, 0);

    auto *page = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    m_summary = new QLabel(page);
    m_summary->setObjectName(QStringLiteral("SectionLabel"));
    m_summary->setContentsMargins(12, 8, 12, 4);
    m_summary->setWordWrap(true);
    pageLayout->addWidget(m_summary);

    m_files = new QListWidget(page);
    m_files->setFrameShape(QFrame::NoFrame);
    m_files->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pageLayout->addWidget(m_files, 1);

    auto *save = new QPushButton(tr("Save selected file…"), page);
    save->setIcon(icon(QStringLiteral("attachment")));
    connect(save, &QPushButton::clicked, this, [this] {
        const int row = m_files->currentRow();
        if (row < 0) {
            Q_EMIT statusMessage(tr("Select an embedded file first."));
            return;
        }
        Q_EMIT saveRequested(row);
    });
    pageLayout->addWidget(save);
    m_stack->addWidget(page);

    auto *empty = new QWidget(this);
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(22, 32, 22, 22);
    emptyLayout->setSpacing(10);
    emptyLayout->addStretch(1);

    auto *glyph = new QLabel(empty);
    glyph->setAlignment(Qt::AlignCenter);
    glyph->setPixmap(Theme::instance()
                         .icon(QStringLiteral("attachment"), Theme::instance().palette().textMuted)
                         .pixmap(32, 32));
    emptyLayout->addWidget(glyph);

    auto *heading = new QLabel(tr("No attachments"), empty);
    heading->setObjectName(QStringLiteral("PanelTitle"));
    heading->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(heading);

    auto *hint = new QLabel(tr("Files embedded in the PDF are listed here. Select one and "
                               "save it to disk."),
                            empty);
    hint->setObjectName(QStringLiteral("SectionLabel"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    emptyLayout->addWidget(hint);
    emptyLayout->addStretch(2);
    m_stack->addWidget(empty);

    rebuild();
}

void AttachmentsPanel::setSession(core::DocumentSession *session)
{
    if (m_session)
        m_session->disconnect(this);
    m_session = session;
    if (m_session) {
        connect(m_session, &core::DocumentSession::documentChanged, this,
                &AttachmentsPanel::rebuild);
    }
    rebuild();
}

void AttachmentsPanel::rebuild()
{
    m_files->clear();

    QList<core::EmbeddedFile> files;
    if (m_session && m_session->isLoaded())
        files = m_session->document()->embeddedFiles();

    const QLocale locale;
    for (const core::EmbeddedFile &file : files) {
        const QString name = file.name.isEmpty() ? tr("Unnamed file") : file.name;
        QString label = name;
        if (file.size >= 0)
            label += QStringLiteral("  ·  ") + locale.formattedDataSize(file.size);
        if (!file.mimeType.isEmpty())
            label += QStringLiteral("  ·  ") + file.mimeType;
        m_files->addItem(label);
    }

    m_summary->setText(tr("%n embedded file(s)", nullptr, files.size()));
    m_stack->setCurrentIndex(files.isEmpty() ? 1 : 0);
}

} // namespace sweet::ui
