#include "ui/panels/PropertiesPanel.h"

#include "core/pdf/PdfAnnotations.h"
#include "core/pdf/PdfDocument.h"
#include "core/session/DocumentSession.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QFileInfo>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
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
    return value.trimmed().isEmpty() ? QStringLiteral("—") : value;
}

QString humanPdfDate(const QString &raw)
{
    QString body = raw.trimmed();
    if (body.startsWith(QLatin1String("D:")))
        body = body.mid(2);

    QString digits;
    for (const QChar character : body) {
        if (!character.isDigit())
            break;
        digits += character;
    }
    if (digits.size() < 8)
        return raw.trimmed().isEmpty() ? QStringLiteral("—") : raw;

    const QDate date(digits.mid(0, 4).toInt(), digits.mid(4, 2).toInt(), digits.mid(6, 2).toInt());
    if (!date.isValid())
        return raw;
    if (digits.size() >= 12) {
        const QTime time(digits.mid(8, 2).toInt(), digits.mid(10, 2).toInt(),
                         digits.size() >= 14 ? digits.mid(12, 2).toInt() : 0);
        if (time.isValid())
            return QLocale::system().toString(QDateTime(date, time), QLocale::ShortFormat);
    }
    return QLocale::system().toString(date, QLocale::ShortFormat);
}

QString paperName(const QSizeF &points)
{
    if (points.isEmpty())
        return QStringLiteral("—");

    const struct {
        const char *name;
        qreal width;
        qreal height;
    } papers[] = {
        { "A3", 841.89, 1190.55 }, { "A4", 595.28, 841.89 }, { "A5", 419.53, 595.28 },
        { "Letter", 612.0, 792.0 }, { "Legal", 612.0, 1008.0 }, { "Tabloid", 792.0, 1224.0 },
        { "Executive", 522.0, 756.0 },
    };

    const qreal width = points.width();
    const qreal height = points.height();
    for (const auto &paper : papers) {
        const bool portrait = qAbs(width - paper.width) < 6.0 && qAbs(height - paper.height) < 6.0;
        const bool landscape = qAbs(width - paper.height) < 6.0 && qAbs(height - paper.width) < 6.0;
        if (portrait)
            return QObject::tr("%1, portrait").arg(QString::fromLatin1(paper.name));
        if (landscape)
            return QObject::tr("%1, landscape").arg(QString::fromLatin1(paper.name));
    }
    return QObject::tr("Custom");
}

QString millimetres(const QSizeF &points)
{
    if (points.isEmpty())
        return QStringLiteral("—");
    constexpr qreal millimetresPerPoint = 25.4 / 72.0;
    return QStringLiteral("%1 × %2 mm")
        .arg(QString::number(points.width() * millimetresPerPoint, 'f', 1),
             QString::number(points.height() * millimetresPerPoint, 'f', 1));
}

QString pointsLabel(const QSizeF &points)
{
    if (points.isEmpty())
        return QStringLiteral("—");
    return QStringLiteral("%1 × %2 pt")
        .arg(QString::number(points.width(), 'f', 1), QString::number(points.height(), 'f', 1));
}

void setPermission(QLabel *label, bool allowed)
{
    label->setObjectName(allowed ? QStringLiteral("PermOn") : QStringLiteral("PermOff"));
    label->setText(allowed ? QObject::tr("Allowed") : QObject::tr("Blocked"));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

QPixmap colorWell(const QColor &color)
{
    QPixmap pixmap(QSize(18, 18) * 2);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(themePalette().borderStrong, 1));
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(1, 1, 16, 16), 3, 3);
    return pixmap;
}

QScrollArea *scrolling(QWidget *body, QWidget *parent)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setWidget(body);
    return scroll;
}

QFormLayout *makeForm()
{
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(7);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return form;
}

QLabel *addValue(QFormLayout *form, QWidget *parent, const QString &caption)
{
    auto *label = new QLabel(caption, parent);
    label->setObjectName(QStringLiteral("FieldCaption"));
    auto *value = new QLabel(QStringLiteral("—"), parent);
    value->setObjectName(QStringLiteral("FieldValue"));
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(label, value);
    return value;
}

