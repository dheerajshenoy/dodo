#include "Model.hpp"

#include "BrowseLinkItem.hpp"
#include "HighlightAnnotation.hpp"
#include "PopupAnnotation.hpp"
#include "RectAnnotation.hpp"
#include "commands/TextHighlightAnnotationCommand.hpp"
#include "mupdf/fitz/display-list.h"
#include "mupdf/fitz/geometry.h"
#include "utils.hpp"

#include <QtConcurrent/QtConcurrent>
#include <pthread.h>
#include <qbytearrayview.h>
#include <qstyle.h>

/**
 * @brief Clean up image data when the last copy of the QImage is destoryed.
 */
static inline void
imageCleanupHandler(void *data)
{
    unsigned char *samples = static_cast<unsigned char *>(data);

    if (samples)
    {
        delete[] samples;
    }
}

static std::array<std::mutex, FZ_LOCK_MAX> mupdf_mutexes;

static void
mupdf_lock_mutex(void *user, int lock)
{
    auto *m = static_cast<std::mutex *>(user);
    m[lock].lock();
}

static void
mupdf_unlock_mutex(void *user, int lock)
{
    auto *m = static_cast<std::mutex *>(user);
    m[lock].unlock();
}

Model::Model(const QString &filepath) noexcept : m_filepath(filepath)
{
    // initialize each mutex
    m_fz_locks.user   = mupdf_mutexes.data();
    m_fz_locks.lock   = mupdf_lock_mutex;
    m_fz_locks.unlock = mupdf_unlock_mutex;
    m_ctx = fz_new_context(nullptr, &m_fz_locks, FZ_STORE_UNLIMITED);
    fz_register_document_handlers(m_ctx);

    m_undo_stack         = new QUndoStack();
    m_highlight_color[0] = 1.0f; // Red
    m_highlight_color[1] = 1.0f; // Green
    m_highlight_color[2] = 0.0f; // Blue
    m_highlight_color[3] = 0.5f; // Alpha (translucent yellow)

    m_annot_rect_color[0] = 1.0f;
    m_annot_rect_color[1] = 0.0f;
    m_annot_rect_color[2] = 0.0f;
    m_annot_rect_color[3] = 0.5f;
}

Model::~Model() noexcept
{
    fz_drop_outline(m_ctx, m_outline);
    pdf_drop_document(m_ctx, m_pdf_doc);
    fz_drop_document(m_ctx, m_doc);
    fz_drop_context(m_ctx);

    for (auto &[_, entry] : m_page_cache)
        fz_drop_display_list(m_ctx, entry.display_list);

    m_page_cache.clear();
    m_stext_page_cache.clear();
    fz_drop_document(m_ctx, m_doc);
}

void
Model::open() noexcept
{
    m_colorspace = fz_device_rgb(m_ctx);
    if (!m_ctx)
    {
        m_success = false;
        return;
    }
    fz_try(m_ctx)
    {
        m_doc = fz_open_document(m_ctx, m_filepath.toStdString().c_str());
        if (!m_doc)
        {
            m_success = false;
            return;
        }
        m_pdf_doc    = pdf_specifics(m_ctx, m_doc);
        m_page_count = fz_count_pages(m_ctx, m_doc);
        m_success    = true;
        cachePageDimension();
    }
    fz_catch(m_ctx)
    {
        fz_drop_context(m_ctx);
        m_ctx     = nullptr;
        m_success = false;
        return;
    }

    for (int i = 0; i < m_page_count; ++i)
        buildPageCache(i);
}

