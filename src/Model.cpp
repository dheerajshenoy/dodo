#include "Model.hpp"
#include "BrowseLinkItem.hpp"
#include <QGraphicsScene>
#include <QImage>
#include <QDebug>
#include <QGraphicsRectItem>
#include <mupdf/fitz.h>
#include <mupdf/fitz/context.h>
#include <mupdf/fitz/document.h>
#include <mupdf/fitz/link.h>
#include <mupdf/fitz/pixmap.h>
#include <mupdf/fitz/structured-text.h>
#include <mupdf/pdf/annot.h>
#include <mupdf/pdf/document.h>
#include <mupdf/pdf/xref.h>
#include <mupdf/pdf/object.h>
#include <qgraphicsitem.h>


void lock_mutex(void* user, int lock) {
    auto mutex = static_cast<std::mutex*>(user);
    std::lock_guard<std::mutex> guard(mutex[lock]);
}

void unlock_mutex(void* user, int lock) {
    auto mutex = static_cast<std::mutex*>(user);
    // Do nothing – std::lock_guard handles unlocking
}

Model::Model(QGraphicsScene *scene)
: m_scene(scene)
{
    fz_locks_context lock_context;
    lock_context.user = m_locks;
    lock_context.lock = lock_mutex;
    lock_context.unlock = unlock_mutex;

    m_ctx = fz_new_context(nullptr, &lock_context, FZ_STORE_UNLIMITED);
    if (!m_ctx) {
        qWarning("Unable to create mupdf context");
        exit(-1);
    }

    m_colorspace = fz_device_rgb(m_ctx);
    fz_register_document_handlers(m_ctx);

}

Model::~Model()
{
    fz_drop_document(m_ctx, m_doc);
    fz_drop_context(m_ctx);
}

void Model::enableICC() noexcept
{
    fz_enable_icc(m_ctx);
}

void Model::setAntialiasingBits(int bits) noexcept
{
    fz_set_aa_level(m_ctx, bits);
}

bool Model::passwordRequired() noexcept
{
    return fz_needs_password(m_ctx, m_doc);
}

bool Model::authenticate(const QString &pwd) noexcept
{
    return fz_authenticate_password(m_ctx, m_doc, pwd.toStdString().c_str());
}

fz_outline* Model::getOutline() noexcept
{
    return fz_load_outline(m_ctx, m_doc);
}


fz_context* Model::clonedContext() noexcept
{
    if (m_ctx)
        return fz_clone_context(m_ctx);
}

void Model::closeFile() noexcept
{
    m_filename.clear();

    if (!m_ctx)
        return;

    fz_drop_document(m_ctx, m_doc);
}

bool Model::openFile(const QString &fileName)
{
    m_filename = fileName;
    m_doc = fz_open_document(m_ctx, CSTR(fileName));
    if (!m_doc)
    {
        qWarning("Unable to open document");
        return false;
    }

    return true;
}

void Model::loadColorProfile(const QString &profileName) noexcept
{
    fz_buffer *profile_data = nullptr;
    fz_var(profile_data);
    fz_try(m_ctx)
    {
        profile_data = fz_read_file(m_ctx, CSTR(profileName));
        m_colorspace = fz_new_icc_colorspace(m_ctx, FZ_COLORSPACE_RGB, 0, nullptr, profile_data);
    }
    fz_always(m_ctx)
    {
        fz_drop_buffer(m_ctx, profile_data);
    }
    fz_catch(m_ctx)
    {
        fz_report_error(m_ctx);
        qWarning() << "Cannot load color profile";
    }
}

bool Model::valid()
{
    return m_doc;
}

int Model::numPages()
{
    if (m_doc)
        return fz_count_pages(m_ctx, m_doc);
    else
        return -1;
}

void Model::setLinkBoundaryBox(bool state)
{
    m_link_boundary_enabled = state;
}