QLineEdit *addEditor(QFormLayout *form, QWidget *parent, const QString &caption)
{
    auto *label = new QLabel(caption, parent);
    label->setObjectName(QStringLiteral("FieldCaption"));
    auto *editor = new QLineEdit(parent);
    editor->setClearButtonEnabled(true);
    form->addRow(label, editor);
    return editor;
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
    header->setFixedHeight(28);
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 12, 0);
    auto *title = new QLabel(tr("Properties"), header);
    title->setObjectName(QStringLiteral("PanelTitle"));
    headerLayout->addWidget(title);
    outer->addWidget(header);

    auto *tabBar = new QWidget(this);
    tabBar->setObjectName(QStringLiteral("PanelHeader"));
    auto *tabs = new QHBoxLayout(tabBar);
    tabs->setContentsMargins(6, 0, 6, 0);
    tabs->setSpacing(0);
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);

    const QStringList names = { tr("File"), tr("Page"), tr("Selection") };
    for (int index = 0; index < names.size(); ++index) {
        auto *tab = new QToolButton(tabBar);
        tab->setObjectName(QStringLiteral("InspectorTab"));
        tab->setText(names.at(index));
        tab->setCheckable(true);
        tab->setAutoRaise(true);
        tab->setToolButtonStyle(Qt::ToolButtonTextOnly);
        tab->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        tab->setFocusPolicy(Qt::TabFocus);
        group->addButton(tab, index);
        tabs->addWidget(tab);
        m_tabs.append(tab);
    }
    m_tabs.first()->setChecked(true);
    connect(group, &QButtonGroup::idClicked, this,
            [this](int id) { showPane(static_cast<Pane>(id)); });
    outer->addWidget(tabBar);

    m_panes = new QStackedWidget(this);
    outer->addWidget(m_panes, 1);

    // --- File --------------------------------------------------------------
    auto *fileBody = new QWidget(m_panes);
    auto *fileColumn = new QVBoxLayout(fileBody);
    fileColumn->setContentsMargins(12, 12, 12, 16);
    fileColumn->setSpacing(8);

    fileColumn->addWidget(sectionLabel(tr("File"), fileBody));
    auto *fileForm = makeForm();
    fileColumn->addLayout(fileForm);
    m_fileName = addValue(fileForm, fileBody, tr("Name"));
    m_location = addValue(fileForm, fileBody, tr("Location"));
    m_fileSize = addValue(fileForm, fileBody, tr("Size"));
    m_format = addValue(fileForm, fileBody, tr("Format"));
    m_pageCount = addValue(fileForm, fileBody, tr("Pages"));
    m_saved = addValue(fileForm, fileBody, tr("Status"));

    fileColumn->addWidget(sectionLabel(tr("Description"), fileBody));
    auto *hint = new QLabel(
        tr("Title, author, subject and keywords are stored in the PDF and saved with the file."),
        fileBody);
    hint->setObjectName(QStringLiteral("InspectorHint"));
    hint->setWordWrap(true);
    fileColumn->addWidget(hint);

    m_metaEditors = new QWidget(fileBody);
    auto *metaForm = makeForm();
    auto *metaLayout = new QVBoxLayout(m_metaEditors);
    metaLayout->setContentsMargins(0, 0, 0, 0);
    metaLayout->addLayout(metaForm);
    m_title = addEditor(metaForm, m_metaEditors, tr("Title"));
    m_author = addEditor(metaForm, m_metaEditors, tr("Author"));
    m_subject = addEditor(metaForm, m_metaEditors, tr("Subject"));
    m_keywords = addEditor(metaForm, m_metaEditors, tr("Keywords"));
    m_keywords->setPlaceholderText(tr("Comma-separated"));
    fileColumn->addWidget(m_metaEditors);

    auto *readForm = makeForm();
    fileColumn->addLayout(readForm);
    m_creator = addValue(readForm, fileBody, tr("Creator"));
    m_producer = addValue(readForm, fileBody, tr("Producer"));
    m_created = addValue(readForm, fileBody, tr("Created"));
    m_modified = addValue(readForm, fileBody, tr("Modified"));

    m_metaStatus = new QLabel(fileBody);
    m_metaStatus->setObjectName(QStringLiteral("PropStatus"));
    m_metaStatus->setWordWrap(true);
    fileColumn->addWidget(m_metaStatus);

    fileColumn->addWidget(sectionLabel(tr("Security"), fileBody));
    auto *securityForm = makeForm();
    fileColumn->addLayout(securityForm);
    m_encryption = addValue(securityForm, fileBody, tr("Encryption"));
    m_permPrint = addValue(securityForm, fileBody, tr("Print"));
    m_permEdit = addValue(securityForm, fileBody, tr("Edit"));
    m_permCopy = addValue(securityForm, fileBody, tr("Copy text"));
    m_permComment = addValue(securityForm, fileBody, tr("Comment"));
    m_permForm = addValue(securityForm, fileBody, tr("Fill forms"));
    m_permAssemble = addValue(securityForm, fileBody, tr("Assemble"));
    fileColumn->addStretch(1);
    m_panes->addWidget(scrolling(fileBody, m_panes));

    for (QLineEdit *editor : { m_title, m_author, m_subject, m_keywords }) {
        connect(editor, &QLineEdit::editingFinished, this, &PropertiesPanel::commitMetadata);
    }

    // --- Page --------------------------------------------------------------
    auto *pageBody = new QWidget(m_panes);
    auto *pageColumn = new QVBoxLayout(pageBody);
    pageColumn->setContentsMargins(12, 12, 12, 16);
    pageColumn->setSpacing(8);
    pageColumn->addWidget(sectionLabel(tr("Current page"), pageBody));
    auto *pageForm = makeForm();
    pageColumn->addLayout(pageForm);
    m_pageNumber = addValue(pageForm, pageBody, tr("Page"));
    m_paper = addValue(pageForm, pageBody, tr("Paper"));
    m_pageMillimetres = addValue(pageForm, pageBody, tr("Size"));
    m_pagePoints = addValue(pageForm, pageBody, tr("Points"));
    m_pageRotation = addValue(pageForm, pageBody, tr("Page rotation"));
    m_viewRotation = addValue(pageForm, pageBody, tr("View rotation"));
    auto *pageHint = new QLabel(
        tr("Page rotation is saved in the file. View rotation only changes how this window shows it."),
        pageBody);
    pageHint->setObjectName(QStringLiteral("InspectorHint"));
    pageHint->setWordWrap(true);
    pageColumn->addWidget(pageHint);
    pageColumn->addStretch(1);
    m_panes->addWidget(scrolling(pageBody, m_panes));

    // --- Selection ---------------------------------------------------------
    auto *selectionBody = new QWidget(m_panes);
    auto *selectionColumn = new QVBoxLayout(selectionBody);
    selectionColumn->setContentsMargins(12, 12, 12, 16);
    selectionColumn->setSpacing(8);

    m_selectionEmpty = new QLabel(
        tr("Nothing is selected.\n\nClick a comment, shape, stamp or text box. "
           "Its colour, opacity and note show up here."),
        selectionBody);
    m_selectionEmpty->setObjectName(QStringLiteral("InspectorHint"));
    m_selectionEmpty->setWordWrap(true);
    selectionColumn->addWidget(m_selectionEmpty);

    m_selectionBody = new QWidget(selectionBody);
    auto *selColumn = new QVBoxLayout(m_selectionBody);
    selColumn->setContentsMargins(0, 0, 0, 0);
    selColumn->setSpacing(8);
    selColumn->addWidget(sectionLabel(tr("General"), m_selectionBody));
    auto *general = makeForm();
    selColumn->addLayout(general);
    m_selType = addValue(general, m_selectionBody, tr("Type"));
    m_selPage = addValue(general, m_selectionBody, tr("Page"));
    m_selAuthor = addValue(general, m_selectionBody, tr("Author"));
    m_selModified = addValue(general, m_selectionBody, tr("Modified"));
    m_selExtra = addValue(general, m_selectionBody, tr("Detail"));

    selColumn->addWidget(sectionLabel(tr("Appearance"), m_selectionBody));
    auto *appearance = new QHBoxLayout;
    appearance->setSpacing(8);
    auto *colorCaption = new QLabel(tr("Colour"), m_selectionBody);
    colorCaption->setObjectName(QStringLiteral("FieldCaption"));
    appearance->addWidget(colorCaption);
    m_selColor = new QToolButton(m_selectionBody);
    m_selColor->setObjectName(QStringLiteral("ColourSwatch"));
    m_selColor->setAutoRaise(true);
    m_selColor->setToolTip(tr("Change colour"));
    m_selColor->setIconSize(QSize(18, 18));
    connect(m_selColor, &QToolButton::clicked, this, [this] {
        if (!m_session)
            return;
        const int page = m_session->selectedAnnotationPage();
        const int object = m_session->selectedAnnotation();
        core::Annotation current;
        bool found = false;
        for (const core::Annotation &annotation : m_session->annotationsOnPage(page)) {
            if (annotation.objectNumber == object) {
                current = annotation;
                found = true;
                break;
            }
        }
        if (!found || !current.editable)
            return;
        const QColor chosen = QColorDialog::getColor(current.color, this, tr("Colour"));
        if (!chosen.isValid())
            return;
        m_session->modifyAnnotation(current, chosen, current.opacity, current.contents);
    });
    appearance->addWidget(m_selColor);
    appearance->addStretch(1);
    selColumn->addLayout(appearance);

    auto *opacityRow = new QHBoxLayout;
    auto *opacityCaption = new QLabel(tr("Opacity"), m_selectionBody);
    opacityCaption->setObjectName(QStringLiteral("FieldCaption"));
    opacityRow->addWidget(opacityCaption, 1);
    m_selOpacityValue = new QLabel(m_selectionBody);
    m_selOpacityValue->setObjectName(QStringLiteral("FieldValue"));
    opacityRow->addWidget(m_selOpacityValue);
    selColumn->addLayout(opacityRow);
    m_selOpacity = new QSlider(Qt::Horizontal, m_selectionBody);
    m_selOpacity->setRange(5, 100);
    connect(m_selOpacity, &QSlider::valueChanged, this, [this](int value) {
        m_selOpacityValue->setText(tr("%1%").arg(value));
    });
    connect(m_selOpacity, &QSlider::sliderReleased, this, &PropertiesPanel::commitSelection);
    selColumn->addWidget(m_selOpacity);

    auto *borderForm = makeForm();
    selColumn->addLayout(borderForm);
    m_selBorder = addValue(borderForm, m_selectionBody, tr("Border"));

    auto *placeCaption = new QLabel(tr("POSITION (PT)"), m_selectionBody);
    placeCaption->setObjectName(QStringLiteral("SectionLabel"));
    selColumn->addWidget(placeCaption);
    auto *place = new QGridLayout;
    place->setHorizontalSpacing(8);
    place->setVerticalSpacing(4);
    const auto spin = [this](QDoubleSpinBox **target) {
        auto *box = new QDoubleSpinBox(m_selectionBody);
        box->setRange(-2000, 20000);
        box->setDecimals(1);
        box->setSuffix(QStringLiteral(" pt"));
        *target = box;
        return box;
    };
    auto *xCaption = new QLabel(tr("X"), m_selectionBody);
    xCaption->setObjectName(QStringLiteral("FieldCaption"));
    auto *yCaption = new QLabel(tr("Y"), m_selectionBody);
    yCaption->setObjectName(QStringLiteral("FieldCaption"));
    auto *wCaption = new QLabel(tr("W"), m_selectionBody);
    wCaption->setObjectName(QStringLiteral("FieldCaption"));
    auto *hCaption = new QLabel(tr("H"), m_selectionBody);
    hCaption->setObjectName(QStringLiteral("FieldCaption"));
    place->addWidget(xCaption, 0, 0);
    place->addWidget(spin(&m_selX), 0, 1);
    place->addWidget(yCaption, 0, 2);
    place->addWidget(spin(&m_selY), 0, 3);
    place->addWidget(wCaption, 1, 0);
    place->addWidget(spin(&m_selW), 1, 1);
    place->addWidget(hCaption, 1, 2);
    place->addWidget(spin(&m_selH), 1, 3);
    selColumn->addLayout(place);
    const auto commitBox = [this] {
        if (!m_session || m_writing || m_selX->hasFocus() || m_selY->hasFocus() || m_selW->hasFocus()
            || m_selH->hasFocus()) {
            return;
        }
        const QRectF rect(m_selX->value(), m_selY->value(), m_selW->value(), m_selH->value());
        if (rect.width() < 1 || rect.height() < 1)
            return;
        m_session->setAnnotationRect(m_session->selectedAnnotationPage(),
                                     m_session->selectedAnnotation(), rect);
    };
    for (QDoubleSpinBox *box : { m_selX, m_selY, m_selW, m_selH })
        connect(box, &QDoubleSpinBox::editingFinished, this, commitBox);

    selColumn->addWidget(sectionLabel(tr("Note"), m_selectionBody));
    m_selContents = new QPlainTextEdit(m_selectionBody);
    m_selContents->setPlaceholderText(tr("Comment text"));
    m_selContents->setFixedHeight(88);
    m_selContents->installEventFilter(this);
    selColumn->addWidget(m_selContents);

    m_selReply = new QPushButton(tr("Reply"), m_selectionBody);
    m_selReply->setIcon(icon(QStringLiteral("comment")));
    connect(m_selReply, &QPushButton::clicked, this,
            [this] { Q_EMIT commandRequested(QStringLiteral("replyAnnotation")); });
    selColumn->addWidget(m_selReply);

    auto *duplicate = new QPushButton(tr("Duplicate"), m_selectionBody);
    duplicate->setIcon(icon(QStringLiteral("duplicate")));
    duplicate->setProperty("sweetToolIcon", QStringLiteral("duplicate"));
    connect(duplicate, &QPushButton::clicked, this,
            [this] { Q_EMIT commandRequested(QStringLiteral("duplicateSelection")); });
    selColumn->addWidget(duplicate);

    m_selDelete = new QPushButton(tr("Delete selection"), m_selectionBody);
    m_selDelete->setIcon(icon(QStringLiteral("trash")));
    connect(m_selDelete, &QPushButton::clicked, this,
            [this] { Q_EMIT commandRequested(QStringLiteral("deleteAnnotation")); });
    selColumn->addWidget(m_selDelete);

    m_selStatus = new QLabel(m_selectionBody);
    m_selStatus->setObjectName(QStringLiteral("PropStatus"));
    m_selStatus->setWordWrap(true);
    selColumn->addWidget(m_selStatus);
    selectionColumn->addWidget(m_selectionBody);
    selectionColumn->addStretch(1);
    m_panes->addWidget(scrolling(selectionBody, m_panes));

    connect(&Theme::instance(), &Theme::changed, this, [this] {
        m_selDelete->setIcon(icon(QStringLiteral("trash")));
        m_selReply->setIcon(icon(QStringLiteral("comment")));
        for (QPushButton *button : findChildren<QPushButton *>()) {
            const QString name = button->property("sweetToolIcon").toString();
            if (!name.isEmpty())
                button->setIcon(icon(name));
        }
        refresh();
    });

    refresh();
}

