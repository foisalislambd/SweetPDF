#include "ui/panels/FormsPanel.h"

#include "core/session/DocumentSession.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedLayout>
#include <QTimer>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

/// What to show as the field's caption: the author's tooltip if there is one,
/// otherwise the field name with its path stripped off.
QString captionFor(const core::FormField &field)
{
    if (!field.label.isEmpty())
        return field.label;
    const QString name = field.name.section(QLatin1Char('.'), -1);
    return name.isEmpty() ? core::forms::displayName(field.type) : name;
}

} // namespace

FormsPanel::FormsPanel(QWidget *parent)
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

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_fields = new QWidget(scroll);
    m_fieldsLayout = new QVBoxLayout(m_fields);
    m_fieldsLayout->setContentsMargins(12, 4, 12, 12);
    m_fieldsLayout->setSpacing(10);
    m_fieldsLayout->addStretch(1);
    scroll->setWidget(m_fields);
    pageLayout->addWidget(scroll, 1);

    auto *reset = new QPushButton(tr("Reset form"), page);
    reset->setIcon(icon(QStringLiteral("undo")));
    reset->setContentsMargins(12, 0, 12, 0);
    connect(reset, &QPushButton::clicked, this, [this] {
        if (!m_session || !m_session->isLoaded())
            return;
        QString error;
        const bool done = m_session->applyStructuralEdit(
            tr("Reset form"),
            [](core::PdfDocument &document) { core::forms::resetAll(document); }, &error);
        Q_EMIT statusMessage(done ? tr("Form reset to its original values.") : error);
    });
    pageLayout->addWidget(reset);
    m_stack->addWidget(page);

    m_emptyState = new QWidget(this);
    auto *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setContentsMargins(22, 32, 22, 22);
    emptyLayout->setSpacing(10);
    emptyLayout->addStretch(1);

    auto *glyph = new QLabel(m_emptyState);
    glyph->setAlignment(Qt::AlignCenter);
    glyph->setPixmap(Theme::instance()
                         .icon(QStringLiteral("form"), Theme::instance().palette().textMuted)
                         .pixmap(32, 32));
    emptyLayout->addWidget(glyph);

    auto *heading = new QLabel(tr("No form fields"), m_emptyState);
    heading->setObjectName(QStringLiteral("PanelTitle"));
    heading->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(heading);

    auto *hint = new QLabel(tr("When a PDF has fillable boxes they are listed here, page by "
                               "page."),
                            m_emptyState);
    hint->setObjectName(QStringLiteral("SectionLabel"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    emptyLayout->addWidget(hint);
    emptyLayout->addStretch(2);
    m_stack->addWidget(m_emptyState);

    m_rebuildTimer = new QTimer(this);
    m_rebuildTimer->setSingleShot(true);
    m_rebuildTimer->setInterval(80);
    connect(m_rebuildTimer, &QTimer::timeout, this, &FormsPanel::rebuild);

    rebuild();
}

void FormsPanel::setSession(core::DocumentSession *session)
{
    if (m_session)
        m_session->disconnect(this);

    m_session = session;
    if (m_session) {
        connect(m_session, &core::DocumentSession::documentChanged, this,
                &FormsPanel::scheduleRebuild);
        connect(m_session, &core::DocumentSession::formsChanged, this, [this] {
            if (!m_writing)
                scheduleRebuild();
        });
    }
    rebuild();
}

void FormsPanel::scheduleRebuild()
{
    m_rebuildTimer->start();
}

void FormsPanel::clearRows()
{
    while (QLayoutItem *item = m_fieldsLayout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }
    m_fieldsLayout->addStretch(1);
}

void FormsPanel::rebuild()
{
    clearRows();
    m_fieldCount = 0;

    QList<core::FormField> fields;
    if (m_session && m_session->isLoaded() && m_session->document()->isPdf())
        fields = core::forms::all(*m_session->document());

    int lastPage = -1;
    for (const core::FormField &field : fields) {
        if (field.type == core::FieldType::PushButton || field.type == core::FieldType::Unknown)
            continue;

        if (field.pageIndex != lastPage) {
            lastPage = field.pageIndex;
            auto *header = new QLabel(tr("Page %1").arg(field.pageIndex + 1), m_fields);
            header->setObjectName(QStringLiteral("SectionLabel"));
            m_fieldsLayout->insertWidget(m_fieldsLayout->count() - 1, header);
        }

        addRow(field);
        ++m_fieldCount;
    }

    m_summary->setText(tr("%n field(s) to fill in", nullptr, m_fieldCount));
    m_stack->setCurrentIndex(m_fieldCount == 0 ? 1 : 0);
}

void FormsPanel::addRow(const core::FormField &field)
{
    auto *row = new QWidget(m_fields);
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);

    const int pageIndex = field.pageIndex;
    const int objectNumber = field.objectNumber;

    // Committing a value writes it into the document; the guard stops the
    // resulting change signal from rebuilding the row being edited.
    const auto commit = [this, pageIndex, objectNumber](const QString &value) {
        if (!m_session)
            return;
        m_writing = true;
        m_session->setFormValue(pageIndex, objectNumber, value);
        m_writing = false;
    };
    const auto announce = [this, pageIndex, objectNumber] {
        Q_EMIT fieldActivated(pageIndex, objectNumber);
    };

    if (field.type != core::FieldType::Checkbox && field.type != core::FieldType::RadioButton) {
        auto *caption = new QLabel(captionFor(field), row);
        caption->setWordWrap(true);
        if (field.required)
            caption->setText(caption->text() + QStringLiteral(" *"));
        layout->addWidget(caption);
    }

    switch (field.type) {
    case core::FieldType::Checkbox:
    case core::FieldType::RadioButton: {
        auto *box = new QCheckBox(captionFor(field), row);
        box->setChecked(!field.value.isEmpty() && field.value != QStringLiteral("Off"));
        box->setEnabled(!field.readOnly);
        const QString onState = m_session
            ? core::forms::onStateOf(*m_session->document(), pageIndex, objectNumber)
            : QStringLiteral("Yes");
        connect(box, &QCheckBox::toggled, row, [commit, onState](bool checked) {
            commit(checked ? onState : QStringLiteral("Off"));
        });
        connect(box, &QCheckBox::clicked, row, announce);
        layout->addWidget(box);
        break;
    }
    case core::FieldType::ComboBox:
    case core::FieldType::ListBox: {
        auto *combo = new QComboBox(row);
        combo->addItems(field.options);
        combo->setEditable(field.type == core::FieldType::ComboBox && !field.readOnly
                           && field.options.isEmpty());
        combo->setCurrentText(field.value);
        combo->setEnabled(!field.readOnly);
        connect(combo, &QComboBox::currentTextChanged, row, commit);
        layout->addWidget(combo);
        break;
    }
    case core::FieldType::Text: {
        if (field.multiline) {
            auto *edit = new QPlainTextEdit(field.value, row);
            edit->setFixedHeight(72);
            edit->setReadOnly(field.readOnly);
            // Multi-line text is committed when focus leaves, not per keystroke.
            connect(edit, &QPlainTextEdit::textChanged, row, [edit, commit] {
                commit(edit->toPlainText());
            });
            layout->addWidget(edit);
        } else {
            auto *edit = new QLineEdit(field.value, row);
            edit->setReadOnly(field.readOnly);
            edit->setPlaceholderText(field.required ? tr("Required") : QString());
            connect(edit, &QLineEdit::editingFinished, row, [edit, commit] {
                commit(edit->text());
            });
            layout->addWidget(edit);
        }
        break;
    }
    case core::FieldType::Signature: {
        QString text = tr("Not signed");
        switch (field.signature) {
        case core::SignatureStatus::Valid:
            text = tr("Signature is valid");
            break;
        case core::SignatureStatus::Invalid:
            text = tr("Signature is invalid");
            break;
        case core::SignatureStatus::BrokenByEdits:
            text = tr("Signature does not cover the latest changes");
            break;
        case core::SignatureStatus::Unchecked:
            text = field.signed_ ? tr("Signed, but it could not be checked") : tr("Not signed");
            break;
        case core::SignatureStatus::Unsigned:
        case core::SignatureStatus::NotApplicable:
            text = tr("Not signed");
            break;
        }
        auto *state = new QLabel(text, row);
        state->setObjectName(QStringLiteral("SectionLabel"));
        state->setWordWrap(true);
        layout->addWidget(state);
        break;
    }
    case core::FieldType::PushButton:
    case core::FieldType::Unknown:
        break;
    }

    auto *link = new QPushButton(tr("Show on page %1").arg(pageIndex + 1), row);
    link->setFlat(true);
    link->setCursor(Qt::PointingHandCursor);
    connect(link, &QPushButton::clicked, row, announce);
    layout->addWidget(link);

    m_fieldsLayout->insertWidget(m_fieldsLayout->count() - 1, row);
}

} // namespace sweet::ui