void
Model::buildPageCache(int pageno) noexcept
{
    auto it = m_page_cache.find(pageno);
    if (it != m_page_cache.end())
        return;

    PageCacheEntry entry;

    fz_context *ctx = m_ctx;

    fz_page *page = fz_load_page(ctx, m_doc, pageno);
    if (!page)
    {
        qDebug() << "Failed to load page";
        return;
    }

    fz_rect bounds = fz_bound_page(ctx, page);

    fz_display_list *dlist = fz_new_display_list(ctx, bounds);
    fz_device *list_dev    = fz_new_list_device(ctx, dlist);

    fz_run_page(ctx, page, list_dev, fz_identity, nullptr);

    // Extract links and cache them
    fz_link *head = fz_load_links(ctx, page);
    for (fz_link *link = head; link; link = link->next)
    {
        if (!link->uri)
            continue;

        CachedLink cl;
        cl.rect = link->rect;
        cl.uri  = QString::fromUtf8(link->uri);

        if (fz_is_external_link(ctx, link->uri))
        {
            cl.type = BrowseLinkItem::LinkType::External;
        }
        else if (cl.uri.startsWith("#page"))
        {
            float xp, yp;
            fz_location loc = fz_resolve_link(ctx, m_doc, link->uri, &xp, &yp);
            cl.type         = BrowseLinkItem::LinkType::Page;
            cl.target_page  = loc.page;
        }
        else
        {
            fz_link_dest dest = fz_resolve_link_dest(ctx, m_doc, link->uri);
            cl.type           = BrowseLinkItem::LinkType::XYZ;
            cl.target_page    = dest.loc.page;
            cl.x              = dest.x;
            cl.y              = dest.y;
            cl.zoom           = dest.zoom;
        }

        entry.links.push_back(std::move(cl));
    }

    fz_drop_link(ctx, head);

    pdf_page *pdfPage = pdf_page_from_fz_page(ctx, page);
    if (pdfPage)
    {
        int index = 0;
        float color[3];

        for (pdf_annot *annot = pdf_first_annot(ctx, pdfPage); annot;
             annot            = pdf_next_annot(ctx, annot), ++index)
        {
            CachedAnnotation ca;
            ca.rect    = pdf_bound_annot(ctx, annot);
            ca.type    = pdf_annot_type(ctx, annot);
            ca.index   = index;
            ca.opacity = pdf_annot_opacity(ctx, annot);

            switch (ca.type)
            {
                case PDF_ANNOT_TEXT:
                    pdf_annot_color(ctx, annot, nullptr, color);
                    ca.color = QColor::fromRgbF(color[0], color[1], color[2],
                                                ca.opacity);
                    ca.text  = pdf_annot_contents(ctx, annot);
                    break;

                case PDF_ANNOT_SQUARE:
                    pdf_annot_interior_color(ctx, annot, nullptr, color);
                    ca.color = QColor::fromRgbF(color[0], color[1], color[2],
                                                ca.opacity);
                    break;

                case PDF_ANNOT_HIGHLIGHT:
                    ca.color = Qt::transparent;
                    break;

                default:
                    continue;
            }

            entry.annotations.push_back(std::move(ca));
        }
    }

    fz_close_device(ctx, list_dev);
    fz_drop_device(ctx, list_dev);
    fz_drop_page(ctx, page);

    entry.display_list = dlist;
    entry.bounds       = bounds;

    m_page_cache[pageno] = entry;
}

bool
Model::passwordRequired() const noexcept
{
    if (!m_doc || !m_pdf_doc)
        return false;
    return pdf_needs_password(m_ctx, m_pdf_doc);
}

void
Model::setPopupColor(const QColor &color) noexcept
{
    m_popup_color[0] = color.redF();
    m_popup_color[1] = color.greenF();
    m_popup_color[2] = color.blueF();
    m_popup_color[3] = color.alphaF();
}

void
Model::setHighlightColor(const QColor &color) noexcept
{
    m_highlight_color[0] = color.redF();
    m_highlight_color[1] = color.greenF();
    m_highlight_color[2] = color.blueF();
    m_highlight_color[3] = color.alphaF();
}

void
Model::setSelectionColor(const QColor &color) noexcept
{
    m_selection_color[0] = color.redF();
    m_selection_color[1] = color.greenF();
    m_selection_color[2] = color.blueF();
    m_selection_color[3] = color.alphaF();
}

void
Model::setAnnotRectColor(const QColor &color) noexcept
{
    m_annot_rect_color[0] = color.redF();
    m_annot_rect_color[1] = color.greenF();
    m_annot_rect_color[2] = color.blueF();
    m_annot_rect_color[3] = color.alphaF();
}

bool
Model::decrypt() noexcept
{
    if (!m_doc || !m_pdf_doc)
        return false;

    // TODO
    return true;
}

bool
Model::encrypt(const EncryptInfo &info) noexcept
{
    if (!m_doc || !m_pdf_doc)
        return false;

    // TODO
    return true;
}

bool
Model::authenticate(const QString &password) noexcept
{
    if (!m_doc || !m_pdf_doc)
        return false;
    return pdf_authenticate_password(m_ctx, m_pdf_doc,
                                     password.toUtf8().constData());
}

