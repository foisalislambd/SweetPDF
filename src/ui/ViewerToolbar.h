#pragma once

#include <QWidget>

class QComboBox;
class QToolButton;

namespace sweet::ui {

class PageView;

/// Floating bar over the page canvas with the controls needed while reading:
/// pointer mode, zoom and page layout. Keeping them here means they stay within
/// a short mouse travel of the page, instead of at the top of a ribbon.
class ViewerToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit ViewerToolbar(PageView *view);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void reposition();
    void syncZoomBox(qreal zoom);
    QToolButton *addButton(const QString &iconName, const QString &tooltip, bool checkable);

    PageView *m_view = nullptr;
    QComboBox *m_zoomBox = nullptr;
    bool m_syncingZoom = false;
};

} // namespace sweet::ui
