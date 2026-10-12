#include "core/pdf/PdfPage.h"

#include "core/pdf/Geometry.h"
#include "core/pdf/MuContext.h"
#include "core/util/Logging.h"

#include <QTransform>

#include <cmath>

namespace sweet::core {
namespace {

/// Text extraction options. Dehyphenation is what MuPDF 1.28 improved: a hyphen
/// at the end of a line stays a soft hyphen so search and copy can join the word.
/// Ligatures are expanded (the default) so "ﬁ" is three characters, which is
/// also more reliable from 1.28 on.
fz_stext_options textOptions()
{
    fz_stext_options options{};
    options.flags = FZ_STEXT_DEHYPHENATE | FZ_STEXT_ACCURATE_BBOXES;
    return options;
}

int collectMatch(fz_context *, void *opaque, int numQuads, fz_quad *hitQuads, int, int)
{
    auto *hits = static_cast<QList<QList<QRectF>> *>(opaque);
    if (hits->size() >= 500)
        return 1;
    QList<QRectF> boxes;
    boxes.reserve(numQuads);
    for (int index = 0; index < numQuads; ++index)
        boxes.append(toQRectF(hitQuads[index]));
    hits->append(std::move(boxes));
    return 0;
}

fz_matrix renderMatrix(qreal scale, int rotation)
{
    const float s = static_cast<float>(scale);
    return fz_pre_rotate(fz_scale(s, s), static_cast<float>(rotation));
}

/// Copies a MuPDF pixmap into a QImage that owns its own buffer.
QImage pixmapToImage(fz_context *ctx, fz_pixmap *pixmap)
{
    const int width = fz_pixmap_width(ctx, pixmap);
    const int height = fz_pixmap_height(ctx, pixmap);
    if (width <= 0 || height <= 0)
        return {};

    const int components = fz_pixmap_components(ctx, pixmap);
    const int stride = fz_pixmap_stride(ctx, pixmap);
    const unsigned char *samples = fz_pixmap_samples(ctx, pixmap);

    const QImage::Format format = components == 4 ? QImage::Format_ARGB32_Premultiplied
                                                 : QImage::Format_RGB888;
    return QImage(samples, width, height, stride, format).copy();
}

} // namespace

PdfPage::PdfPage(fz_document *document, int index, std::mutex &parseMutex)
    : m_document(document)
    , m_parseMutex(&parseMutex)
    , m_index(index)
{
    const std::lock_guard<std::mutex> guard(parseMutex);
    fz_context *ctx = MuContext::forThread();

    fz_page *page = nullptr;
    fz_rect bounds = fz_empty_rect;
    // fz_var keeps these readable after a longjmp out of the fz_try block.
    fz_var(page);
    fz_var(bounds);
    fz_try(ctx) {
        page = fz_load_page(ctx, document, index);
        bounds = fz_bound_page(ctx, page);
    }
    fz_catch(ctx) {
        // Load can succeed and bound can fail; drop the page before raising so
        // we do not leak the fz_page that MuPDF already handed us.
        fz_drop_page(ctx, page);
        MuContext::raise(ctx, QStringLiteral("Could not load page %1").arg(index + 1));
    }

    m_page = page;
    m_bounds = bounds;
    m_sizePoints = sizeOf(bounds);
}

PdfPage::~PdfPage()
{
    const std::lock_guard<std::mutex> guard(*m_parseMutex);
    fz_context *ctx = MuContext::forThread();
    if (m_displayList) {
        fz_drop_display_list(ctx, m_displayList);
        m_displayList = nullptr;
    }
    if (m_page) {
        fz_drop_page(ctx, m_page);
        m_page = nullptr;
    }
}

QSizeF PdfPage::sizePoints(int rotation) const
{
    const int normalised = ((rotation % 360) + 360) % 360;
    if (normalised == 90 || normalised == 270)
        return QSizeF(m_sizePoints.height(), m_sizePoints.width());
    return m_sizePoints;
}

fz_display_list *PdfPage::displayList(fz_context *ctx) const
{
    const std::lock_guard<std::mutex> guard(*m_parseMutex);
    if (!m_displayList) {
        fz_display_list *list = nullptr;
        fz_device *device = nullptr;
        fz_var(list);
        fz_var(device);
        fz_try(ctx) {
            list = fz_new_display_list(ctx, m_bounds);
            device = fz_new_list_device(ctx, list);
            fz_run_page(ctx, m_page, device, fz_identity, nullptr);
            fz_close_device(ctx, device);
        }
        fz_always(ctx)
            fz_drop_device(ctx, device);
        fz_catch(ctx) {
            fz_drop_display_list(ctx, list);
            MuContext::raise(ctx, QStringLiteral("Could not build the display list for page %1")
                                      .arg(m_index + 1));
        }
        m_displayList = list;
    }

    // Hand out a kept reference so callers can release the parse lock and still
    // use the list while another thread may discardDisplayList() the cache.
    return fz_keep_display_list(ctx, m_displayList);
}

QTransform PdfPage::pageToImage(const QSizeF &sizePoints, qreal scale, int rotation)
{
    const fz_matrix ctm = renderMatrix(scale, rotation);
    const fz_rect box = fz_transform_rect(
        fz_make_rect(0, 0, static_cast<float>(sizePoints.width()),
                     static_cast<float>(sizePoints.height())),
        ctm);
    const fz_irect pixels = fz_round_rect(box);
    return toQTransform(fz_concat(
        ctm, fz_translate(static_cast<float>(-pixels.x0), static_cast<float>(-pixels.y0))));
}

QImage PdfPage::render(qreal scale, int rotation) const
{
    if (scale <= 0.0)
        return {};

    fz_context *ctx = MuContext::forThread();
    fz_display_list *list = displayList(ctx);

    const fz_matrix ctm = renderMatrix(scale, rotation);
    const fz_irect bbox = fz_round_rect(fz_transform_rect(m_bounds, ctm));

    fz_pixmap *pixmap = nullptr;
    fz_device *device = nullptr;
    fz_var(pixmap);
    fz_var(device);
    QImage image;
    try {
        fz_try(ctx) {
            pixmap = fz_new_pixmap_with_bbox(ctx, fz_device_bgr(ctx), bbox, nullptr, 1);
            fz_clear_pixmap_with_value(ctx, pixmap, 0xff);
            device = fz_new_draw_device(ctx, fz_identity, pixmap);
            fz_run_display_list(ctx, list, device, ctm, fz_infinite_rect, nullptr);
            fz_close_device(ctx, device);
        }
        fz_always(ctx)
            fz_drop_device(ctx, device);
        fz_catch(ctx) {
            fz_drop_pixmap(ctx, pixmap);
            pixmap = nullptr;
            MuContext::raise(ctx, QStringLiteral("Could not render page %1").arg(m_index + 1));
        }

        image = pixmapToImage(ctx, pixmap);
        fz_drop_pixmap(ctx, pixmap);
        pixmap = nullptr;
    } catch (...) {
        fz_drop_display_list(ctx, list);
        throw;
    }
    fz_drop_display_list(ctx, list);
    return image;
}

QString PdfPage::text() const
{
    fz_context *ctx = MuContext::forThread();
    fz_display_list *list = displayList(ctx);

    const fz_stext_options options = textOptions();
    fz_stext_page *stext = nullptr;
    fz_var(stext);
    QString result;
    try {
        fz_try(ctx)
            stext = fz_new_stext_page_from_display_list(ctx, list, &options);
        fz_catch(ctx)
            MuContext::raise(ctx,
                             QStringLiteral("Could not extract text from page %1").arg(m_index + 1));

        for (fz_stext_block *block = stext->first_block; block; block = block->next) {
            if (block->type != FZ_STEXT_BLOCK_TEXT)
                continue;
            for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
                for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
                    const char32_t codepoint = static_cast<char32_t>(ch->c);
                    result += QString::fromUcs4(&codepoint, 1);
                }
                result += u'\n';
            }
            result += u'\n';
        }
    } catch (...) {
        fz_drop_stext_page(ctx, stext);
        fz_drop_display_list(ctx, list);
        throw;
    }
    fz_drop_stext_page(ctx, stext);
    fz_drop_display_list(ctx, list);
    return result;
}

