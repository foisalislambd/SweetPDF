#include "ui/window/WelcomeView.h"

#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

namespace sweet::ui {
namespace {

constexpr int contentWidth = 1080;
constexpr int guideWidth = 320;
constexpr int recentLimit = 12;
constexpr int tileColumns = 4;

/// Labels and glyphs remember which palette entry they were built from, so a
/// theme flip is a walk over the children instead of rebuilding the page.
enum class Ink { Text, Muted, Accent, Inverted };

const char *const inkProperty = "sweetInk";
const char *const glyphProperty = "sweetGlyph";
const char *const glyphSizeProperty = "sweetGlyphSize";

QColor inkColor(Ink ink)
{
    switch (ink) {
    case Ink::Muted:
        return themePalette().textMuted;
    case Ink::Accent:
        return themePalette().accent;
    case Ink::Inverted:
        return themePalette().textInverted;
    case Ink::Text:
        break;
    }
    return themePalette().text;
}

/// A clickable panel. QPushButton cannot be used for the tiles and the recent
/// rows: it sizes itself from its own text and ignores a child layout, which
/// leaves multi-line cards collapsed. A frame with a click handler lays out
/// normally and still picks up hover styling from the stylesheet.
class Card : public QFrame
{
public:
    Card(const QString &name, std::function<void()> onClick, QWidget *parent)
        : QFrame(parent)
        , m_onClick(std::move(onClick))
    {
        setObjectName(name);
        setAttribute(Qt::WA_StyledBackground, true);
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())
            && m_onClick) {
            m_onClick();
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    std::function<void()> m_onClick;
};

/// The tasks the home screen offers. Every id is either an action id owned by
/// MainWindow or a composite task it recognises, so the tiles stay declarative.
struct Task
{
    const char *id;
    const char *iconName;
    const char *title;
    const char *blurb;
};

const Task tasks[] = {
    { "open", "open", QT_TRANSLATE_NOOP("WelcomeView", "Open a PDF"),
      QT_TRANSLATE_NOOP("WelcomeView", "Browse your computer") },
    { "editText", "edit-text", QT_TRANSLATE_NOOP("WelcomeView", "Edit text"),
      QT_TRANSLATE_NOOP("WelcomeView", "Replace words on the page") },
    { "organise", "pages", QT_TRANSLATE_NOOP("WelcomeView", "Organise pages"),
      QT_TRANSLATE_NOOP("WelcomeView", "Reorder, rotate, delete, extract") },
    { "sign", "sign", QT_TRANSLATE_NOOP("WelcomeView", "Fill and sign"),
      QT_TRANSLATE_NOOP("WelcomeView", "Forms and signatures") },
    { "merge", "merge", QT_TRANSLATE_NOOP("WelcomeView", "Combine files"),
      QT_TRANSLATE_NOOP("WelcomeView", "Merge several PDFs into one") },
    { "exportImages", "export", QT_TRANSLATE_NOOP("WelcomeView", "Export"),
      QT_TRANSLATE_NOOP("WelcomeView", "Pages as images, text or HTML") },
    { "ocr", "ocr", QT_TRANSLATE_NOOP("WelcomeView", "Scan and OCR"),
      QT_TRANSLATE_NOOP("WelcomeView", "Make a scan searchable") },
    { "encrypt", "lock", QT_TRANSLATE_NOOP("WelcomeView", "Protect"),
      QT_TRANSLATE_NOOP("WelcomeView", "Passwords and permissions") },
};

/// First steps, one line each so the card stays scannable.
const char *const steps[] = {
    QT_TRANSLATE_NOOP("WelcomeView", "Open a PDF, or drop one anywhere on this window."),
    QT_TRANSLATE_NOOP("WelcomeView", "Use the ribbon tabs for markup, pages, editing and tools."),
    QT_TRANSLATE_NOOP("WelcomeView",
                      "The left panel holds thumbnails, bookmarks, comments and forms."),
    QT_TRANSLATE_NOOP("WelcomeView", "Everything is undoable until you save, so experiment."),
    QT_TRANSLATE_NOOP("WelcomeView", "Saving writes real PDF objects that any viewer can read."),
};

struct Shortcut
{
    const char *what;
    const char *keys;
};

const Shortcut shortcuts[] = {
    { QT_TRANSLATE_NOOP("WelcomeView", "Open"), "Ctrl+O" },
    { QT_TRANSLATE_NOOP("WelcomeView", "Save"), "Ctrl+S" },
    { QT_TRANSLATE_NOOP("WelcomeView", "Find"), "Ctrl+F" },
    { QT_TRANSLATE_NOOP("WelcomeView", "Undo"), "Ctrl+Z" },
    { QT_TRANSLATE_NOOP("WelcomeView", "Fit page"), "Ctrl+1" },
    { QT_TRANSLATE_NOOP("WelcomeView", "Navigation panel"), "Ctrl+B" },
};

QLabel *makeLabel(const QString &text, TextRole role, Ink ink, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setFont(themeFont(role));
    label->setProperty(inkProperty, static_cast<int>(ink));
    label->setStyleSheet(QStringLiteral("color: %1;").arg(inkColor(ink).name()));
    // Cards are clickable; their text must not swallow the click.
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    return label;
}

QLabel *makeSectionLabel(const QString &text, QWidget *parent)
{
    return makeLabel(text, TextRole::SectionLabel, Ink::Muted, parent);
}

QLabel *makeGlyph(const QString &name, Ink ink, int size, QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setProperty(inkProperty, static_cast<int>(ink));
    label->setProperty(glyphProperty, name);
    label->setProperty(glyphSizeProperty, size);
    label->setPixmap(Theme::instance().icon(name, inkColor(ink)).pixmap(size, size));
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    label->setFixedSize(size, size);
    return label;
}

/// The brand mark, which is artwork rather than themed iconography.
QLabel *makeBrandMark(int size, QWidget *parent)
{
    auto *label = new QLabel(parent);
    const QPixmap art(QStringLiteral(":/icons/org.sweetpdf.SweetPDF.png"));
    label->setPixmap(art.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    label->setFixedSize(size, size);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    return label;
}

QString describeSize(qint64 bytes)
{
    return QLocale::system().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

/// Relative for the last week, absolute after that: "3 days ago" is easier to
/// place than a date, but only while the date is still recent.
QString describeWhen(const QDateTime &when)
{
    const qint64 days = when.daysTo(QDateTime::currentDateTime());
    if (days <= 0) {
        return WelcomeView::tr("Today %1")
            .arg(QLocale::system().toString(when.time(), QLocale::ShortFormat));
    }
    if (days == 1)
        return WelcomeView::tr("Yesterday");
    if (days < 7)
        return WelcomeView::tr("%n days ago", nullptr, static_cast<int>(days));
    return QLocale::system().toString(when.date(), QLocale::ShortFormat);
}

} // namespace

WelcomeView::WelcomeView(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    // The page grows with the recent list, so it scrolls rather than squeezing
    // its cards on a short window.
    auto *scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(scroll);

    auto *page = new QWidget(scroll);
    scroll->setWidget(page);

    auto *centering = new QHBoxLayout(page);
    centering->setContentsMargins(40, 34, 40, 40);
    centering->addStretch(1);

    auto *column = new QWidget(page);
    column->setMaximumWidth(contentWidth);
    auto *layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(28);
    // The column keeps the maximum width above; the spacers only take what is
    // left over on a wide window, which is what centres it.
    centering->addWidget(column, 10);
    centering->addStretch(1);

    buildHeader(layout);
    buildTiles(layout);

    auto *lower = new QHBoxLayout;
    lower->setSpacing(24);
    lower->addWidget(buildRecentSection(), 1);
    lower->addWidget(buildGuideSection(), 0, Qt::AlignTop);
    layout->addLayout(lower, 1);
    layout->addStretch(1);

    connect(&Theme::instance(), &Theme::changed, this, &WelcomeView::retint);
}

void WelcomeView::buildHeader(QVBoxLayout *column)
{
    auto *header = new QWidget(this);
    auto *row = new QHBoxLayout(header);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(16);
    row->addWidget(makeBrandMark(60, header), 0, Qt::AlignTop);

    auto *text = new QVBoxLayout;
    text->setSpacing(2);

    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);
    titleRow->addWidget(makeLabel(tr("SweetPDF"), TextRole::Display, Ink::Text, header));

    const QString version = QApplication::applicationVersion();
    if (!version.isEmpty() && version != QLatin1String("0.0.0")) {
        auto *badge = makeLabel(version, TextRole::Caption, Ink::Accent, header);
        badge->setObjectName(QStringLiteral("VersionBadge"));
        titleRow->addWidget(badge, 0, Qt::AlignVCenter);
    }
    titleRow->addStretch(1);
    text->addLayout(titleRow);

    auto *tagline = makeLabel(tr("Read, annotate, edit and organise PDF documents. Everything "
                                 "happens on this computer - nothing is uploaded anywhere."),
                              TextRole::Body, Ink::Muted, header);
    tagline->setWordWrap(true);
    text->addWidget(tagline);
    row->addLayout(text, 1);

    // The ribbon is hidden on this screen, so the two window-level commands that
    // still make sense without a document get a home here.
    auto *controls = new QVBoxLayout;
    controls->setSpacing(8);

    auto *open = new QPushButton(icon(QStringLiteral("open")), tr("Open PDF"), header);
    open->setProperty("accent", true);
    open->setMinimumWidth(168);
    open->setToolTip(tr("Open a document (Ctrl+O)"));
    connect(open, &QPushButton::clicked, this, &WelcomeView::openRequested);
    controls->addWidget(open);

    auto *minor = new QHBoxLayout;
    minor->setSpacing(6);

    auto *theme = new QPushButton(icon(QStringLiteral("moon")), tr("Theme"), header);
    theme->setProperty("flat", true);
    theme->setToolTip(tr("Switch between the light and dark theme"));
    connect(theme, &QPushButton::clicked, this,
            [this] { Q_EMIT toolRequested(QStringLiteral("toggleTheme")); });
    minor->addWidget(theme);

    auto *settings = new QPushButton(icon(QStringLiteral("settings")), tr("Settings"), header);
    settings->setProperty("flat", true);
    connect(settings, &QPushButton::clicked, this,
            [this] { Q_EMIT toolRequested(QStringLiteral("settings")); });
    minor->addWidget(settings);
    controls->addLayout(minor);

    row->addLayout(controls, 0);
    column->addWidget(header);
}

void WelcomeView::buildTiles(QVBoxLayout *column)
{
    auto *section = new QWidget(this);
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(makeSectionLabel(tr("What would you like to do?"), section));

    m_tileGrid = new QGridLayout;
    m_tileGrid->setSpacing(12);
    layout->addLayout(m_tileGrid);

    int index = 0;
    for (const Task &task : tasks) {
        const QString id = QString::fromLatin1(task.id);

        auto *tile = new Card(QStringLiteral("WelcomeTile"), [this, id] {
            if (id == QLatin1String("open"))
                Q_EMIT openRequested();
            else
                Q_EMIT toolRequested(id);
        }, section);
        tile->setToolTip(tr(task.blurb));

        auto *inner = new QVBoxLayout(tile);
        inner->setContentsMargins(16, 15, 16, 15);
        inner->setSpacing(8);
        inner->addWidget(makeGlyph(QString::fromLatin1(task.iconName), Ink::Accent, 24, tile));
        inner->addWidget(makeLabel(tr(task.title), TextRole::Body, Ink::Text, tile));

        auto *blurb = makeLabel(tr(task.blurb), TextRole::Caption, Ink::Muted, tile);
        blurb->setWordWrap(true);
        inner->addWidget(blurb);
        inner->addStretch(1);

        m_tileGrid->addWidget(tile, index / tileColumns, index % tileColumns);
        ++index;
    }
    for (int c = 0; c < tileColumns; ++c)
        m_tileGrid->setColumnStretch(c, 1);

    column->addWidget(section);
}

QWidget *WelcomeView::buildRecentSection()
{
    auto *section = new QWidget(this);
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *header = new QHBoxLayout;
    header->setSpacing(10);
    header->addWidget(makeSectionLabel(tr("Recent"), section));
    header->addStretch(1);

    m_filter = new QLineEdit(section);
    m_filter->setPlaceholderText(tr("Filter by name"));
    m_filter->setClearButtonEnabled(true);
    m_filter->setFixedWidth(190);
    m_filter->addAction(icon(QStringLiteral("search")), QLineEdit::LeadingPosition);
    connect(m_filter, &QLineEdit::textChanged, this, [this] { rebuildRecentRows(); });
    header->addWidget(m_filter);

    auto *clear = new QPushButton(tr("Clear list"), section);
    clear->setProperty("flat", true);
    connect(clear, &QPushButton::clicked, this, &WelcomeView::clearRecentRequested);
    header->addWidget(clear);
    layout->addLayout(header);

    m_recentRows = new QWidget(section);
    m_recentLayout = new QVBoxLayout(m_recentRows);
    m_recentLayout->setContentsMargins(0, 0, 0, 0);
    m_recentLayout->setSpacing(6);
    layout->addWidget(m_recentRows);

    // Shown instead of the list while there is nothing to list. It also advertises
    // that dropping a file works, which is otherwise invisible.
    m_dropZone = new QWidget(section);
    m_dropZone->setObjectName(QStringLiteral("WelcomeDropZone"));
    m_dropZone->setAttribute(Qt::WA_StyledBackground, true);
    auto *dropLayout = new QVBoxLayout(m_dropZone);
    dropLayout->setContentsMargins(24, 36, 24, 36);
    dropLayout->setSpacing(10);
    dropLayout->addWidget(makeGlyph(QStringLiteral("file-pdf"), Ink::Muted, 32, m_dropZone), 0,
                          Qt::AlignHCenter);

    auto *dropTitle = makeLabel(tr("Drop a PDF here"), TextRole::Title, Ink::Text, m_dropZone);
    dropTitle->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(dropTitle);

    auto *dropHint = makeLabel(tr("Documents you open will be listed here."), TextRole::Small,
                               Ink::Muted, m_dropZone);
    dropHint->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(dropHint);
    layout->addWidget(m_dropZone);

    m_recentSummary = makeLabel(QString(), TextRole::Caption, Ink::Muted, section);
    layout->addWidget(m_recentSummary);
    layout->addStretch(1);

    rebuildRecentRows();
    return section;
}

QWidget *WelcomeView::buildGuideSection()
{
    auto *section = new QWidget(this);
    section->setFixedWidth(guideWidth);
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(makeSectionLabel(tr("Getting started"), section));

    auto *stepsCard = new QWidget(section);
    stepsCard->setObjectName(QStringLiteral("WelcomePanel"));
    stepsCard->setAttribute(Qt::WA_StyledBackground, true);
    auto *stepLayout = new QVBoxLayout(stepsCard);
    stepLayout->setContentsMargins(18, 16, 18, 16);
    stepLayout->setSpacing(11);

    int number = 1;
    for (const char *step : steps) {
        auto *row = new QHBoxLayout;
        row->setSpacing(10);

        auto *badge = makeLabel(QString::number(number++), TextRole::Caption, Ink::Inverted,
                                stepsCard);
        badge->setObjectName(QStringLiteral("StepBadge"));
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedSize(20, 20);
        row->addWidget(badge, 0, Qt::AlignTop);

        auto *text = makeLabel(tr(step), TextRole::Small, Ink::Text, stepsCard);
        text->setWordWrap(true);
        row->addWidget(text, 1);
        stepLayout->addLayout(row);
    }
    layout->addWidget(stepsCard);

    auto *keys = new QWidget(section);
    keys->setObjectName(QStringLiteral("WelcomePanel"));
    keys->setAttribute(Qt::WA_StyledBackground, true);
    auto *keyLayout = new QVBoxLayout(keys);
    keyLayout->setContentsMargins(18, 16, 18, 16);
    keyLayout->setSpacing(10);

    auto *keyHeader = new QHBoxLayout;
    keyHeader->setSpacing(8);
    keyHeader->addWidget(makeGlyph(QStringLiteral("keyboard"), Ink::Muted, 16, keys));
    keyHeader->addWidget(makeLabel(tr("Handy shortcuts"), TextRole::Body, Ink::Text, keys), 1);
    keyLayout->addLayout(keyHeader);

    for (const Shortcut &shortcut : shortcuts) {
        auto *row = new QHBoxLayout;
        row->addWidget(makeLabel(tr(shortcut.what), TextRole::Small, Ink::Muted, keys), 1);

        auto *chip = makeLabel(QString::fromLatin1(shortcut.keys), TextRole::Caption, Ink::Text,
                               keys);
        chip->setObjectName(QStringLiteral("KeyChip"));
        row->addWidget(chip, 0, Qt::AlignRight);
        keyLayout->addLayout(row);
    }

    auto *guide = new QPushButton(icon(QStringLiteral("info")), tr("Read the full guide"), keys);
    guide->setProperty("flat", true);
    connect(guide, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://github.com/sweetpdf/sweetpdf#readme")));
    });
    keyLayout->addWidget(guide);