bool
Model::reloadDocument() noexcept
{
    // Lock to prevent concurrent access (if multithreaded)
    std::lock_guard<std::mutex> lock(m_doc_mutex);

    // Drop text page cache
    clear_fz_stext_page_cache();

    // Drop display lists / page cache entries
    for (auto &[_, entry] : m_page_cache)
        fz_drop_display_list(m_ctx, entry.display_list);
    m_page_cache.clear();

    fz_drop_outline(m_ctx, m_outline);
    pdf_drop_document(m_ctx, m_pdf_doc);
    fz_drop_document(m_ctx, m_doc);
    // fz_drop_context(m_ctx);
    m_pdf_doc = nullptr;
    m_doc     = nullptr;
    m_outline = nullptr;
    m_success = false;
    // m_ctx        = nullptr;
    m_page_count = 0;

    // Reopen the document
    open(); // ensure open() sets m_doc, m_pdf_doc, m_success

    return m_success;
}

bool
Model::SaveChanges() noexcept
{
    if (!m_doc || !m_pdf_doc)
        return false;

    std::string path = m_filepath.toStdString();

    fz_try(m_ctx)
    {
        // 1. MUST clear text pages; they hold page references!
        clear_fz_stext_page_cache();

        // pdf_write_options opts = pdf_default_write_options;
        // opts.do_incremental    = 1; // Faster and more reliable for
        // annotations

        if (m_pdf_doc)
        {
            pdf_save_document(m_ctx, m_pdf_doc, path.c_str(), nullptr);

            // TODO: reload safely
            // reloadDocument();
            // emit documentSaved();
        }
        else
        {
            qWarning() << "No PDF document opened!";
            return false;
        }
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Cannot save file: " << fz_caught_message(m_ctx);
        return false;
    }

    return true;
}

bool
Model::SaveAs(const QString &newFilePath) noexcept
{
    if (!m_doc || !m_pdf_doc)
        return false;

    fz_try(m_ctx)
    {
        pdf_save_document(m_ctx, m_pdf_doc, newFilePath.toStdString().c_str(),
                          nullptr); // TODO: options for saving
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Save As failed: " << fz_caught_message(m_ctx);
        return false;
    }
    return true;
}

bool
Model::followLink(const LinkInfo &info) noexcept
{
    if (!m_doc)
        return false;

    // TODO
}

fz_outline *
Model::getOutline() noexcept
{
    if (!m_doc)
        return nullptr;
    if (!m_outline)
        m_outline = fz_load_outline(m_ctx, m_doc);
    return m_outline;
}

void
Model::cachePageDimension() noexcept
{
    if (!m_doc)
        return;

    fz_page *page     = fz_load_page(m_ctx, m_doc, 0);
    fz_rect rect      = fz_bound_page(m_ctx, page);
    m_page_width_pts  = rect.x1 - rect.x0;
    m_page_height_pts = rect.y1 - rect.y0;

    fz_drop_page(m_ctx, page);
}

std::vector<QPolygonF>
Model::computeTextSelectionQuad(int pageno, const QPointF &start,
                                const QPointF &end) noexcept
{
    std::vector<QPolygonF> quads;
    constexpr int MAX_HITS = 1000;
    fz_quad hits[MAX_HITS];
    int count;

    fz_point a = {static_cast<float>(start.x()), static_cast<float>(start.y())};
    fz_point b = {static_cast<float>(end.x()), static_cast<float>(end.y())};

    fz_stext_page *text_page;
    {
        const float scale = m_zoom * (m_dpi / 72.0f);
        fz_matrix ctm     = fz_scale(1 / scale, 1 / scale);
        a                 = fz_transform_point(a, ctm);
        b                 = fz_transform_point(b, ctm);

        m_selection_start = a;
        m_selection_end   = b;
    }

    fz_try(m_ctx)
    {
        fz_page *page = fz_load_page(m_ctx, m_doc, pageno);
        if (m_stext_page_cache.contains(pageno))
        {
            text_page = m_stext_page_cache[pageno];
        }
        else
        {
            text_page = fz_new_stext_page_from_page(m_ctx, page, nullptr);
            m_stext_page_cache[pageno] = text_page;
        }
        // text_page = fz_new_stext_page_from_page(m_ctx, page, nullptr);

        fz_snap_selection(m_ctx, text_page, &a, &b, FZ_SELECT_CHARS);
        count = fz_highlight_selection(m_ctx, text_page, a, b, hits, MAX_HITS);
    }
    fz_always(m_ctx)
    {
        // fz_drop_stext_page(m_ctx, text_page);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Selection failed";
        return quads;
    }

    const float scale = m_zoom * (m_dpi / 72.0f);

    for (int i = 0; i < count; ++i)
    {
        const fz_quad &q = hits[i];
        QPolygonF poly;
        poly << QPointF(q.ll.x * scale, q.ll.y * scale)
             << QPointF(q.lr.x * scale, q.lr.y * scale)
             << QPointF(q.ur.x * scale, q.ur.y * scale)
             << QPointF(q.ul.x * scale, q.ul.y * scale);
        quads.push_back(poly);
    }

    return quads;
}

