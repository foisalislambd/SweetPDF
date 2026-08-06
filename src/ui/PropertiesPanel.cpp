#include "ui/PropertiesPanel.h"

#include "core/DocumentSession.h"
#include "core/PdfDocument.h"
#include "ui/Theme.h"

#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLocale>
#include <QScrollArea>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

QLabel *sectionLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text.toUpper(), parent);
    label->setObjectName(QStringLiteral("SectionLabel"));
    return label;
}

QString orDash(const QString &value)
{
    return value.isEmpty() ? QStringLiteral("—") : value;
}

QString describePageSize(const QSizeF &points)
{
    if (points.isEmpty())
        return QStringLiteral("—");

    const qreal millimetresPerPoint = 25.4 / 72.0;
    return QStringLiteral("%1 × %2 mm  (%3 × %4 pt)")
        .arg(QString::number(points.width() * millimetresPerPoint, 'f', 1),
             QString::number(points.height() * millimetresPerPoint, 'f', 1),
             QString::number(points.width(), 'f', 0),
             QString::number(points.height(), 'f', 0));
}

} // namespace

PropertiesPanel::PropertiesPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("PropertiesPanel"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("PanelHeader"));
    header->setFixedHeight(34);
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 12, 0);
    auto *title = new QLabel(tr("Properties"), header);
    title->setObjectName(QStringLiteral("PanelTitle"));
    headerLayout->addWidget(title);
    outer->addWidget(header);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    outer->addWidget(scroll, 1);

    auto *body = new QWidget(scroll);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(12, 12, 12, 16);
    layout->setSpacing(8);
    scroll->setWidget(body);

    layout->addWidget(sectionLabel(tr("Document"), body));
    auto *documentForm = new QFormLayout;
    documentForm->setLabelAlignment(Qt::AlignLeft);
    documentForm->setHorizontalSpacing(10);
    documentForm->setVerticalSpacing(6);
    documentForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(documentForm);

    m_fileName = addField(documentForm, tr("File"));
    m_title = addField(documentForm, tr("Title"));
    m_author = addField(documentForm, tr("Author"));
    m_producer = addField(documentForm, tr("Producer"));
    m_format = addField(documentForm, tr("Format"));

    auto *divider = new QFrame(body);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Plain);
    divider->setStyleSheet(QStringLiteral("color: %1;").arg(themePalette().border.name()));
    layout->addWidget(divider);

    layout->addWidget(sectionLabel(tr("Layout"), body));
    auto *layoutForm = new QFormLayout;
    layoutForm->setHorizontalSpacing(10);
    layoutForm->setVerticalSpacing(6);
    layoutForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(layoutForm);

    m_pageCount = addField(layoutForm, tr("Pages"));
    m_pageSize = addField(layoutForm, tr("Page size"));
    m_fileSize = addField(layoutForm, tr("File size"));

    layout->addSpacing(4);
    layout->addWidget(sectionLabel(tr("Security"), body));
    auto *securityForm = new QFormLayout;
    securityForm->setHorizontalSpacing(10);
    securityForm->setVerticalSpacing(6);
    securityForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(securityForm);

    m_security = addField(securityForm, tr("Encryption"));
    m_permissions = addField(securityForm, tr("Allowed"));

    layout->addStretch(1);

    connect(&Theme::instance(), &Theme::changed, this, [divider] {
        divider->setStyleSheet(QStringLiteral("color: %1;").arg(themePalette().border.name()));
    });

    refresh();
}

QLabel *PropertiesPanel::addField(QFormLayout *form, const QString &label)
{
    auto *caption = new QLabel(label, this);
    caption->setObjectName(QStringLiteral("SectionLabel"));

    auto *value = new QLabel(QStringLiteral("—"), this);
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(caption, value);
    return value;
}

void PropertiesPanel::setSession(core::DocumentSession *session)
{
    if (m_session == session)
        return;

    if (m_session)
        m_session->disconnect(this);

    m_session = session;
    if (m_session) {
        connect(m_session, &core::DocumentSession::documentChanged, this,
                &PropertiesPanel::refresh);
        connect(m_session, &core::DocumentSession::currentPageChanged, this,
                &PropertiesPanel::refresh);
    }
    refresh();
}

void PropertiesPanel::refresh()
{
    const auto document = m_session ? m_session->document() : nullptr;
    if (!document) {
        for (QLabel *field : { m_fileName, m_title, m_author, m_producer, m_format,
                               m_pageCount, m_pageSize, m_fileSize, m_security,
                               m_permissions }) {
            field->setText(QStringLiteral("—"));
        }
        return;
    }

    const core::DocumentInfo &info = document->info();
    m_fileName->setText(orDash(QFileInfo(document->filePath()).fileName()));
    m_title->setText(orDash(info.title));
    m_author->setText(orDash(info.author));
    m_producer->setText(orDash(info.producer));
    m_format->setText(orDash(info.format));

    m_pageCount->setText(QString::number(document->pageCount()));
    m_pageSize->setText(describePageSize(document->pageSizePoints(m_session->currentPage())));
    m_fileSize->setText(QLocale::system().formattedDataSize(info.fileSize));

    m_security->setText(document->isEncrypted() ? tr("Password protected") : tr("None"));

    const core::DocumentPermissions &allowed = document->permissions();
    QStringList granted;
    if (allowed.print)
        granted << tr("print");
    if (allowed.modify)
        granted << tr("edit");
    if (allowed.copy)
        granted << tr("copy");
    if (allowed.annotate)
        granted << tr("comment");
    if (allowed.form)
        granted << tr("fill forms");
    if (allowed.assemble)
        granted << tr("assemble");
    m_permissions->setText(granted.isEmpty() ? tr("nothing") : granted.join(QStringLiteral(", ")));
}

} // namespace sweet::ui