QPixmap Model::renderPage(int pageno, float zoom, float rotation) noexcept
{
    QPixmap qpix;

    if (!m_ctx)
        return qpix;

    float scale = m_dpi / 72.0 * zoom * m_dpr;

#ifndef NDEBUG
    qDebug() << "Render DPI:" << m_dpi << ", DPR:" << m_dpr;
#endif


    // RenderTask *task = new RenderTask(ctx, m_doc, m_colorspace, pageno, m_transform);
    //
    // connect(task, &RenderTask::finished, this, [&](int page, QImage img) {
    //     emit imageRenderRequested(page, img, lowQuality);
    // });
    // QThreadPool::globalInstance()->start(task);
    fz_drop_page(m_ctx, m_page);

    fz_try(m_ctx)
    {
        m_page = fz_load_page(m_ctx, m_doc, pageno);
        if (!m_page)
            return qpix;

        fz_rect bounds;
        bounds = fz_bound_page(m_ctx, m_page);
        // FIXME: Load link here maybe ?

        m_transform = fz_transform_page(bounds, scale, rotation);
        fz_rect transformed = fz_transform_rect(bounds, m_transform);
        fz_irect bbox = fz_round_rect(transformed);

        fz_pixmap *pix;
        pix = fz_new_pixmap_with_bbox(m_ctx,
                                      m_colorspace,
                                      bbox,
                                      nullptr,
                                      1);
        if (!pix)
        {
            fz_drop_page(m_ctx, m_page);
            return qpix;
        }

        fz_clear_pixmap_with_value(m_ctx, pix, 0xFFFFFF); // 255 = white

        fz_device *dev = fz_new_draw_device(m_ctx, fz_identity, pix);

        if (!dev)
        {
            fz_drop_pixmap(m_ctx, pix);
            fz_drop_page(m_ctx, m_page);
            return qpix;
        }

        fz_run_page(m_ctx, m_page, dev, m_transform, nullptr);

        if (m_invert_color_mode)
        {
            fz_invert_pixmap_luminance(m_ctx, pix);
            // apply_night_mode(pix);
        }

        // fz_gamma_pixmap(m_ctx, pix, 1.0f);
        // Convert fz_pixmap to QImage
        m_width = fz_pixmap_width(m_ctx,pix);
        m_height = fz_pixmap_height(m_ctx,pix);
        unsigned char *samples = fz_pixmap_samples(m_ctx,pix);
        int stride = fz_pixmap_stride(m_ctx,pix);
        int n = fz_pixmap_components(m_ctx, pix);
        QImage image;

#ifndef NDEBUG
        qDebug() << "Pixmap size:" << fz_pixmap_width(m_ctx, pix) << "x" << fz_pixmap_height(m_ctx, pix);
#endif
        switch (n) {
            case 1:
                image = QImage(samples, m_width, m_height, stride, QImage::Format_Grayscale8);
                break;
            case 3:
                image = QImage(samples, m_width, m_height, stride, QImage::Format_RGB888);
                break;
            case 4:
                image = QImage(samples, m_width, m_height, stride, QImage::Format_RGBA8888);
                break;
            default:
                qWarning() << "Unsupported pixmap component count:" << n;
                fz_drop_pixmap(m_ctx, pix);
                fz_drop_page(m_ctx, m_page);
                return qpix;
        }

        // for (int y = 0; y < m_height; ++y)
        //     memcpy(image.scanLine(y), samples + y * stride, m_width * n);  // 3 bytes per pixel

        image.setDotsPerMeterX(static_cast<int>((m_dpi * 1000) / 25.4));
        image.setDotsPerMeterY(static_cast<int>((m_dpi * 1000) / 25.4));
        image = image.copy();
        image.setDevicePixelRatio(m_dpr);
        qpix = QPixmap::fromImage(image);
        qpix.setDevicePixelRatio(m_dpr);

        // Cleanup
        fz_close_device(m_ctx, dev);
        fz_drop_device(m_ctx, dev);
        fz_drop_pixmap(m_ctx, pix);
        // fz_drop_page(m_ctx, m_page);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Render failed for page" << pageno;
        return qpix;
    }

    return qpix;
}