std::string
Model::getSelectedText(int pageno, const fz_point &a,
                       const fz_point &b) noexcept
{
    std::string result;
    fz_page *page{nullptr};
    char *selection_text{nullptr};

    fz_try(m_ctx)
    {
        page = fz_load_page(m_ctx, m_doc, pageno);

        fz_stext_page *text_page;

        if (m_stext_page_cache.contains(pageno))
        {
            text_page = m_stext_page_cache[pageno];
        }
        else
        {
            text_page = fz_new_stext_page_from_page(m_ctx, page, nullptr);
            m_stext_page_cache[pageno] = text_page;
        }

        selection_text = fz_copy_selection(m_ctx, text_page, a, b, 0);
    }
    fz_always(m_ctx)
    {
        if (selection_text)
        {
            result = std::string(selection_text);
            fz_free(m_ctx, selection_text);
        }
        fz_drop_page(m_ctx, page);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Failed to copy selection text";
    }

    return result;
}

std::vector<std::pair<QString, QString>>
Model::properties() noexcept
{
    std::vector<std::pair<QString, QString>> props;
    props.reserve(16); // Typical number of PDF properties

    if (!m_ctx || !m_doc)
        return props;

    pdf_document *pdfdoc = pdf_specifics(m_ctx, m_doc);

    if (!pdfdoc)
        return props;

    // ========== Info Dictionary ==========
    pdf_obj *info
        = pdf_dict_get(m_ctx, pdf_trailer(m_ctx, pdfdoc), PDF_NAME(Info));
    if (info && pdf_is_dict(m_ctx, info))
    {
        int len = pdf_dict_len(m_ctx, info);
        for (int i = 0; i < len; ++i)
        {
            pdf_obj *keyObj = pdf_dict_get_key(m_ctx, info, i);
            pdf_obj *valObj = pdf_dict_get_val(m_ctx, info, i);

            if (!pdf_is_name(m_ctx, keyObj))
                continue;

            QString key = QString::fromLatin1(pdf_to_name(m_ctx, keyObj));
            QString val;

            if (pdf_is_string(m_ctx, valObj))
            {
                const char *s = pdf_to_str_buf(m_ctx, valObj);
                int slen      = pdf_to_str_len(m_ctx, valObj);

                if (slen >= 2 && (quint8)s[0] == 0xFE && (quint8)s[1] == 0xFF)
                {
                    QStringDecoder decoder(QStringDecoder::Utf16BE);
                    val = decoder(QByteArray(s + 2, slen - 2));
                }
                else
                {
                    val = QString::fromUtf8(s, slen);
                }
            }
            else if (pdf_is_int(m_ctx, valObj))
                val = QString::number(pdf_to_int(m_ctx, valObj));
            else if (pdf_is_bool(m_ctx, valObj))
                val = pdf_to_bool(m_ctx, valObj) ? "true" : "false";
            else if (pdf_is_name(m_ctx, valObj))
                val = QString::fromLatin1(pdf_to_name(m_ctx, valObj));
            else
                val = QStringLiteral("[Non-string value]");

            props.push_back({key, val});
        }
    }

    // ========== Add Derived Properties ==========
    props.push_back(qMakePair("Page Count",
                              QString::number(pdf_count_pages(m_ctx, pdfdoc))));
    props.push_back(qMakePair(
        "Encrypted", pdf_needs_password(m_ctx, pdfdoc) ? "Yes" : "No"));
    props.push_back(qMakePair(
        "PDF Version",
        QString("%1.%2").arg(pdfdoc->version / 10).arg(pdfdoc->version % 10)));
    props.push_back(qMakePair("File Path", m_filepath));

    return props;
}