std::shared_ptr<const PageText> PdfPage::textLayer() const
{
    {
        const std::lock_guard<std::mutex> guard(*m_parseMutex);
        if (m_textLayer)
            return m_textLayer;
    }

    fz_context *ctx = MuContext::forThread();
    fz_display_list *list = displayList(ctx);

    const fz_stext_options options = textOptions();
    fz_stext_page *stext = nullptr;
    fz_var(stext);
    try {
        fz_try(ctx)
            stext = fz_new_stext_page_from_display_list(ctx, list, &options);
        fz_catch(ctx) {
            fz_drop_display_list(ctx, list);
            qCWarning(lcPdf) << "Could not extract text from page" << m_index + 1 << ":"
                             << MuContext::lastError(ctx);
            auto empty = std::make_shared<const PageText>(QString(), QList<QRectF>(),
                                                          QList<TextRange>());
            const std::lock_guard<std::mutex> guard(*m_parseMutex);
            m_textLayer = empty;
            return empty;
        }
    } catch (...) {
        fz_drop_display_list(ctx, list);
        throw;
    }

    QString text;
    QList<QRectF> boxes;
    QList<TextRange> lines;

    for (fz_stext_block *block = stext->first_block; block; block = block->next) {
        if (block->type != FZ_STEXT_BLOCK_TEXT)
            continue;

        for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
            const int lineStart = int(text.size());
            for (fz_stext_char *character = line->first_char; character;
                 character = character->next) {
                const char32_t codepoint = static_cast<char32_t>(character->c);
                const QString glyph = QString::fromUcs4(&codepoint, 1);
                const QRectF box = toQRectF(character->quad);
                // A codepoint outside the BMP becomes a surrogate pair, so every
                // unit of it shares the glyph box and indices stay aligned.
                for (int unit = 0; unit < glyph.size(); ++unit)
                    boxes.append(box);
                text += glyph;
            }

            if (int(text.size()) > lineStart) {
                lines.append({ lineStart, int(text.size()) });
                text += u'\n';
                boxes.append(QRectF());
            }
        }
    }

    fz_drop_stext_page(ctx, stext);
    fz_drop_display_list(ctx, list);

    auto built = std::make_shared<const PageText>(std::move(text), std::move(boxes),
                                                  std::move(lines));
    const std::lock_guard<std::mutex> guard(*m_parseMutex);
    m_textLayer = built;
    return built;
}