QList<QPair<QRectF, int>>
Model::searchHelper(int pageno, const QString &term, bool caseSensitive)
{
    QList<QPair<QRectF, int>> results;
    fz_stext_page *textPage = nullptr;

    if (!m_ctx)
        return {};

    fz_try(m_ctx)
    {
        // page = fz_load_page(m_ctx, m_doc, pageno);
        textPage = fz_new_stext_page_from_page(m_ctx, m_page, nullptr);

        if (!textPage)
        {
            // fz_drop_page(m_ctx, page);
            fz_drop_stext_page(m_ctx, textPage);
            qWarning() << "Unable to get texts from page";
            return {};
        }

        for (fz_stext_block* block = textPage->first_block; block; block = block->next)
        {
            if (block->type != FZ_STEXT_BLOCK_TEXT)
                continue;

            for (fz_stext_line* line = block->u.t.first_line; line; line = line->next)
            {
                QString currentWord;
                QRectF currentRect;
                bool inWord = false;

                for (fz_stext_char* ch = line->first_char; ch; ch = ch->next)
                {
                    if (ch->c == 0)
                        continue;

                    QChar qch = QChar(ch->c);

                    QRectF charRect(
                        ch->quad.ul.x,
                        ch->quad.ul.y,
                        ch->quad.ur.x - ch->quad.ul.x,
                        ch->quad.ll.y - ch->quad.ul.y
                    );

                    // Handle single-character match
                    if (term.length() == 1)
                    {
                        if ((caseSensitive && qch == term[0]) ||
                            (!caseSensitive && qch.toLower() == term[0].toLower()))
                        {
                            results.append(qMakePair(charRect, m_match_count));
                            m_match_count++;
                        }
                        continue;
                    }

                    // Build up word for multi-char term match
                    if (qch.isSpace())
                    {
                        // Word boundary — evaluate match
                        if (!currentWord.isEmpty())
                        {
                            QString wordToCheck = caseSensitive ? currentWord : currentWord.toLower();
                            QString termToCheck = caseSensitive ? term : term.toLower();

                            if (wordToCheck == termToCheck)
                            {
                                results.append(qMakePair(currentRect, m_match_count));
                                m_match_count++;
                            }
                        }

                        // Reset word state
                        currentWord.clear();
                        currentRect = QRectF();
                    } else {
                        if (currentWord.isEmpty())
                        {
                            currentWord = qch;
                            currentRect = charRect;
                        } else {
                            currentWord += qch;
                            currentRect = currentRect.united(charRect);
                        }

                        // Check mid-word match
                        QString wordToCheck = caseSensitive ? currentWord : currentWord.toLower();
                        QString termToCheck = caseSensitive ? term : term.toLower();

                        if (wordToCheck == termToCheck)
                        {
                            results.append(qMakePair(currentRect, m_match_count));
                            m_match_count++;

                            // Reset to allow overlapping matches (optional)
                            currentWord.clear();
                            currentRect = QRectF();
                        }
                    }
                }

                // Final check at end of line
                if (!currentWord.isEmpty())
                {
                    QString wordToCheck = caseSensitive ? currentWord : currentWord.toLower();
                    QString termToCheck = caseSensitive ? term : term.toLower();

                    if (wordToCheck == termToCheck)
                    {
                        results.append(qMakePair(currentRect, m_match_count));
                        m_match_count++;
                    }
                }

            }
        }

    }
    fz_always(m_ctx)
    {
        // fz_drop_page(m_ctx, page);
        fz_drop_stext_page(m_ctx, textPage);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "MuPDF exception during text search on page" << pageno;
    }

    return results;
}

void Model::searchAll(const QString &term, bool caseSensitive)
{
    QMap<int, QList<QPair<QRectF, int>>> resultsMap;
    m_match_count = 0;

    for (int pageNum = 0; pageNum < numPages(); ++pageNum) {
        QList<QPair<QRectF, int>> results;

        results = searchHelper(pageNum, term, caseSensitive);

        if (!results.isEmpty())
        {
            resultsMap[pageNum] = results;
        }
    }

    emit searchResultsReady(resultsMap, m_match_count);
}

void Model::clearLinks() noexcept
{
    for (auto &link : m_scene->items())
    {
        if (link->data(0).toString() == "link")
            m_scene->removeItem(link);
    }
}

