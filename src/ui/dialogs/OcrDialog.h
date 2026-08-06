#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace sweet::ui {

/// Text recognition settings, for turning a scan into a searchable PDF.
class OcrDialog : public QDialog
{
    Q_OBJECT

public:
    OcrDialog(const QString &suggestedOutput, QWidget *parent = nullptr);

    QString language() const;
    QString outputPath() const;
    int dpi() const;

private:
    void chooseOutput();

    QComboBox *m_language = nullptr;
    QLineEdit *m_output = nullptr;
    QSpinBox *m_dpi = nullptr;
    QLabel *m_hint = nullptr;
};

} // namespace sweet::ui