void PdfPage::discardTextLayer()
{
    const std::lock_guard<std::mutex> guard(*m_parseMutex);
    m_textLayer.reset();
}

void PdfPage::discardDisplayList()
{
    const std::lock_guard<std::mutex> guard(*m_parseMutex);
    if (m_displayList) {
        fz_drop_display_list(MuContext::forThread(), m_displayList);
        m_displayList = nullptr;
    }
    m_textLayer.reset();
}

void PdfPage::refreshBounds()
{
    const std::lock_guard<std::mutex> guard(*m_parseMutex);
    fz_context *ctx = MuContext::forThread();
    if (m_displayList) {
        fz_drop_display_list(ctx, m_displayList);
        m_displayList = nullptr;
    }
    m_textLayer.reset();

    fz_rect bounds = fz_empty_rect;
    fz_var(bounds);
    fz_try(ctx)
        bounds = fz_bound_page(ctx, m_page);
    fz_catch(ctx) {
        qCWarning(lcPdf) << "Could not refresh bounds for page" << m_index + 1 << ":"
                         << MuContext::lastError(ctx);
        return;
    }
    m_bounds = bounds;
    m_sizePoints = sizeOf(bounds);
}

QList<QRectF> PdfPage::search(const QString &needle, int maxHits) const
{
    if (needle.isEmpty() || maxHits <= 0)
        return {};

    fz_context *ctx = MuContext::forThread();
    const QByteArray utf8 = needle.toUtf8();

    QList<fz_quad> quads(maxHits);
    int hits = 0;
    fz_var(hits);
    const std::lock_guard<std::mutex> guard(*m_parseMutex);
    fz_try(ctx)
        hits = fz_search_page(ctx, m_page, utf8.constData(), nullptr, quads.data(), maxHits);
    fz_catch(ctx) {
        qCWarning(lcPdf) << "Search failed on page" << m_index + 1 << ":"
                         << MuContext::lastError(ctx);
        return {};
    }

    QList<QRectF> boxes;
    boxes.reserve(hits);
    for (int i = 0; i < hits; ++i)
        boxes.append(toQRectF(quads.at(i)));
    return boxes;
}

PdfPage::FindResult PdfPage::findText(const QString &needle, const FindOptions &options) const
{
    FindResult result;
    if (needle.isEmpty())
        return result;

    const auto layer = textLayer();
    if (!layer)
        return result;

    if (!options.regularExpression) {
        const auto sensitivity =
            options.matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
        result.ranges = layer->findAll(needle, sensitivity, options.wholeWords);
        return result;
    }

    fz_context *ctx = MuContext::forThread();
    fz_display_list *list = displayList(ctx);
    const fz_stext_options extraction = textOptions();
    fz_stext_page *stext = nullptr;
    fz_var(stext);
    QList<QList<QRectF>> quads;
    int failed = 0;
    fz_var(failed);
    {
        const std::lock_guard<std::mutex> guard(*m_parseMutex);
        fz_try(ctx)
        {
            stext = fz_new_stext_page_from_display_list(ctx, list, &extraction);
            fz_search_options flags = FZ_SEARCH_REGEXP;
            if (!options.matchCase)
                flags = static_cast<fz_search_options>(flags | FZ_SEARCH_IGNORE_CASE);
            fz_match_stext_page_cb(ctx, stext, needle.toUtf8().constData(), collectMatch, &quads,
                                   flags);
        }
        fz_catch(ctx)
            failed = 1;
    }
    fz_drop_stext_page(ctx, stext);
    fz_drop_display_list(ctx, list);

    if (failed) {
        const QString failure = MuContext::lastError(ctx);
        result.error = failure.isEmpty()
            ? QStringLiteral("That regular expression could not be compiled")
            : failure;
        return result;
    }

    for (const QList<QRectF> &hit : quads) {
        const TextRange range = layer->covering(hit);
        if (!range.isEmpty())
            result.ranges.append(range);
    }
    return result;
}

} // namespace sweet::core