bool PropertiesPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_selContents && event->type() == QEvent::FocusOut)
        commitSelection();
    return QWidget::eventFilter(watched, event);
}

void PropertiesPanel::setValue(QLabel *label, const QString &text)
{
    label->setText(text.isEmpty() ? QStringLiteral("—") : text);
}

void PropertiesPanel::setEditor(QLineEdit *editor, const QString &text)
{
    if (editor->hasFocus())
        return;
    if (editor->text() == text)
        return;
    const QSignalBlocker blocker(editor);
    editor->setText(text);
}

void PropertiesPanel::showPane(Pane pane)
{
    m_panes->setCurrentIndex(static_cast<int>(pane));
    if (QToolButton *tab = m_tabs.value(static_cast<int>(pane))) {
        const QSignalBlocker blocker(tab);
        tab->setChecked(true);
    }
}

void PropertiesPanel::setSession(core::DocumentSession *session)
{
    if (m_session == session)
        return;

    if (m_session)
        m_session->disconnect(this);

    m_session = session;
    m_shownObject = 0;
    if (m_session) {
        connect(m_session, &core::DocumentSession::documentChanged, this, &PropertiesPanel::refresh);
        connect(m_session, &core::DocumentSession::currentPageChanged, this,
                &PropertiesPanel::refresh);
        connect(m_session, &core::DocumentSession::rotationChanged, this, &PropertiesPanel::refresh);
        connect(m_session, &core::DocumentSession::modifiedChanged, this, &PropertiesPanel::refresh);
        connect(m_session, &core::DocumentSession::annotationsChanged, this,
                &PropertiesPanel::refresh);
        connect(m_session, &core::DocumentSession::selectedAnnotationChanged, this,
                [this](int, int objectNumber) {
                    const bool becameSelected = m_shownObject == 0 && objectNumber != 0;
                    refresh();
                    if (becameSelected) {
                        showPane(Pane::Selection);
                        Q_EMIT revealRequested();
                    }
                });
    }
    refresh();
}

