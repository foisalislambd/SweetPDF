#include "ui/dialogs/SignatureDialog.h"

#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

constexpr qreal penWidth = 2.6;

/// Renders text as a signature: an italic serif face at a size that fills the
/// image, on transparent background.
QImage typedSignature(const QString &text, int height)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return {};

    QFont font(QStringLiteral("Georgia"), height / 2);
    font.setStyleHint(QFont::Serif);
    font.setItalic(true);
    font.setPixelSize(int(height * 0.62));

    const QFontMetrics metrics(font);
    const QRect bounds = metrics.boundingRect(trimmed);
    const int margin = height / 8;

    QImage image(bounds.width() + margin * 2, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    painter.setPen(QColor(0x10, 0x1B, 0x3A));
    painter.drawText(image.rect(), Qt::AlignCenter, trimmed);
    return image;
}

/// Trims fully transparent borders so the signature sits tight in its box.
QImage cropToInk(const QImage &source)
{
    if (source.isNull())
        return source;

    int left = source.width();
    int right = -1;
    int top = source.height();
    int bottom = -1;
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            if (qAlpha(source.pixel(x, y)) == 0)
                continue;
            left = qMin(left, x);
            right = qMax(right, x);
            top = qMin(top, y);
            bottom = qMax(bottom, y);
        }
    }
    if (right < 0)
        return {};

    const int pad = 4;
    const QRect ink = QRect(QPoint(left, top), QPoint(right, bottom))
                          .adjusted(-pad, -pad, pad, pad)
                          .intersected(source.rect());
    return source.copy(ink);
}

} // namespace

// --- Pad -------------------------------------------------------------------

SignaturePad::SignaturePad(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("SignaturePad"));
    setCursor(Qt::CrossCursor);
    setMouseTracking(false);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void SignaturePad::clear()
{
    m_strokes.clear();
    update();
    Q_EMIT changed();
}

void SignaturePad::paintEvent(QPaintEvent *)
{
    const Palette &palette = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.surface);
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

    // A baseline, so the signature is written on a line like on paper.
    QPen guide(palette.border);
    guide.setStyle(Qt::DashLine);
    painter.setPen(guide);
    const int baseline = int(height() * 0.72);
    painter.drawLine(16, baseline, width() - 16, baseline);

    QPen ink(palette.text);
    ink.setWidthF(penWidth);
    ink.setCapStyle(Qt::RoundCap);
    ink.setJoinStyle(Qt::RoundJoin);
    painter.setPen(ink);
    painter.setBrush(Qt::NoBrush);
    for (const QPainterPath &stroke : m_strokes)
        painter.drawPath(stroke);
}

void SignaturePad::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    QPainterPath stroke;
    stroke.moveTo(event->position());
    m_strokes.append(stroke);
    m_drawing = true;
    update();
}

void SignaturePad::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_drawing || m_strokes.isEmpty())
        return;

    m_strokes.last().lineTo(event->position());
    update();
}

void SignaturePad::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_drawing) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_drawing = false;
    // A single click leaves a dot-less path; drop it rather than keep an empty stroke.
    if (m_strokes.last().elementCount() < 2)
        m_strokes.removeLast();
    Q_EMIT changed();
    update();
}

