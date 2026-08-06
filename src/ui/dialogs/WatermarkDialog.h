#pragma once

#include <QColor>
#include <QDialog>
#include <QList>

class QComboBox;
class QLineEdit;
class QSlider;
class QSpinBox;
class QToolButton;

namespace sweet::ui {

/// A text watermark or stamp across a range of pages.
class WatermarkDialog : public QDialog
{
    Q_OBJECT

public:
    WatermarkDialog(int pageCount, int currentPage, QWidget *parent = nullptr);

    QString text() const;
    QColor color() const;
    qreal opacity() const;
    int fontSize() const;

    /// Where on the page the text sits, as a fraction of width and height.
    enum class Position { Centre, Top, Bottom };
    Position position() const;

    QList<int> pages() const;

private:
    void pickColour();

    int m_pageCount = 0;
    int m_currentPage = 0;
    QLineEdit *m_text = nullptr;
    QToolButton *m_colourButton = nullptr;
    QColor m_colour = QColor(0xD0, 0x21, 0x21);
    QSlider *m_opacity = nullptr;
    QSpinBox *m_fontSize = nullptr;
    QComboBox *m_position = nullptr;
    QComboBox *m_scope = nullptr;
    QLineEdit *m_range = nullptr;
};

} // namespace sweet::ui