void PropertiesPanel::commitMetadata()
{
    if (m_writing || !m_session || !m_session->document() || !m_session->document()->isPdf())
        return;

    core::DocumentInfo info = m_session->document()->info();
    const QString title = m_title->text().trimmed();
    const QString author = m_author->text().trimmed();
    const QString subject = m_subject->text().trimmed();
    const QString keywords = m_keywords->text().trimmed();
    if (info.title == title && info.author == author && info.subject == subject
        && info.keywords == keywords) {
        return;
    }

    info.title = title;
    info.author = author;
    info.subject = subject;
    info.keywords = keywords;

    m_writing = true;
    QString error;
    const bool ok = m_session->setDocumentInfo(info, &error);
    m_writing = false;
    m_metaStatus->setText(ok ? QString() : error);
    if (!ok)
        refresh();
}

void PropertiesPanel::commitSelection()
{
    if (m_writing || !m_session || m_session->selectedAnnotation() == 0)
        return;

    const int page = m_session->selectedAnnotationPage();
    const int object = m_session->selectedAnnotation();
    for (const core::Annotation &annotation : m_session->annotationsOnPage(page)) {
        if (annotation.objectNumber != object || !annotation.editable)
            return;
        const QString contents = m_selContents->toPlainText();
        const qreal opacity = m_selOpacity->value() / 100.0;
        if (annotation.contents == contents && qFuzzyCompare(annotation.opacity, opacity))
            return;
        m_writing = true;
        m_session->modifyAnnotation(annotation, annotation.color, opacity, contents);
        m_writing = false;
        return;
    }
}