void Model::renderLinks(int pageno)
{
    clearLinks();
    fz_try(m_ctx)
    {
        // m_page = fz_load_page(m_ctx, m_doc, pageno);
        fz_link *head = fz_load_links(m_ctx, m_page);
        if (!head)
            return;

        fz_link *link = head;

        while (link)
        {
            if (link->uri) {
                fz_rect r = fz_transform_rect(link->rect, m_transform);

                float x = r.x0 * m_inv_dpr;
                float y = (r.y0) * m_inv_dpr;
                float w = (r.x1 - r.x0) * m_inv_dpr;
                float h = (r.y1 - r.y0) * m_inv_dpr;

                QRectF qtRect(x, y, w, h);
                BrowseLinkItem *item;
                QString link_str(link->uri);
                bool external = fz_is_external_link(m_ctx, link->uri);
                if (!external)
                {
                    if (link_str.startsWith("#page"))
                    {
                        int page = link_str.mid(6).toInt();
                        item = new BrowseLinkItem(qtRect,
                                                  link_str,
                                                  BrowseLinkItem::LinkType::Internal_Page);
                        item->setGotoPageNo(page - 1);
                        connect(item, &BrowseLinkItem::jumpToPageRequested, this, &Model::jumpToPageRequested);
                    } else {
                        item = new BrowseLinkItem(qtRect,
                                                  link_str,
                                                  BrowseLinkItem::LinkType::Internal_Section);
                        fz_link_dest dest = fz_resolve_link_dest(m_ctx, m_doc, link->uri);
                        int pageno = dest.loc.page;
                        int chapterno = dest.loc.chapter;
                        switch(dest.type) {

                            case FZ_LINK_DEST_FIT: {
                            }
                            break;

                            case FZ_LINK_DEST_FIT_B: break;
                            case FZ_LINK_DEST_FIT_H: {
                                // emit horizontalFitRequested();
                            }
                            break;

                            case FZ_LINK_DEST_FIT_BH: break;
                            case FZ_LINK_DEST_FIT_V: {
                                // emit verticalFitRequested();
                            }
                            break;

                            case FZ_LINK_DEST_FIT_BV: break;
                            case FZ_LINK_DEST_FIT_R: {
                                // auto loc = dest.loc;
                                // emit fitRectRequested(dest.x, dest.y, dest.w, dest.h);
                            }
                            case FZ_LINK_DEST_XYZ: {
                                item->setGotoPageNo(pageno);
                                item->setXYZ({ .x = dest.x, .y = dest.y, .zoom = dest.zoom });
                                connect(item, &BrowseLinkItem::jumpToLocationRequested, this,
                                        &Model::jumpToLocationRequested);
                            }
                            break;
                            default:
                                qWarning() << "Unknown goto destination type";
                                break;

                        }
                    }

                } else {
                    item = new BrowseLinkItem(qtRect,
                                              link_str,
                                              BrowseLinkItem::LinkType::External);
                }
                item->setData(0, "link");
                m_scene->addItem(item);
            }
            link = link->next;
        }
        fz_drop_link(m_ctx, head);
        // fz_drop_page(m_ctx, page);
    }

    fz_catch(m_ctx)
    {
        qWarning() << "MuPDF error in renderlink: " << fz_caught_message(m_ctx);
    }
}

void Model::addHighlightAnnotation(int pageno, const QRectF &pdfRect) noexcept
{
    auto bbox = convertToMuPdfRect(pdfRect, m_transform, m_dpi / 72.0);
    fz_try(m_ctx)
    {
        // m_page = fz_load_page(m_ctx, m_doc, pageno);
        // if (!m_page)
        // {
        //     fz_drop_page(m_ctx, page);
        //     return;
        // }

        pdf_page *pdf_page = pdf_page_from_fz_page(m_ctx, m_page);
        if (!pdf_page)
        {
            // fz_drop_page(m_ctx, page);
            pdf_drop_page(m_ctx, pdf_page);
            return;
        }

        // Convert to quads (Highlight annotations require quads)

        pdf_annot *annot = pdf_create_annot(m_ctx, pdf_page,
                                            pdf_annot_type::PDF_ANNOT_SQUARE);

        if (!annot)
        {
            pdf_drop_page(m_ctx, pdf_page);
            // fz_drop_page(m_ctx, page);
            return;
        }

        // Convert to quads (Highlight annotations require quads)
        // fz_quad quad;
        // quad.ul.x = bbox.x0;
        // quad.ul.y = bbox.y1;
        // quad.ur.x = bbox.x1;
        // quad.ur.y = bbox.y1;
        // quad.ll.x = bbox.x0;
        // quad.ll.y = bbox.y0;
        // quad.lr.x = bbox.x1;
        // quad.lr.y = bbox.y0;
        // pdf_set_annot_quad_points(m_ctx, annot, 1, &quad);

        float yellow[] = {1.0f, 1.0f, 0.0f};
        int n = 3;

        pdf_set_annot_rect(m_ctx, annot, bbox);
        pdf_set_annot_interior_color(m_ctx, annot, n, yellow);
        pdf_set_annot_opacity(m_ctx, annot, 0.2);
        pdf_update_annot(m_ctx, annot);

        pdf_drop_annot(m_ctx, annot);
        pdf_drop_page(m_ctx, pdf_page);
        // fz_drop_page(m_ctx, page);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Cannot add highlight annotation!: " << fz_caught_message(m_ctx);
    }
}