    layout->addWidget(keys);
    layout->addStretch(1);
    return section;
}

void WelcomeView::setRecentFiles(const QStringList &paths)
{
    m_recentFiles = paths;
    rebuildRecentRows();
}

void WelcomeView::rebuildRecentRows()
{
    if (!m_recentLayout)
        return;

    QLayoutItem *item = nullptr;
    while ((item = m_recentLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    const QString needle = m_filter ? m_filter->text().trimmed() : QString();

    QStringList shown;
    int existing = 0;
    for (const QString &path : m_recentFiles) {
        const QFileInfo info(path);
        if (!info.exists())
            continue;
        ++existing;
        if (!needle.isEmpty() && !info.fileName().contains(needle, Qt::CaseInsensitive))
            continue;
        if (shown.size() < recentLimit)
            shown.append(path);
    }

    m_dropZone->setVisible(existing == 0);
    m_recentRows->setVisible(!shown.isEmpty());

    for (const QString &path : shown) {
        const QFileInfo info(path);

        auto *row = new Card(QStringLiteral("WelcomeCard"),
                             [this, path] { Q_EMIT fileRequested(path); }, m_recentRows);
        row->setToolTip(path);

        auto *grid = new QGridLayout(row);
        grid->setContentsMargins(14, 11, 14, 11);
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(1);
        grid->addWidget(makeGlyph(QStringLiteral("file-pdf"), Ink::Accent, 22, row), 0, 0, 2, 1);
        grid->addWidget(makeLabel(info.fileName(), TextRole::Body, Ink::Text, row), 0, 1);

        auto *where = makeLabel(info.absolutePath(), TextRole::Caption, Ink::Muted, row);
        where->setMinimumWidth(0);
        grid->addWidget(where, 1, 1);
        grid->setColumnStretch(1, 1);

        auto *when = makeLabel(describeWhen(info.lastModified()), TextRole::Small, Ink::Muted, row);
        when->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(when, 0, 2);

        auto *size = makeLabel(describeSize(info.size()), TextRole::Caption, Ink::Muted, row);
        size->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(size, 1, 2);

        row->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(row, &QWidget::customContextMenuRequested, this,
                [this, row, path](const QPoint &at) {
                    QMenu menu(this);
                    connect(menu.addAction(tr("Open")), &QAction::triggered, this,
                            [this, path] { Q_EMIT fileRequested(path); });
                    connect(menu.addAction(tr("Copy path")), &QAction::triggered, this,
                            [path] { QApplication::clipboard()->setText(path); });
                    connect(menu.addAction(tr("Show in folder")), &QAction::triggered, this,
                            [path] {
                                QDesktopServices::openUrl(
                                    QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
                            });
                    menu.addSeparator();
                    connect(menu.addAction(tr("Remove from list")), &QAction::triggered, this,
                            [this, path] { Q_EMIT forgetRequested(path); });
                    menu.exec(row->mapToGlobal(at));
                });

        m_recentLayout->addWidget(row);
    }

    if (existing == 0)
        m_recentSummary->clear();
    else if (shown.size() < existing)
        m_recentSummary->setText(tr("Showing %1 of %2 documents").arg(shown.size()).arg(existing));
    else if (existing == 1)
        m_recentSummary->setText(tr("1 document"));
    else
        m_recentSummary->setText(tr("%1 documents").arg(existing));
}

void WelcomeView::retint()
{
    for (QLabel *label : findChildren<QLabel *>()) {
        const QVariant ink = label->property(inkProperty);
        if (!ink.isValid())
            continue;

        const QColor color = inkColor(static_cast<Ink>(ink.toInt()));
        const QVariant glyph = label->property(glyphProperty);
        if (glyph.isValid()) {
            const int size = label->property(glyphSizeProperty).toInt();
            label->setPixmap(Theme::instance().icon(glyph.toString(), color).pixmap(size, size));
        } else {
            label->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
        }
    }
}

} // namespace sweet::ui
