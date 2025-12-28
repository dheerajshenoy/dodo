#include "Model.hpp"

#include "utils.hpp"

#include <QDebug>

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

void
Model::open() noexcept
{
    m_ctx        = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    m_colorspace = fz_device_rgb(m_ctx);
    fz_register_document_handlers(m_ctx);
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

    // TODO
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

QPixmap
Model::renderPageToPixmap(int pageno) noexcept
{

    QPixmap pixmap;

    if (!m_ctx)
        return pixmap;

    fz_device *dev{nullptr};
    fz_pixmap *pix{nullptr};

    const float scale = m_zoom * m_dpr * m_dpi;

    fz_try(m_ctx)
    {
        // fz_drop_stext_page(m_ctx, m_text_page);
        // fz_drop_page(m_ctx, m_page);

        // if (!cache)
        //     m_pageno = pageno;

        fz_page *page = fz_load_page(m_ctx, m_doc, pageno);

        if (!page)
            return pixmap;

        fz_rect bounds;
        bounds = fz_bound_page(m_ctx, page);
        // m_page_height = bounds.y1 - bounds.y0;

        m_transform = fz_transform_page(bounds, scale, m_rotation);

        // fz_matrix ctm = fz_identity;

        // // Move origin to page top-left
        // ctm = fz_pre_translate(ctm, -bounds.x0, -bounds.y0);

        // // Apply rotation
        // ctm = fz_pre_rotate(ctm, m_rotation);

        // // Apply scale (zoom × DPR)
        // ctm = fz_pre_scale(ctm, scale, scale);

        fz_rect transformed = fz_transform_rect(bounds, m_transform);
        fz_irect bbox       = fz_round_rect(transformed);

        // fz_stext_page *stext_page
        //     = fz_new_stext_page_from_page(m_ctx, page, nullptr);

        // if (cache)
        //     return qpix;

        pix = fz_new_pixmap_with_bbox(m_ctx, m_colorspace, bbox, nullptr, 1);

        if (!pix)
            return pixmap;

        fz_clear_pixmap_with_value(m_ctx, pix, 255); // 255 = white

        dev = fz_new_draw_device(m_ctx, fz_identity, pix);

        if (!dev)
            return pixmap;

        fz_run_page(m_ctx, page, dev, m_transform, nullptr);

        if (m_invert_color)
            fz_invert_pixmap_luminance(m_ctx, pix);
        // apply_night_mode(pix);

        fz_gamma_pixmap(m_ctx, pix, 1.0f);

        int width                     = fz_pixmap_width(m_ctx, pix);
        int height                    = fz_pixmap_height(m_ctx, pix);
        const int stride              = fz_pixmap_stride(m_ctx, pix);
        const int n                   = fz_pixmap_components(m_ctx, pix);
        const int size                = width * height * n;
        unsigned char *samples        = fz_pixmap_samples(m_ctx, pix);
        unsigned char *copyed_samples = new unsigned char[size];
        // let QImage own the pixmap memory and delete it when done
        memcpy(copyed_samples, samples, size);
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
                qWarning() << "Unsupported pixmap component count:" << n;
                return pixmap;
        }

        // No .copy() — let QImage own the buffer to avoid copying
        QImage image(copyed_samples, width, height, stride, fmt,
                     imageCleanupHandler, copyed_samples);

        image.setDotsPerMeterX(static_cast<int>((m_dpi * 1000) / 25.4));
        image.setDotsPerMeterY(static_cast<int>((m_dpi * 1000) / 25.4));
        image.setDevicePixelRatio(m_dpr);

        pixmap = QPixmap::fromImage(image, Qt::NoFormatConversion);
        pixmap.setDevicePixelRatio(m_dpr);
    }
    fz_always(m_ctx)
    {
        fz_close_device(m_ctx, dev);
        fz_drop_device(m_ctx, dev);
        fz_drop_pixmap(m_ctx, pix);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Render failed: " << fz_caught_message(m_ctx);
    }

    return pixmap;
}

std::vector<QPolygonF>
Model::computeTextSelectionQuad(int pageno, const QPointF &start,
                                const QPointF &end) noexcept
{
    std::vector<QPolygonF> quads;
    constexpr int MAX_HITS = 1000;
    fz_quad hits[MAX_HITS];
    int count;

    fz_point a, b;
    selectionHelper(start, end, a, b, m_transform, m_dpr);

    fz_try(m_ctx)
    {
        fz_page *page = fz_load_page(m_ctx, m_doc, pageno);
        fz_stext_page *text_page
            = fz_new_stext_page_from_page(m_ctx, page, nullptr);

        fz_snap_selection(m_ctx, text_page, &a, &b, FZ_SELECT_CHARS);
        count = fz_highlight_selection(m_ctx, text_page, a, b, hits, MAX_HITS);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Selection failed";
        return quads;
    }

    for (int i = 0; i < count; ++i)
    {
        const fz_quad &q = fz_transform_quad(hits[i], m_transform);
        QPolygonF poly;
        poly << QPointF(q.ll.x * m_inv_dpr, q.ll.y * m_inv_dpr)
             << QPointF(q.lr.x * m_inv_dpr, q.lr.y * m_inv_dpr)
             << QPointF(q.ur.x * m_inv_dpr, q.ur.y * m_inv_dpr)
             << QPointF(q.ul.x * m_inv_dpr, q.ul.y * m_inv_dpr);
        quads.push_back(poly);
    }

    return quads;
}