void PropertiesPanel::refresh()
{
    const auto document = m_session ? m_session->document() : nullptr;
    if (!document) {
        for (QLabel *field : { m_fileName, m_location, m_fileSize, m_format, m_pageCount, m_saved,
                               m_creator, m_producer, m_created, m_modified, m_encryption,
                               m_pageNumber, m_paper, m_pageMillimetres, m_pagePoints,
                               m_pageRotation, m_viewRotation }) {
            setValue(field, QStringLiteral("—"));
        }
        for (QLabel *permission : { m_permPrint, m_permEdit, m_permCopy, m_permComment, m_permForm,
                                    m_permAssemble }) {
            setPermission(permission, false);
            permission->setText(QStringLiteral("—"));
        }
        for (QLineEdit *editor : { m_title, m_author, m_subject, m_keywords }) {
            editor->clear();
            editor->setEnabled(false);
        }
        m_selectionEmpty->setVisible(true);
        m_selectionBody->setVisible(false);
        m_shownObject = 0;
        return;
    }

    const core::DocumentInfo &info = document->info();
    const QFileInfo file(document->filePath());
    setValue(m_fileName, orDash(file.fileName()));
    setValue(m_location, orDash(file.absolutePath()));
    m_location->setToolTip(document->filePath());
    setValue(m_fileSize, QLocale::system().formattedDataSize(info.fileSize));
    setValue(m_format, orDash(info.format));
    setValue(m_pageCount, QString::number(document->pageCount()));
    setValue(m_saved, m_session->isModified() ? tr("Unsaved changes") : tr("Saved"));
    setValue(m_creator, orDash(info.creator));
    setValue(m_producer, orDash(info.producer));
    setValue(m_created, humanPdfDate(info.creationDate));
    setValue(m_modified, humanPdfDate(info.modificationDate));

    const bool pdf = document->isPdf();
    m_metaEditors->setEnabled(pdf);
    setEditor(m_title, info.title);
    setEditor(m_author, info.author);
    setEditor(m_subject, info.subject);
    setEditor(m_keywords, info.keywords);
    if (!pdf)
        m_metaStatus->setText(tr("This format can be read. Description is stored only in a PDF."));

    setValue(m_encryption, document->isEncrypted() ? tr("Password protected") : tr("None"));
    const core::DocumentPermissions &allowed = document->permissions();
    setPermission(m_permPrint, allowed.print);
    setPermission(m_permEdit, allowed.modify);
    setPermission(m_permCopy, allowed.copy);
    setPermission(m_permComment, allowed.annotate);
    setPermission(m_permForm, allowed.form);
    setPermission(m_permAssemble, allowed.assemble);

    const int pageIndex = m_session->currentPage();
    const QSizeF size = document->pageSizePoints(pageIndex);
    setValue(m_pageNumber, tr("%1 of %2").arg(pageIndex + 1).arg(document->pageCount()));
    setValue(m_paper, paperName(size));
    setValue(m_pageMillimetres, millimetres(size));
    setValue(m_pagePoints, pointsLabel(size));
    setValue(m_pageRotation, tr("%1°").arg(document->pageRotation(pageIndex)));
    setValue(m_viewRotation, tr("%1°").arg(m_session->rotation()));

    core::Annotation selected;
    bool hasSelection = false;
    if (m_session->selectedAnnotation() != 0) {
        const int selectedPage = m_session->selectedAnnotationPage();
        for (const core::Annotation &annotation : m_session->annotationsOnPage(selectedPage)) {
            if (annotation.objectNumber == m_session->selectedAnnotation()) {
                selected = annotation;
                hasSelection = true;
                break;
            }
        }
    }

    m_shownObject = hasSelection ? selected.objectNumber : 0;
    m_selectionEmpty->setVisible(!hasSelection);
    m_selectionBody->setVisible(hasSelection);
    if (!hasSelection)
        return;

    setValue(m_selType, core::annotations::displayName(selected.type));
    setValue(m_selPage, QString::number(selected.pageIndex + 1));
    setValue(m_selAuthor, orDash(selected.author));
    setValue(m_selModified, selected.modified.isValid()
                                ? QLocale::system().toString(selected.modified, QLocale::ShortFormat)
                                : QStringLiteral("—"));

    QString detail = QStringLiteral("—");
    if (selected.type == core::AnnotationType::Stamp && !selected.iconName.isEmpty())
        detail = selected.iconName;
    else if (selected.type == core::AnnotationType::Line)
        detail = tr("Line");
    setValue(m_selExtra, detail);
    setValue(m_selBorder, tr("%1 pt").arg(selected.borderWidth, 0, 'f', 1));
    if (!m_selX->hasFocus() && !m_selY->hasFocus() && !m_selW->hasFocus() && !m_selH->hasFocus()) {
        const QSignalBlocker blockX(m_selX);
        const QSignalBlocker blockY(m_selY);
        const QSignalBlocker blockW(m_selW);
        const QSignalBlocker blockH(m_selH);
        m_selX->setValue(selected.rect.x());
        m_selY->setValue(selected.rect.y());
        m_selW->setValue(selected.rect.width());
        m_selH->setValue(selected.rect.height());
    }
    const bool boxEditable = selected.editable && selected.type != core::AnnotationType::Ink
        && selected.type != core::AnnotationType::Highlight
        && selected.type != core::AnnotationType::Underline
        && selected.type != core::AnnotationType::StrikeOut
        && selected.type != core::AnnotationType::Squiggly;
    for (QDoubleSpinBox *box : { m_selX, m_selY, m_selW, m_selH })
        box->setEnabled(boxEditable);

    m_selColor->setIcon(colorWell(selected.color));
    m_selColor->setEnabled(selected.editable);
    m_selOpacity->setEnabled(selected.editable);
    m_selContents->setEnabled(selected.editable);
    m_selDelete->setEnabled(selected.editable);
    if (!m_selOpacity->isSliderDown()) {
        const QSignalBlocker blocker(m_selOpacity);
        m_selOpacity->setValue(qRound(selected.opacity * 100.0));
        m_selOpacityValue->setText(tr("%1%").arg(m_selOpacity->value()));
    }
    if (!m_selContents->hasFocus()) {
        const QSignalBlocker blocker(m_selContents);
        if (m_selContents->toPlainText() != selected.contents)
            m_selContents->setPlainText(selected.contents);
    }
    m_selStatus->setText(selected.editable
                             ? QString()
                             : tr("This item can be viewed here, and is edited from its own tool."));
}

} // namespace sweet::ui