fz_point
Model::toPDFSpace(int pageno, QPointF pixelPos) const noexcept
{
    // 1. Get the page bounds
    fz_page *page  = fz_load_page(m_ctx, m_doc, pageno);
    fz_rect bounds = fz_bound_page(m_ctx, page);

    // 2. Re-create the same transform used in rendering
    const float scale   = m_zoom * m_dpi * m_dpr;
    fz_matrix transform = fz_transform_page(bounds, scale, m_rotation);

    // 3. Get the bbox (to find the origin shift)
    fz_rect transformed = fz_transform_rect(bounds, transform);
    fz_irect bbox       = fz_round_rect(transformed);

    // 4. Reverse Step 6: Adjust for Qt's Device Pixel Ratio
    // Map from logical Qt coordinates back to physical pixel coordinates
    float physicalX = pixelPos.x() * m_dpr;
    float physicalY = pixelPos.y() * m_dpr;

    // 5. Reverse Step 5: ADD the bbox origin
    // Move from the pixmap-local (0,0) back to the transformed coordinate space
    fz_point p;
    p.x = physicalX + bbox.x0;
    p.y = physicalY + bbox.y0;

    // 6. Reverse Step 2: Invert the transformation matrix
    // This takes the point from transformed (scaled/rotated) space back to PDF
    // points
    fz_matrix inv_transform = fz_invert_matrix(transform);
    p                       = fz_transform_point(p, inv_transform);

    // Clean up
    fz_drop_page(m_ctx, page);

    return p;
}

QPointF
Model::toPixelSpace(int pageno, fz_point p) const noexcept
{
    // 1. Get the page bounds (identical to your render function)
    fz_page *page  = fz_load_page(m_ctx, m_doc, pageno);
    fz_rect bounds = fz_bound_page(m_ctx, page);

    // 2. Re-create the same transform used in rendering
    const float scale
        = m_zoom * m_dpr * m_dpi; // note that there is not / 72.0f here
    fz_matrix transform = fz_transform_page(bounds, scale, m_rotation);

    // 3. Get the bbox (this is the key!)
    fz_rect transformed = fz_transform_rect(bounds, transform);
    fz_irect bbox       = fz_round_rect(transformed);

    p = fz_transform_point(p, transform);

    // 5. SUBTRACT the bbox origin
    // Pixmap (0,0) is actually at bbox.x0, bbox.y0 in the transformed space
    float localX = p.x - bbox.x0;
    float localY = p.y - bbox.y0;

    // 6. Adjust for Qt's Device Pixel Ratio
    // Since the pixmap is scaled by DPR, but QGraphicsItem::setPos
    // expects logical coordinates:
    return QPointF(localX / m_dpr, localY / m_dpr);
}

Model::RenderJob
Model::createRenderJob(int pageno) const noexcept
{
    RenderJob job;
    job.filepath     = m_filepath;
    job.pageno       = pageno;
    job.dpr          = m_dpr;
    job.dpi          = m_dpi;
    job.zoom         = m_zoom * m_dpr * m_dpi;
    job.rotation     = m_rotation;
    job.invert_color = m_invert_color;
    job.colorspace   = m_colorspace;
    return job;
}

void
Model::requestPageRender(
    const RenderJob &job,
    const std::function<void(PageRenderResult)> &callback) noexcept
{
    QFuture<void> _ = QtConcurrent::run([this, job, callback]()
    {
        PageRenderResult result = renderPageWithExtrasAsync(job);

        if (callback)
        {
            QMetaObject::invokeMethod(QApplication::instance(),
                                      [result, callback]()
            { callback(result); }, Qt::QueuedConnection);
        }
    });
}