QImage SignaturePad::toImage(int height) const
{
    if (m_strokes.isEmpty())
        return {};

    // Render at the requested height and let the crop decide the width.
    const qreal scale = qreal(height) / qMax(1, this->height());
    QImage image(int(width() * scale), height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(scale, scale);
    QPen ink(QColor(0x10, 0x1B, 0x3A));
    ink.setWidthF(penWidth);
    ink.setCapStyle(Qt::RoundCap);
    ink.setJoinStyle(Qt::RoundJoin);
    painter.setPen(ink);
    for (const QPainterPath &stroke : m_strokes)
        painter.drawPath(stroke);
    painter.end();

    return cropToInk(image);
}

// --- Dialog ----------------------------------------------------------------

SignatureDialog::SignatureDialog(const QString &defaultName, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Sign document"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 14);
    layout->setSpacing(12);

    auto *hint = new QLabel(tr("Draw your signature, type your name, or use a picture of a "
                               "signature you already have."),
                            this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs, 1);

    // Draw
    auto *drawPage = new QWidget(m_tabs);
    auto *drawLayout = new QVBoxLayout(drawPage);
    drawLayout->setContentsMargins(0, 10, 0, 0);
    m_pad = new SignaturePad(drawPage);
    drawLayout->addWidget(m_pad, 1);
    auto *clear = new QPushButton(tr("Clear"), drawPage);
    clear->setIcon(icon(QStringLiteral("trash")));
    connect(clear, &QPushButton::clicked, m_pad, &SignaturePad::clear);
    drawLayout->addWidget(clear, 0, Qt::AlignRight);
    m_tabs->addTab(drawPage, icon(QStringLiteral("draw")), tr("Draw"));

    // Type
    auto *typePage = new QWidget(m_tabs);
    auto *typeLayout = new QVBoxLayout(typePage);
    typeLayout->setContentsMargins(0, 10, 0, 0);
    m_typed = new QLineEdit(defaultName, typePage);
    m_typed->setPlaceholderText(tr("Your name"));
    typeLayout->addWidget(m_typed);

    auto *typedPreview = new QLabel(typePage);
    typedPreview->setMinimumHeight(90);
    typedPreview->setAlignment(Qt::AlignCenter);
    typedPreview->setObjectName(QStringLiteral("SignaturePreview"));
    typeLayout->addWidget(typedPreview, 1);

    const auto refreshTyped = [this, typedPreview] {
        const QImage rendered = cropToInk(typedSignature(m_typed->text(), 80));
        typedPreview->setPixmap(QPixmap::fromImage(rendered));
    };
    connect(m_typed, &QLineEdit::textChanged, this, refreshTyped);
    refreshTyped();
    m_tabs->addTab(typePage, icon(QStringLiteral("add-text")), tr("Type"));

    // Image
    auto *imagePage = new QWidget(m_tabs);
    auto *imageLayout = new QVBoxLayout(imagePage);
    imageLayout->setContentsMargins(0, 10, 0, 0);
    m_preview = new QLabel(tr("No picture chosen"), imagePage);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumHeight(90);
    m_preview->setObjectName(QStringLiteral("SignaturePreview"));
    imageLayout->addWidget(m_preview, 1);
    auto *choose = new QPushButton(tr("Choose picture…"), imagePage);
    choose->setIcon(icon(QStringLiteral("add-image")));
    connect(choose, &QPushButton::clicked, this, &SignatureDialog::loadFromFile);
    imageLayout->addWidget(choose, 0, Qt::AlignRight);
    m_tabs->addTab(imagePage, icon(QStringLiteral("add-image")), tr("Picture"));

    m_allPages = new QCheckBox(tr("Place on every page"), this);
    layout->addWidget(m_allPages);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Place signature"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void SignatureDialog::loadFromFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose a signature picture"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
    if (path.isEmpty())
        return;

    QImage loaded(path);
    if (loaded.isNull()) {
        m_preview->setText(tr("That file is not a picture SweetPDF can read."));
        return;
    }

    // Scans of signatures come on white paper; drop the white so only ink lands
    // on the page.
    QImage keyed = loaded.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < keyed.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(keyed.scanLine(y));
        for (int x = 0; x < keyed.width(); ++x) {
            const QRgb pixel = line[x];
            const int lightness = qGray(pixel);
            if (lightness > 235)
                line[x] = qRgba(0, 0, 0, 0);
            else if (lightness > 160)
                line[x] = qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), 255 - lightness);
        }
    }

    m_loaded = cropToInk(keyed);
    m_preview->setPixmap(QPixmap::fromImage(
        m_loaded.scaledToHeight(80, Qt::SmoothTransformation)));
}

QImage SignatureDialog::signature() const
{
    switch (m_tabs->currentIndex()) {
    case 0:
        return m_pad->toImage(180);
    case 1:
        return cropToInk(typedSignature(m_typed->text(), 180));
    default:
        return m_loaded;
    }
}

bool SignatureDialog::appliesToAllPages() const
{
    return m_allPages->isChecked();
}

} // namespace sweet::ui
