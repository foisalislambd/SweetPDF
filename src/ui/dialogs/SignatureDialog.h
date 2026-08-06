#pragma once

#include <QDialog>
#include <QImage>
#include <QPainterPath>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QTabWidget;

namespace sweet::ui {

/// A scratch pad the signature is drawn on with the mouse or a pen tablet.
///
/// Strokes are kept as paths rather than as pixels so the signature can be
/// rendered at whatever resolution the page needs, which is what keeps it from
/// looking like a screenshot once it is placed.
class SignaturePad : public QWidget
{
    Q_OBJECT

public:
    explicit SignaturePad(QWidget *parent = nullptr);

    bool isEmpty() const noexcept { return m_strokes.isEmpty(); }
    void clear();

    /// The drawing as a transparent image, cropped to the ink and rendered at
    /// `height` pixels tall. Returns a null image when nothing was drawn.
    QImage toImage(int height = 160) const;

    QSize minimumSizeHint() const override { return QSize(360, 140); }

Q_SIGNALS:
    void changed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QList<QPainterPath> m_strokes;
    bool m_drawing = false;
};

/// Creates a signature to stamp onto the page: drawn by hand, typed as a name,
/// or loaded from a picture of a real signature.
class SignatureDialog : public QDialog
{
    Q_OBJECT

public:
    /// `defaultName` seeds the typed tab, usually the document author.
    explicit SignatureDialog(const QString &defaultName, QWidget *parent = nullptr);

    /// The signature to place, transparent outside the ink. Null when nothing was
    /// drawn, typed or loaded.
    QImage signature() const;

    /// True when the signature should be added to every page rather than just the
    /// current one, which is what people want for initials.
    bool appliesToAllPages() const;

private:
    void loadFromFile();

    QTabWidget *m_tabs = nullptr;
    SignaturePad *m_pad = nullptr;
    QLineEdit *m_typed = nullptr;
    QLabel *m_preview = nullptr;
    QImage m_loaded;
    QCheckBox *m_allPages = nullptr;
};

} // namespace sweet::ui