Model::PageRenderResult
Model::renderPageWithExtrasAsync(const RenderJob &job) noexcept
{
    PageRenderResult result;

    fz_context *ctx = fz_clone_context(m_ctx);

    if (!ctx)
        return result;

    fz_link *head{nullptr};
    fz_pixmap *pix{nullptr};
    fz_device *dev{nullptr};

    auto cleanup = [&]() -> void
    {
        fz_close_device(ctx, dev);
        fz_drop_device(ctx, dev);
        fz_drop_link(ctx, head);
        fz_drop_pixmap(ctx, pix);
        fz_drop_context(ctx);
    };

    fz_try(ctx)
    {
        // Get cached display list
        if (!m_page_cache.contains(job.pageno))
        {
            qWarning() << "Page not cached:" << job.pageno;
            return result;
        }
        const PageCacheEntry &entry = m_page_cache[job.pageno];

        fz_matrix transform
            = fz_transform_page(entry.bounds, job.zoom, job.rotation);
        fz_rect transformed = fz_transform_rect(entry.bounds, transform);
        fz_irect bbox       = fz_round_rect(transformed);

        // --- Render page to QImage ---
        pix = fz_new_pixmap_with_bbox(ctx, job.colorspace, bbox, nullptr, 1);
        fz_clear_pixmap_with_value(ctx, pix, 255);

        dev = fz_new_draw_device(ctx, fz_identity, pix);
        fz_run_display_list(ctx, entry.display_list, dev, transform,
                            fz_rect_from_irect(bbox), nullptr);

        if (job.invert_color)
            fz_invert_pixmap_luminance(ctx, pix);
        fz_gamma_pixmap(ctx, pix, 1.0f);

        int width  = fz_pixmap_width(ctx, pix);
        int height = fz_pixmap_height(ctx, pix);
        int n      = fz_pixmap_components(ctx, pix);
        int stride = fz_pixmap_stride(ctx, pix);

        unsigned char *samples = fz_pixmap_samples(ctx, pix);
        if (!samples)
            return result;

        unsigned char *copyed_samples = new unsigned char[width * height * n];
        memcpy(copyed_samples, samples, width * height * n);

        QImage::Format fmt;
        switch (n)
        {
            case 1:
                fmt = QImage::Format_Grayscale8;
                break;
            case 3:
                fmt = QImage::Format_RGB888;
                break;
            case 4:
                fmt = QImage::Format_RGBA8888;
                break;

            default:
            {
                qWarning() << "Unsupported pixmap component count:" << n;
                return result;
            }
        }

        result.image = QImage(copyed_samples, width, height, stride, fmt,
                              imageCleanupHandler, copyed_samples);
        result.image.setDotsPerMeterX(
            static_cast<int>((job.dpi * 1000) / 25.4));
        result.image.setDotsPerMeterY(
            static_cast<int>((job.dpi * 1000) / 25.4));
        result.image.setDevicePixelRatio(job.dpr);

        // --- Extract links ---
        for (const auto &link : entry.links)
        {
            if (link.uri.isEmpty())
                continue;
            fz_rect r = fz_transform_rect(link.rect, transform);
            QRectF qtRect(r.x0 / job.dpr, r.y0 / job.dpr,
                          (r.x1 - r.x0) / job.dpr, (r.y1 - r.y0) / job.dpr);

            BrowseLinkItem *item
                = new BrowseLinkItem(qtRect, link.uri, link.type);

            if (link.type == BrowseLinkItem::LinkType::Page)
            {
                item->setGotoPageNo(link.target_page);
            }

            if (link.type == BrowseLinkItem::LinkType::XYZ)
            {
                item->setGotoPageNo(link.target_page);
                item->setXYZ(
                    BrowseLinkItem::Location{link.x, link.y, link.zoom});
            }

            if (item)
                result.links.push_back(item);
        }

        for (const auto &annot : entry.annotations)
        {
            fz_rect r = fz_transform_rect(annot.rect, transform);

            QRectF qtRect(r.x0 / job.dpr, r.y0 / job.dpr,
                          (r.x1 - r.x0) / job.dpr, (r.y1 - r.y0) / job.dpr);

            Annotation *annot_item = nullptr;

            switch (annot.type)
            {
                case PDF_ANNOT_TEXT:
                    annot_item = new PopupAnnotation(qtRect, annot.text,
                                                     annot.index, annot.color);
                    break;

                case PDF_ANNOT_SQUARE:
                    annot_item
                        = new RectAnnotation(qtRect, annot.index, annot.color);
                    break;

                case PDF_ANNOT_HIGHLIGHT:
                    annot_item = new HighlightAnnotation(qtRect, annot.index,
                                                         annot.color);
                    break;

                default:
                    continue;
            }

            result.annotations.push_back(annot_item);
        }
    }
    fz_always(ctx)
    {
        cleanup();
    }
    fz_catch(ctx)
    {
        qWarning() << "MuPDF error in thread:" << fz_caught_message(ctx);
    }

    return result;
}

