#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

namespace sweet::ui {

/// Header, footer and page numbers, written as text boxes on the chosen pages.
class HeaderFooterDialog : public QDialog
{
    Q_OBJECT

public:
    HeaderFooterDialog(int pageCount, int currentPage, QWidget *parent = nullptr);

    QString headerText() const;
    QString footerText() const;
    bool pageNumbers() const;
    bool batesNumbers() const;
    QString batesPrefix() const;
    int batesStart() const;
    int batesDigits() const;
    int fontSize() const;
    /// True when every page is included. False means the current page only.
    bool allPages() const;

private:
    QLineEdit *m_header = nullptr;
    QLineEdit *m_footer = nullptr;
    QCheckBox *m_numbers = nullptr;
    QCheckBox *m_bates = nullptr;
    QLineEdit *m_batesPrefix = nullptr;
    QSpinBox *m_batesStart = nullptr;
    QSpinBox *m_batesDigits = nullptr;
    QSpinBox *m_fontSize = nullptr;
    QComboBox *m_scope = nullptr;
};

} // namespace sweet::ui
