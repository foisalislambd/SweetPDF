#include "ui/panels/LayersPanel.h"

#include "core/pdf/PdfDocument.h"
#include "core/session/DocumentSession.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QStackedLayout>
#include <QVBoxLayout>

namespace sweet::ui {

LayersPanel::LayersPanel(QWidget *parent)
    : QWidget(parent)
{
    m_stack = new QStackedLayout(this);
    m_stack->setContentsMargins(0, 0, 0, 0);

    auto *page = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    m_configs = new QComboBox(page);
    m_configs->setVisible(false);
    pageLayout->addWidget(m_configs);
    connect(m_configs, &QComboBox::activated, this, [this](int row) {
        if (!m_session || m_writing || row < 0)
            return;
        m_writing = true;
        m_configRow = row;
        const int index = m_configs->itemData(row).toInt();
        const bool applied = m_session->selectLayerConfig(index);
        m_writing = false;
        if (!applied)
            m_configRow = -1;
        rebuild();
    });

    m_summary = new QLabel(page);
    m_summary->setObjectName(QStringLiteral("SectionLabel"));
    m_summary->setContentsMargins(12, 8, 12, 4);
    m_summary->setWordWrap(true);
    pageLayout->addWidget(m_summary);

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_list = new QWidget(scroll);
    m_listLayout = new QVBoxLayout(m_list);
    m_listLayout->setContentsMargins(12, 4, 12, 12);
    m_listLayout->setSpacing(6);
    m_listLayout->addStretch(1);
    scroll->setWidget(m_list);
    pageLayout->addWidget(scroll, 1);
    m_stack->addWidget(page);

    auto *empty = new QWidget(this);
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(22, 32, 22, 22);
    emptyLayout->setSpacing(10);
    emptyLayout->addStretch(1);

    auto *glyph = new QLabel(empty);
    glyph->setAlignment(Qt::AlignCenter);
    glyph->setPixmap(Theme::instance()
                         .icon(QStringLiteral("layers"), Theme::instance().palette().textMuted)
                         .pixmap(32, 32));
    emptyLayout->addWidget(glyph);

    auto *heading = new QLabel(tr("No layers"), empty);
    heading->setObjectName(QStringLiteral("PanelTitle"));
    heading->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(heading);

    auto *hint = new QLabel(tr("Optional content groups in the PDF are listed here. "
                               "Hiding one redraws the pages."),
                            empty);
    hint->setObjectName(QStringLiteral("SectionLabel"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    emptyLayout->addWidget(hint);
    emptyLayout->addStretch(2);
    m_stack->addWidget(empty);

    rebuild();
}

void LayersPanel::setSession(core::DocumentSession *session)
{
    if (m_session)
        m_session->disconnect(this);
    m_session = session;
    if (m_session) {
        connect(m_session, &core::DocumentSession::documentChanged, this, &LayersPanel::rebuild);
    }
    rebuild();
}

void LayersPanel::rebuild()
{
    while (QLayoutItem *item = m_listLayout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }
    m_listLayout->addStretch(1);

    QList<core::LayerConfig> configs;
    QList<core::OptionalLayer> layers;
    if (m_session && m_session->isLoaded()) {
        configs = m_session->document()->layerConfigs();
        layers = m_session->document()->layers();
    }

    const QSignalBlocker blocker(m_configs);
    m_configs->clear();
    for (const core::LayerConfig &config : configs)
        m_configs->addItem(config.name, config.index);
    m_configs->setVisible(!configs.isEmpty());
    if (m_configRow >= 0 && m_configRow < m_configs->count())
        m_configs->setCurrentIndex(m_configRow);

    for (const core::OptionalLayer &layer : layers) {
        auto *box = new QCheckBox(layer.name, m_list);
        box->setChecked(layer.enabled);
        const int index = layer.index;
        connect(box, &QCheckBox::toggled, this, [this, index](bool enabled) {
            if (!m_session || m_writing)
                return;
            m_writing = true;
            m_session->setLayerEnabled(index, enabled);
            m_writing = false;
        });
        m_listLayout->insertWidget(m_listLayout->count() - 1, box);
    }

    m_summary->setText(tr("%n layer(s)", nullptr, layers.size()));
    m_stack->setCurrentIndex(layers.isEmpty() ? 1 : 0);
}

} // namespace sweet::ui