void
Model::highlightTextSelection(int pageno, const QPointF &start,
                              const QPointF &end) noexcept
{
    fz_page *page{nullptr};

    constexpr int MAX_HITS = 1000;
    fz_quad hits[MAX_HITS];
    int count = 0;

    fz_try(m_ctx)
    {
        page = fz_load_page(m_ctx, m_doc, pageno);

        fz_stext_page *text_page;

        if (m_stext_page_cache.contains(pageno))
        {
            text_page = m_stext_page_cache[pageno];
        }
        else
        {
            text_page = fz_new_stext_page_from_page(m_ctx, page, nullptr);
            m_stext_page_cache[pageno] = text_page;
        }

        fz_point a, b;
        a     = toPDFSpace(pageno, start);
        b     = toPDFSpace(pageno, end);
        count = fz_highlight_selection(m_ctx, text_page, a, b, hits, MAX_HITS);
    }
    fz_always(m_ctx)
    {
        fz_drop_page(m_ctx, page);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Failed to copy selection text";
    }

    // Collect quads for the command
    std::vector<fz_quad> quads;
    quads.reserve(count);
    for (int i = 0; i < count; ++i)
        quads.push_back(hits[i]);

    // // Create and push the command onto the undo stack for undo/redo support
    m_undo_stack->push(new TextHighlightAnnotationCommand(
        this, pageno, std::move(quads), m_highlight_color));
}

std::vector<int>
Model::addHighlightAnnotation(int pageno, const std::vector<fz_quad> &quads,
                              const float color[4]) noexcept
{
    std::vector<int> objNums;

    fz_try(m_ctx)
    {
        // Load the specific page for this annotation
        pdf_page *page = pdf_load_page(m_ctx, m_pdf_doc, pageno);

        if (!page)
            fz_throw(m_ctx, FZ_ERROR_GENERIC, "Failed to load page");

        // Create a separate highlight annotation for each quad
        // This looks better visually for multi-line selections
        pdf_annot *annot = pdf_create_annot(m_ctx, page, PDF_ANNOT_HIGHLIGHT);
        if (!annot)
            return objNums;

        if (quads.empty())
            return objNums;
        pdf_set_annot_quad_points(m_ctx, annot, quads.size(), &quads[0]);
        pdf_set_annot_color(m_ctx, annot, 3, color);
        pdf_set_annot_opacity(m_ctx, annot, color[3]);
        pdf_update_annot(m_ctx, annot);
        pdf_update_page(m_ctx, page);

        // Store the object number for later undo
        pdf_obj *obj = pdf_annot_obj(m_ctx, annot);
        objNums.push_back(pdf_to_num(m_ctx, obj));

        pdf_drop_annot(m_ctx, annot);
        // Drop the page we loaded (not the Model's internal page)
        pdf_drop_page(m_ctx, page);

        if (m_page_cache.contains(pageno))
        {
            m_page_cache.erase(pageno);
            buildPageCache(pageno);
        }
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Redo failed:" << fz_caught_message(m_ctx);
    }

    return objNums;
}

void
Model::removeHighlightAnnotation(int pageno,
                                 const std::vector<int> &objNums) noexcept
{
    fz_try(m_ctx)
    {
        // Load the specific page for this annotation
        pdf_page *page = pdf_load_page(m_ctx, m_pdf_doc, pageno);
        if (!page)
            fz_throw(m_ctx, FZ_ERROR_GENERIC, "Failed to load page");

        // Delete all annotations that were created by this command
        // We need to delete them one at a time, re-finding each by objNum
        // since the list changes after each deletion
        for (int objNum : objNums)
        {
            pdf_annot *annot = pdf_first_annot(m_ctx, page);
            while (annot)
            {
                pdf_obj *obj = pdf_annot_obj(m_ctx, annot);
                if (pdf_to_num(m_ctx, obj) == objNum)
                {
                    pdf_delete_annot(m_ctx, page, annot);
                    pdf_update_page(m_ctx, page);
                    break;
                }
                annot = pdf_next_annot(m_ctx, annot);
            }
        }

        // Drop the page we loaded (not the Model's internal page)
        fz_drop_page(m_ctx, (fz_page *)page);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Undo failed:" << fz_caught_message(m_ctx);
    }
}
