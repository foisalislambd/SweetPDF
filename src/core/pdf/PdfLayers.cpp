#include "core/pdf/PdfDocument.h"

#include "core/pdf/MuContext.h"
#include "core/util/Logging.h"

namespace sweet::core {
namespace {

LayerConfig readConfig(fz_context *ctx, pdf_document *pdf, int index)
{
    LayerConfig config;
    config.index = index;
    fz_try(ctx) {
        if (const char *name = pdf_layer_config_name(ctx, pdf, index))
            config.name = QString::fromUtf8(name);
    }
    fz_catch(ctx)
        config.name.clear();
    if (config.name.isEmpty())
        config.name = QStringLiteral("Configuration %1").arg(index + 1);
    return config;
}

OptionalLayer readLayer(fz_context *ctx, pdf_document *pdf, int index)
{
    OptionalLayer layer;
    layer.index = index;
    fz_try(ctx)
    {
        if (const char *name = pdf_layer_name(ctx, pdf, index))
            layer.name = QString::fromUtf8(name);
        layer.enabled = pdf_layer_is_enabled(ctx, pdf, index) != 0;
    }
    fz_catch(ctx)
    {
        qCWarning(lcPdf) << "Could not read layer" << index << ":" << MuContext::lastError(ctx);
        layer.index = -1;
    }
    if (layer.name.isEmpty() && layer.index >= 0)
        layer.name = QStringLiteral("Layer %1").arg(index + 1);
    return layer;
}

} // namespace

QList<OptionalLayer> PdfDocument::layers() const
{
    if (!m_pdf)
        return {};

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(m_parseMutex);

    QList<OptionalLayer> layers;
    int count = 0;
    fz_var(count);
    fz_try(ctx)
        count = pdf_count_layers(ctx, m_pdf);
    fz_catch(ctx)
    {
        qCWarning(lcPdf) << "Could not read optional content:" << MuContext::lastError(ctx);
        return {};
    }

    for (int index = 0; index < count; ++index) {
        OptionalLayer layer = readLayer(ctx, m_pdf, index);
        if (layer.index >= 0)
            layers.append(std::move(layer));
    }
    return layers;
}

QList<LayerConfig> PdfDocument::layerConfigs() const
{
    if (!m_pdf)
        return {};

    fz_context *ctx = MuContext::forThread();
    const std::lock_guard<std::mutex> guard(m_parseMutex);

    int count = 0;
    fz_var(count);
    fz_try(ctx)
        count = pdf_count_layer_configs(ctx, m_pdf);
    fz_catch(ctx)
    {
        qCWarning(lcPdf) << "Could not read layer configurations:" << MuContext::lastError(ctx);
        return {};
    }

    QList<LayerConfig> configs;
    for (int index = 0; index < count; ++index)
        configs.append(readConfig(ctx, m_pdf, index));
    return configs;
}

bool PdfDocument::selectLayerConfig(int index)
{
    if (!m_pdf || index < 0)
        return false;

    ensureStructure();
    fz_context *ctx = MuContext::forThread();
    bool applied = false;
    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        int count = 0;
        fz_var(count);
        fz_var(applied);
        fz_try(ctx) {
            count = pdf_count_layer_configs(ctx, m_pdf);
            if (index < count) {
                pdf_select_layer_config(ctx, m_pdf, index);
                applied = true;
            }
        }
        fz_catch(ctx) {
            qCWarning(lcPdf) << "Could not apply layer configuration" << index << ":"
                             << MuContext::lastError(ctx);
            return false;
        }
    }
    if (!applied)
        return false;
    markEdited();
    discardPageCaches(-1);
    return true;
}

bool PdfDocument::setLayerEnabled(int index, bool enabled)
{
    if (!m_pdf || index < 0)
        return false;

    ensureStructure();

    fz_context *ctx = MuContext::forThread();
    bool applied = false;
    {
        const std::lock_guard<std::mutex> guard(m_parseMutex);
        int count = 0;
        fz_var(count);
        fz_var(applied);
        fz_try(ctx)
        {
            count = pdf_count_layers(ctx, m_pdf);
            if (index < count) {
                pdf_enable_layer(ctx, m_pdf, index, enabled ? 1 : 0);
                applied = true;
            }
        }
        fz_catch(ctx)
        {
            qCWarning(lcPdf) << "Could not change layer" << index << ":"
                             << MuContext::lastError(ctx);
            return false;
        }
    }

    if (!applied)
        return false;

    markEdited();
    discardPageCaches(-1);
    return true;
}

} // namespace sweet::core
