#include "ui/dialogs/CompareDialog.h"

#include "ui/theme/Theme.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace sweet::ui {
namespace {

QPixmap pageShot(const sweet::core::PdfDocument &document, int pageIndex, int width)
{
    auto page = document.page(pageIndex);
    if (!page)
        return {};
    const QSizeF points = page->sizePoints();
    if (points.width() < 1)
        return {};
    const qreal scale = width / points.width();
    return QPixmap::fromImage(page->render(scale));
}

QString pageWords(const sweet::core::PdfDocument &document, int pageIndex)
{
    auto page = document.page(pageIndex);
    if (!page || !page->textLayer())
        return {};
    return page->textLayer()->plainText().simplified();
}

} // namespace

CompareDialog::CompareDialog(const std::shared_ptr<sweet::core::PdfDocument> &left, QWidget *parent)
    : QDialog(parent)
    , m_left(left)
{
    setWindowTitle(tr("Compare documents"));
    resize(860, 640);

    auto *column = new QVBoxLayout(this);
    auto *bar = new QHBoxLayout;
    auto *open = new QPushButton(tr("Choose other PDF…"), this);
    connect(open, &QPushButton::clicked, this, &CompareDialog::chooseOther);
    bar->addWidget(open);
    bar->addStretch(1);
    bar->addWidget(new QLabel(tr("Page"), this));
    m_page = new QSpinBox(this);
    m_page->setRange(1, qMax(1, m_left ? m_left->pageCount() : 1));
    connect(m_page, &QSpinBox::valueChanged, this, [this](int) { render(); });
    bar->addWidget(m_page);
    column->addLayout(bar);

    auto *pages = new QHBoxLayout;
    m_leftView = new QLabel(this);
    m_rightView = new QLabel(tr("Choose another PDF to compare."), this);
    m_leftView->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_rightView->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_leftView->setMinimumSize(380, 480);
    m_rightView->setMinimumSize(380, 480);
    pages->addWidget(m_leftView, 1);
    pages->addWidget(m_rightView, 1);
    column->addLayout(pages, 1);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    column->addWidget(m_status);
    render();
}

void CompareDialog::chooseOther()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Document to compare"), QString(),
                                                      tr("PDF documents (*.pdf)"));
    if (path.isEmpty())
        return;
    try {
        m_right = sweet::core::PdfDocument::open(path);
    } catch (const std::exception &failure) {
        m_status->setText(QString::fromUtf8(failure.what()));
        m_right.reset();
        return;
    }
    const int pages = qMax(m_left ? m_left->pageCount() : 1, m_right->pageCount());
    m_page->setMaximum(pages);
    render();
}

void CompareDialog::render()
{
    const int index = m_page->value() - 1;
    if (m_left && index < m_left->pageCount())
        m_leftView->setPixmap(pageShot(*m_left, index, 400));
    else
        m_leftView->setText(tr("This document has no page %1.").arg(index + 1));

    if (!m_right) {
        m_status->setText(tr("The left page is the document you have open."));
        return;
    }
    if (index < m_right->pageCount())
        m_rightView->setPixmap(pageShot(*m_right, index, 400));
    else {
        m_rightView->setPixmap({});
        m_rightView->setText(tr("The other document ends before this page."));
    }

    const bool leftHas = m_left && index < m_left->pageCount();
    const bool rightHas = index < m_right->pageCount();
    if (leftHas && rightHas) {
        const QString a = pageWords(*m_left, index);
        const QString b = pageWords(*m_right, index);
        m_status->setText(a == b ? tr("The words on this page match.")
                                 : tr("The words on this page differ."));
        m_status->setStyleSheet(QStringLiteral("color: %1;")
                                    .arg(a == b ? themePalette().textMuted.name()
                                                : themePalette().markupPink.name()));
    } else {
        m_status->setText(tr("The documents do not have the same number of pages."));
    }
}

} // namespace sweet::ui
