#include "Model.hpp"

#include "BrowseLinkItem.hpp"
#include "HighlightAnnotation.hpp"
#include "PopupAnnotation.hpp"
#include "RectAnnotation.hpp"
#include "utils.hpp"

#include <QtConcurrent/QtConcurrent>
#include <pthread.h>
#include <qbytearrayview.h>

Model::Model(const QString &filepath) noexcept : m_filepath(filepath)
{
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
    fz_drop_document(m_ctx, m_doc);
    fz_drop_context(m_ctx);
}

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

void
Model::open() noexcept
{
    // initialize each mutex
    m_fz_locks.user   = mupdf_mutexes.data();
    m_fz_locks.lock   = mupdf_lock_mutex;
    m_fz_locks.unlock = mupdf_unlock_mutex;

    m_ctx = fz_new_context(nullptr, &m_fz_locks, FZ_STORE_UNLIMITED);
    fz_register_document_handlers(m_ctx);
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
    if (m_doc)
        fz_drop_document(m_ctx, m_doc);
    m_doc = nullptr;
    open();
    return m_success;
}

bool
Model::SaveChanges() noexcept
{
    if (!m_doc || !m_pdf_doc)
        return false;

    fz_try(m_ctx)
    {
        if (m_pdf_doc)
        {
            pdf_save_document(m_ctx, m_pdf_doc, m_filepath.toStdString().data(),
                              nullptr); // TODO: options for saving
            reloadDocument();
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

        fz_snap_selection(m_ctx, text_page, &a, &b, FZ_SELECT_WORDS);
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
                       const fz_point &b) const noexcept
{
    std::string result;

    fz_try(m_ctx)
    {
        fz_page *page = fz_load_page(m_ctx, m_doc, pageno);
        fz_stext_page *text_page
            = fz_new_stext_page_from_page(m_ctx, page, nullptr);
        result = fz_copy_selection(m_ctx, text_page, a, b, 0);
        fz_drop_stext_page(m_ctx, text_page);
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

QPointF
Model::mapPdfToPixmap(int pageno, float pdfX, float pdfY) noexcept
{
    // 1. Get the page bounds (identical to your render function)
    fz_page *page  = fz_load_page(m_ctx, m_doc, pageno);
    fz_rect bounds = fz_bound_page(m_ctx, page);

    // 2. Re-create the same transform used in rendering
    const float scale   = m_zoom * m_dpr * (m_dpi / 72.0f);
    fz_matrix transform = fz_transform_page(bounds, scale, m_rotation);

    // 3. Get the bbox (this is the key!)
    fz_rect transformed = fz_transform_rect(bounds, transform);
    fz_irect bbox       = fz_round_rect(transformed);

    // 4. Transform the point
    fz_point p = {pdfX, pdfY};
    p          = fz_transform_point(p, transform);

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
    fz_page *page{nullptr};
    fz_document *doc{nullptr};
    fz_pixmap *pix{nullptr};
    fz_device *dev{nullptr};

    auto cleanup = [&]() -> void
    {
        fz_close_device(ctx, dev);
        fz_drop_device(ctx, dev);
        fz_drop_link(ctx, head);
        fz_drop_pixmap(ctx, pix);
        fz_drop_page(ctx, page);
        fz_drop_document(ctx, doc);
        fz_drop_context(ctx);
    };

    fz_try(ctx)
    {
        doc = fz_open_document(ctx, job.filepath.toUtf8().constData());
        if (!doc)
            return result;

        page = fz_load_page(ctx, doc, job.pageno);

        if (!page)
            return result;

        fz_rect bounds = fz_bound_page(ctx, page);

        fz_matrix transform = fz_transform_page(bounds, job.zoom, job.rotation);
        fz_rect transformed = fz_transform_rect(bounds, transform);
        fz_irect bbox       = fz_round_rect(transformed);

        // --- Render page to QImage ---
        pix = fz_new_pixmap_with_bbox(ctx, job.colorspace, bbox, nullptr, 1);
        if (!pix)
            return result;

        fz_clear_pixmap_with_value(ctx, pix, 255);
        dev = fz_new_draw_device(ctx, fz_identity, pix);
        if (!dev)
            return result;

        fz_run_page(ctx, page, dev, transform, nullptr);
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
        head = fz_load_links(ctx, page);
        for (fz_link *link = head; link; link = link->next)
        {
            if (!link->uri)
                continue;
            fz_rect r = fz_transform_rect(link->rect, transform);
            QRectF qtRect(r.x0 / job.dpr, r.y0 / job.dpr,
                          (r.x1 - r.x0) / job.dpr, (r.y1 - r.y0) / job.dpr);

            BrowseLinkItem *item = nullptr;
            const QLatin1StringView uri(link->uri);

            if (fz_is_external_link(ctx, link->uri))
                item = new BrowseLinkItem(
                    qtRect, uri, BrowseLinkItem::LinkType::External, true);
            else if (uri.toString().startsWith("#page"))
            {
                float xp, yp;
                fz_location loc
                    = fz_resolve_link(ctx, doc, link->uri, &xp, &yp);
                item = new BrowseLinkItem(qtRect, uri,
                                          BrowseLinkItem::LinkType::Page, true);
                item->setGotoPageNo(loc.page);
            }
            else
            {
                const fz_link_dest dest
                    = fz_resolve_link_dest(ctx, doc, link->uri);
                item = new BrowseLinkItem(qtRect, uri,
                                          BrowseLinkItem::LinkType::XYZ, true);
                item->setGotoPageNo(dest.loc.page);
                item->setXYZ({.x = dest.x, .y = dest.y, .zoom = dest.zoom});
            }

            if (item)
                result.links.push_back(item);
        }

        // --- Extract annotations ---
        pdf_page *pdfPage = pdf_page_from_fz_page(ctx, page);
        if (pdfPage)
        {
            int index = 0;
            float color[3];
            for (pdf_annot *annot = pdf_first_annot(ctx, pdfPage); annot;
                 annot            = pdf_next_annot(ctx, annot), ++index)
            {
                Annotation *annot_item = nullptr;

                fz_rect bbox = pdf_bound_annot(ctx, annot);
                bbox         = fz_transform_rect(bbox, transform);
                QRectF qrect(bbox.x0 * m_inv_dpr, bbox.y0 * m_inv_dpr,
                             (bbox.x1 - bbox.x0) * m_inv_dpr,
                             (bbox.y1 - bbox.y0) * m_inv_dpr);
                const enum pdf_annot_type type = pdf_annot_type(ctx, annot);
                const float alpha              = pdf_annot_opacity(ctx, annot);
                switch (type)
                {
                    case PDF_ANNOT_TEXT:
                    {
                        pdf_annot_color(ctx, annot, &n, color);
                        const QString text  = pdf_annot_contents(ctx, annot);
                        const QColor qcolor = QColor::fromRgbF(
                            color[0], color[1], color[2], alpha);
                        PopupAnnotation *popup
                            = new PopupAnnotation(qrect, text, index, qcolor);
                        annot_item = popup;
                    }
                    break;

                    case PDF_ANNOT_SQUARE:
                    {
                        pdf_annot_interior_color(ctx, annot, &n, color);
                        const QColor qcolor = QColor::fromRgbF(
                            color[0], color[1], color[2], alpha);
                        RectAnnotation *rect_annot
                            = new RectAnnotation(qrect, index, qcolor);
                        annot_item = rect_annot;
                    }
                    break;

                    case PDF_ANNOT_HIGHLIGHT:
                    {
                        // pdf_annot_color(ctx, annot, &n, color);
                        // const QColor qcolor
                        //     = QColor::fromRgbF(color[0], color[1], color[2],
                        //     alpha);
                        HighlightAnnotation *highlight
                            = new HighlightAnnotation(qrect, index,
                                                      Qt::transparent);
                        annot_item = highlight;
                    }
                    break;

                    default:
                        break;
                }

                if (annot_item)
                    result.annotations.push_back(annot_item);
            }
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