bool Model::save() noexcept
{
    fz_try(m_ctx)
    {
        auto pdf_doc = pdf_document_from_fz_document(m_ctx, m_doc);
        pdf_save_document(m_ctx, pdf_doc, CSTR(m_filename), nullptr);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Cannot save file: " << fz_caught_message(m_ctx);
        return false;
    }

    return true;
}

fz_rect Model::convertToMuPdfRect(const QRectF &qtRect,
                                  const fz_matrix &transform,
                                  float dpiScale) noexcept
{
    // Undo DPI scaling
    float x0 = qtRect.left() ;
    float y0 = qtRect.top() ;
    float x1 = qtRect.right() ;
    float y1 = qtRect.bottom() ;

    fz_point p0 = fz_make_point(x0, y0);
    fz_point p1 = fz_make_point(x1, y1);

    // Invert the matrix used during rendering
    fz_matrix inverse = fz_invert_matrix(transform);

    // Transform points back into PDF space
    p0 = fz_transform_point(p0, inverse);
    p1 = fz_transform_point(p1, inverse);

    // Return rectangle
    return fz_make_rect(p0.x, p0.y, p1.x, p1.y);
}

void Model::visitLinkKB(int pageno, float zoom) noexcept
{
    m_hint_to_link_map.clear();
    fz_try(m_ctx)
    {
        // fz_page *page = fz_load_page(m_ctx, m_doc, pageno);
        fz_link *head = fz_load_links(m_ctx, m_page);
        fz_link *link = head;

        if (!link)
        {
            fz_drop_page(m_ctx, m_page);
            return;
        }

        int i=-1;
        while (link)
        {
            if (link->uri) {
                fz_rect r = fz_transform_rect(link->rect, m_transform);
                float x = r.x0 * m_inv_dpr;
                float h = (r.y1 - r.y0) * m_inv_dpr;
                float y = (r.y1 - h) * m_inv_dpr;

                QString hint = QString::number(i++);
                auto dest = fz_resolve_link_dest(m_ctx, m_doc, link->uri);
                m_hint_to_link_map[hint] = { .uri = QString::fromUtf8(link->uri),
                    .dest = dest };

                QRect rect(x, y, 1.1 * zoom, 1.1 * zoom);

                QGraphicsRectItem *item = new QGraphicsRectItem(rect);
                item->setBrush(QColor(m_link_hint_bg));
                item->setPen(Qt::NoPen);
                item->setData(0, "kb_link_overlay");
                m_scene->addItem(item);

                // Add text overlay
                QGraphicsSimpleTextItem *text = new QGraphicsSimpleTextItem(hint);
                // 🔧 Set larger font based on rect height
                QFont font;
                font.setPointSizeF(h * 0.6);  // Try 0.6 or adjust to taste
                font.setBold(true);
                text->setFont(font);

                text->setBrush(QColor(m_link_hint_fg));
                text->setPos(rect.topLeft());
                text->setZValue(item->zValue() + 1);
                text->setData(0, "kb_link_overlay");
                m_scene->addItem(text);
            }
            link = link->next;
            i++;
        }

        fz_drop_link(m_ctx, head);
        // fz_drop_page(m_ctx, page);
    }

    fz_catch(m_ctx)
    {
        qWarning() << "MuPDF error in renderlink: " << fz_caught_message(m_ctx);
    }
}

void Model::copyLinkKB(int pageno) noexcept
{
    // TODO: copy link hint
#ifndef NDEBUG
    qDebug() << "Copy link hint not yet implemented";
#endif
}


void Model::clearKBHintsOverlay() noexcept
{
    for (auto &link : m_scene->items())
    {
        if (link->data(0).toString() == "kb_link_overlay")
            m_scene->removeItem(link);
    }
}

