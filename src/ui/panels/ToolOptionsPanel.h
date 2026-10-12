#pragma once

#include "core/session/DocumentSession.h"

#include <QColor>
#include <QList>
#include <QWidget>

class QLabel;
class QSlider;
class QToolButton;
class QVBoxLayout;

namespace sweet::ui {

/// Settings for the tool that is currently picked.
///
/// Sits at the top of the right-hand inspector so that choosing a tool in the
/// ribbon and adjusting it happen in one place, instead of the ribbon having to
/// carry a style group that means nothing for most tools. Only the controls the
/// active tool actually uses are shown, and tools with no settings hide it
/// entirely rather than showing an empty box.
class ToolOptionsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ToolOptionsPanel(QWidget *parent = nullptr);

    void setSession(sweet::core::DocumentSession *session);

    /// Whether the current tool has anything to configure. False for Select and
    /// Hand, which are pure navigation.
    bool hasOptions() const noexcept { return m_hasOptions; }

Q_SIGNALS:
    /// A command the panel offers on the tool's behalf, by action id, so the
    /// window stays the only place that knows what commands do.
    void commandRequested(const QString &actionId);

    /// Emitted when the visible controls change, because the inspector decides
    /// whether to show this panel at all.
    void optionsChanged();

private:
    void rebuild();
    void applyStyleFromSession();
    void retint();

    QWidget *buildColourRow(QWidget *parent);
    QWidget *buildOpacityRow(QWidget *parent);
    QWidget *buildBorderWidthRow(QWidget *parent);
    QWidget *buildFillRow(QWidget *parent);
    QWidget *buildFontSizeRow(QWidget *parent);
    QWidget *buildShapePresets(QWidget *parent);
    QWidget *buildArrowEndingRow(QWidget *parent);
    QWidget *buildStampPresets(QWidget *parent);
    QWidget *buildCommandButton(const QString &actionId, const QString &text,
                                const QString &iconName, QWidget *parent);

    sweet::core::DocumentSession *m_session = nullptr;
    sweet::core::Tool m_tool = sweet::core::Tool::Select;
    bool m_hasOptions = false;

    QLabel *m_icon = nullptr;
    QLabel *m_name = nullptr;
    QLabel *m_hint = nullptr;
    QVBoxLayout *m_body = nullptr;
    QSlider *m_opacity = nullptr;
    QLabel *m_opacityValue = nullptr;
    QSlider *m_borderWidth = nullptr;
    QLabel *m_borderWidthValue = nullptr;
    QSlider *m_fontSize = nullptr;
    QLabel *m_fontSizeValue = nullptr;
    QList<QToolButton *> m_swatches;
    QList<QToolButton *> m_fillSwatches;
    QList<QToolButton *> m_endingButtons;
    QList<QToolButton *> m_stampButtons;
};

} // namespace sweet::ui
