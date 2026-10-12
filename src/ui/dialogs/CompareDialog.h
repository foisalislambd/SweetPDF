#pragma once

#include "core/pdf/PdfDocument.h"

#include <QDialog>

#include <memory>

class QLabel;
class QSpinBox;

namespace sweet::ui {

/// Shows one page of the open document beside the same page of another file.
class CompareDialog : public QDialog
{
    Q_OBJECT

public:
    CompareDialog(const std::shared_ptr<sweet::core::PdfDocument> &left, QWidget *parent = nullptr);

private:
    void chooseOther();
    void render();

    std::shared_ptr<sweet::core::PdfDocument> m_left;
    std::shared_ptr<sweet::core::PdfDocument> m_right;
    QSpinBox *m_page = nullptr;
    QLabel *m_leftView = nullptr;
    QLabel *m_rightView = nullptr;
    QLabel *m_status = nullptr;
};

} // namespace sweet::ui