void Model::followLink(const LinkInfo &info) noexcept
{
    QString link_str = info.uri;
    auto link_dest = info.dest;

    if (link_str.startsWith("#"))
    {
        if (link_str.startsWith("#page"))
        {
            int pageno = link_str.mid(6).toInt() - 1;
            emit jumpToPageRequested(pageno);
        } else {
            qWarning () << "Not yet supported";
            // TODO: Handle sections etc.
        }
    }
    else {
        QDesktopServices::openUrl(QUrl(link_str));
    }
}

bool Model::hasUnsavedChanges() noexcept
{
    pdf_document *idoc = pdf_specifics(m_ctx, m_doc);
    if (idoc)
        return pdf_has_unsaved_changes(m_ctx, idoc);
    return false;
}

QList<QPair<QString, QString>> Model::extractPDFProperties() noexcept
{
    QList<QPair<QString, QString>> props;

    if (!m_ctx || !m_doc)
        return props;

    pdf_document* pdfdoc = pdf_specifics(m_ctx, m_doc);

    if (!pdfdoc)
        return props;

    // ========== Info Dictionary ==========
    pdf_obj* info = pdf_dict_get(m_ctx, pdf_trailer(m_ctx, pdfdoc), PDF_NAME(Info));
    if (info && pdf_is_dict(m_ctx, info)) {
        int len = pdf_dict_len(m_ctx, info);
        for (int i = 0; i < len; ++i) {
            pdf_obj* keyObj = pdf_dict_get_key(m_ctx, info, i);
            pdf_obj* valObj = pdf_dict_get_val(m_ctx, info, i);

            if (!pdf_is_name(m_ctx, keyObj))
                continue;

            QString key = QString::fromLatin1(pdf_to_name(m_ctx, keyObj));
            QString val;

            if (pdf_is_string(m_ctx, valObj)) {
                const char* s = pdf_to_str_buf(m_ctx, valObj);
                int slen = pdf_to_str_len(m_ctx, valObj);

                if (slen >= 2 && (quint8)s[0] == 0xFE && (quint8)s[1] == 0xFF)
                    val = QString::fromUtf16(reinterpret_cast<const ushort*>(s + 2), (slen - 2) / 2);
                else
                    val = QString::fromUtf8(s, slen);
            }
            else if (pdf_is_int(m_ctx, valObj))
                val = QString::number(pdf_to_int(m_ctx, valObj));
            else if (pdf_is_bool(m_ctx, valObj))
                val = pdf_to_bool(m_ctx, valObj) ? "true" : "false";
            else if (pdf_is_name(m_ctx, valObj))
                val = QString::fromLatin1(pdf_to_name(m_ctx, valObj));
            else
                val = QStringLiteral("[Non-string value]");

            props.append({ key, val });
        }
    }

    // ========== Add Derived Properties ==========
    props.append(qMakePair("Page Count", QString::number(pdf_count_pages(m_ctx, pdfdoc))));
    props.append(qMakePair("Encrypted", pdf_needs_password(m_ctx, pdfdoc) ? "Yes" : "No"));
    props.append(qMakePair("PDF Version",
                           QString("%1.%2").arg(pdfdoc->version / 10).arg(pdfdoc->version % 10)));

    return props;
}

void Model::invertColor() noexcept
{
    m_invert_color_mode = !m_invert_color_mode;
}

void Model::apply_night_mode(fz_pixmap* pixmap) noexcept
{
    unsigned char* samples = fz_pixmap_samples(m_ctx, pixmap);
    int n = fz_pixmap_components(m_ctx, pixmap);  // usually 4 for RGBA
    int w = fz_pixmap_width(m_ctx, pixmap);
    int h = fz_pixmap_height(m_ctx, pixmap);
    int stride = fz_pixmap_stride(m_ctx, pixmap);

    for (int y = 0; y < h; ++y)
    {
        unsigned char* row = samples + y * stride;

        for (int x = 0; x < w; ++x)
        {
            unsigned char* px = row + x * n;

            // Skip alpha channel
            for (int c = 0; c < n - 1; ++c)
            {
                // Soft inversion: shift colors toward darker tones
                // Pure white (255) becomes 0 (black)
                // Pure black (0) becomes 255 (white)
                // Midtones become dimmed
                if (fz_pixmap_colorspace(m_ctx, pixmap) != nullptr && c < n - 1)
                    px[c] = 255 - px[c];
            }
        }
    }
}

