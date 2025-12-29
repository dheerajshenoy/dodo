#include "Model.hpp"

#include "BrowseLinkItem.hpp"
#include "HighlightAnnotation.hpp"
#include "PopupAnnotation.hpp"
#include "RectAnnotation.hpp"
#include "mupdf/fitz/link.h"
#include "mupdf/fitz/structured-text.h"
#include "utils.hpp"

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

    m_selection_start = a;
    m_selection_end   = b;

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

std::vector<Annotation *>
Model::getAnnotations(int pageno) noexcept
{
    std::vector<Annotation *> annots;
    int index = 0;

    fz_try(m_ctx)
    {
        fz_page *page      = fz_load_page(m_ctx, m_doc, pageno);
        pdf_page *pdf_page = pdf_page_from_fz_page(m_ctx, page);

        if (!pdf_page)
            return annots;

        // Count annotations for reserve (optional optimization)
        int annotCount = 0;
        for (pdf_annot *a = pdf_first_annot(m_ctx, pdf_page); a;
             a            = pdf_next_annot(m_ctx, a))
            ++annotCount;
        annots.reserve(annotCount);

        pdf_annot *annot = pdf_first_annot(m_ctx, pdf_page);
        int n;
        float color[3];
        while (annot)
        {
            Annotation *annot_item = nullptr;
            fz_rect bbox           = pdf_bound_annot(m_ctx, annot);
            bbox                   = fz_transform_rect(bbox, m_transform);
            QRectF qrect(bbox.x0 * m_inv_dpr, bbox.y0 * m_inv_dpr,
                         (bbox.x1 - bbox.x0) * m_inv_dpr,
                         (bbox.y1 - bbox.y0) * m_inv_dpr);
            const enum pdf_annot_type type = pdf_annot_type(m_ctx, annot);
            const float alpha              = pdf_annot_opacity(m_ctx, annot);
            switch (type)
            {
                case PDF_ANNOT_TEXT:
                {
                    pdf_annot_color(m_ctx, annot, &n, color);
                    const QString text = pdf_annot_contents(m_ctx, annot);
                    const QColor qcolor
                        = QColor::fromRgbF(color[0], color[1], color[2], alpha);
                    PopupAnnotation *popup
                        = new PopupAnnotation(qrect, text, index, qcolor);
                    annot_item = popup;
                }
                break;

                case PDF_ANNOT_SQUARE:
                {
                    pdf_annot_interior_color(m_ctx, annot, &n, color);
                    const QColor qcolor
                        = QColor::fromRgbF(color[0], color[1], color[2], alpha);
                    RectAnnotation *rect_annot
                        = new RectAnnotation(qrect, index, qcolor);
                    annot_item = rect_annot;
                }
                break;

                case PDF_ANNOT_HIGHLIGHT:
                {
                    // pdf_annot_color(m_ctx, annot, &n, color);
                    // const QColor qcolor
                    //     = QColor::fromRgbF(color[0], color[1], color[2],
                    //     alpha);
                    HighlightAnnotation *highlight = new HighlightAnnotation(
                        qrect, index, Qt::transparent);
                    annot_item = highlight;
                }
                break;

                default:
                    break;
            }

            if (annot_item)
                annots.push_back(annot_item);
            annot = pdf_next_annot(m_ctx, annot);
            ++index;
        }
    }
    fz_catch(m_ctx)
    {
        qDebug() << "MuPDF exception during annotation retrieval";
    }

    return annots;
}

std::vector<BrowseLinkItem *>
Model::getLinks(int pageno) noexcept
{
    std::vector<BrowseLinkItem *> items;

    fz_page *page{nullptr};
    fz_link *head{nullptr};

    fz_try(m_ctx)
    {
        page = fz_load_page(m_ctx, m_doc, pageno);
        head = fz_load_links(m_ctx, page);
        if (!head)
        {
            fz_drop_link(m_ctx, head);
            fz_drop_page(m_ctx, page);
            return items;
        }

        // Count links for reserve
        int linkCount = 0;
        for (fz_link *l = head; l; l = l->next)
            ++linkCount;
        items.reserve(linkCount);

        for (fz_link *link = head; link; link = link->next)
        {
            if (!link->uri)
                continue;

            const fz_rect r = fz_transform_rect(link->rect, m_transform);

            const float x = r.x0 * m_inv_dpr;
            const float y = r.y0 * m_inv_dpr;
            const float w = (r.x1 - r.x0) * m_inv_dpr;
            const float h = (r.y1 - r.y0) * m_inv_dpr;
            QRectF qtRect(x, y, w, h);

            BrowseLinkItem *item = nullptr;
            const QLatin1StringView uri(link->uri);

            if (fz_is_external_link(m_ctx, link->uri))
            {
                item = new BrowseLinkItem(qtRect, uri,
                                          BrowseLinkItem::LinkType::External);
            }
            else if (uri.startsWith(QLatin1String("#page")))
            {
                float xp, yp;
                fz_location loc
                    = fz_resolve_link(m_ctx, m_doc, link->uri, &xp, &yp);
                item = new BrowseLinkItem(qtRect, uri,
                                          BrowseLinkItem::LinkType::Page);
                item->setGotoPageNo(loc.page);
            }
            else
            {
                const fz_link_dest dest
                    = fz_resolve_link_dest(m_ctx, m_doc, link->uri);
                const int pageno = dest.loc.page;

                switch (dest.type)
                {
                    case FZ_LINK_DEST_FIT_H:
                        item = new BrowseLinkItem(
                            qtRect, uri, BrowseLinkItem::LinkType::FitH);
                        item->setLinkType(BrowseLinkItem::LinkType::FitH);
                        item->setGotoPageNo(pageno);
                        item->setXYZ({.x = 0, .y = dest.y, .zoom = 0});
                        break;

                    case FZ_LINK_DEST_FIT_V:
                        item = new BrowseLinkItem(
                            qtRect, uri, BrowseLinkItem::LinkType::FitV);
                        item->setLinkType(BrowseLinkItem::LinkType::FitV);
                        item->setGotoPageNo(pageno);
                        break;

                    case FZ_LINK_DEST_FIT_R:
                    case FZ_LINK_DEST_XYZ:
                        item = new BrowseLinkItem(
                            qtRect, uri, BrowseLinkItem::LinkType::Section);
                        item->setLinkType(BrowseLinkItem::LinkType::XYZ);
                        item->setGotoPageNo(pageno);
                        item->setXYZ(
                            {.x = dest.x, .y = dest.y, .zoom = dest.zoom});
                        break;

                    default:
                        qWarning() << "Unknown link destination type";
                        break;
                }
            }

            if (item)
            {
                item->setData(0, "link");
                item->setURI(link->uri);
                items.push_back(item);
            }
        }
    }
    fz_always(m_ctx)
    {
        fz_drop_link(m_ctx, head);
        fz_drop_page(m_ctx, page);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "MuPDF error in getLinks:" << fz_caught_message(m_ctx);
    }

    return items;
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
    const float scale   = m_zoom * m_dpr * m_dpi;
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
