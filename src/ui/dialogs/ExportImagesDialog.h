#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QToolButton;

namespace sweet::ui {

/// Export pages as image files: which pages, what format, and how sharp.
class ExportImagesDialog : public QDialog
{
    Q_OBJECT

public:
    ExportImagesDialog(int pageCount, int currentPage, const QString &suggestedName,
                       QWidget *parent = nullptr);

    QList<int> pages() const;
    QString directory() const;
    QString baseName() const;
    QString format() const;
    int dpi() const;

private:
    void chooseDirectory();
    void updateEstimate();

    int m_pageCount = 0;
    QComboBox *m_scope = nullptr;
    QLineEdit *m_range = nullptr;
    QLineEdit *m_directory = nullptr;
    QLineEdit *m_baseName = nullptr;
    QComboBox *m_format = nullptr;
    QSpinBox *m_dpi = nullptr;
    QLabel *m_estimate = nullptr;
    int m_currentPage = 0;
};

} // namespace sweet::ui
