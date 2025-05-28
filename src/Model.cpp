#include "Model.hpp"

#include "BrowseLinkItem.hpp"
#include "HighlightItem.hpp"

#include <QDebug>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QImage>
#include <mupdf/fitz.h>
#include <mupdf/fitz/context.h>
#include <mupdf/fitz/document.h>
#include <mupdf/fitz/geometry.h>
#include <mupdf/fitz/link.h>
#include <mupdf/fitz/pixmap.h>
#include <mupdf/fitz/structured-text.h>
#include <mupdf/fitz/util.h>
#include <mupdf/pdf/annot.h>
#include <mupdf/pdf/document.h>
#include <mupdf/pdf/object.h>
#include <mupdf/pdf/xref.h>
#include <qgraphicsitem.h>
#include <synctex/synctex_parser.h>

fz_quad
union_quad(const fz_quad& a, const fz_quad& b)
{
    float min_x = std::min({a.ul.x, a.ur.x, a.ll.x, a.lr.x, b.ul.x, b.ur.x, b.ll.x, b.lr.x});
    float min_y = std::min({a.ul.y, a.ur.y, a.ll.y, a.lr.y, b.ul.y, b.ur.y, b.ll.y, b.lr.y});
    float max_x = std::max({a.ul.x, a.ur.x, a.ll.x, a.lr.x, b.ul.x, b.ur.x, b.ll.x, b.lr.x});
    float max_y = std::max({a.ul.y, a.ur.y, a.ll.y, a.lr.y, b.ul.y, b.ur.y, b.ll.y, b.lr.y});

    fz_quad result;

    result.ul.x = min_x;
    result.ul.y = min_y;
    result.ur.x = max_x;
    result.ur.y = min_y;
    result.ll.x = min_x;
    result.ll.y = max_y;
    result.lr.x = max_x;
    result.lr.y = max_y;

    return result;
}

void
lock_mutex(void* user, int lock)
{
    auto mutex = static_cast<std::mutex*>(user);
    std::lock_guard<std::mutex> guard(mutex[lock]);
}

void
unlock_mutex(void* user, int lock)
{
    auto mutex = static_cast<std::mutex*>(user);
    // Do nothing – std::lock_guard handles unlocking
}

Model::Model(QGraphicsScene* scene) : m_scene(scene)
{
    fz_locks_context lock_context;
    lock_context.user   = m_locks;
    lock_context.lock   = lock_mutex;
    lock_context.unlock = unlock_mutex;

    m_ctx = fz_new_context(nullptr, &lock_context, FZ_STORE_UNLIMITED);
    if (!m_ctx)
    {
        qWarning("Unable to create mupdf context");
        exit(-1);
    }

    m_selection_color = 0x330000FF;

    m_highlight_color[0] = 1.0f; // Red
    m_highlight_color[1] = 1.0f; // Green
    m_highlight_color[2] = 0.0f; // Blue
    m_highlight_color[3] = 0.4f; // Alpha (translucent yellow)

    m_colorspace = fz_device_rgb(m_ctx);
    fz_register_document_handlers(m_ctx);
}

Model::~Model()
{
    fz_drop_stext_page(m_ctx, m_text_page);
    pdf_drop_page(m_ctx, m_pdfpage);
    pdf_drop_document(m_ctx, m_pdfdoc);
    fz_drop_page(m_ctx, m_page);
    fz_drop_document(m_ctx, m_doc);
    fz_drop_context(m_ctx);
}

void
Model::initSaveOptions() noexcept
{
    m_write_opts = pdf_default_write_options;
    // m_write_opts.do_compress = 1;
    // m_write_opts.do_compress_fonts = 1;
    // m_write_opts.do_compress_images = 1;
    // if (pdf_can_be_saved_incrementally(m_ctx, m_pdfdoc))
    //     m_write_opts.do_incremental = 1;
}

void
Model::enableICC() noexcept
{
    fz_enable_icc(m_ctx);
}

void
Model::setAntialiasingBits(int bits) noexcept
{
    fz_set_aa_level(m_ctx, bits);
}

bool
Model::passwordRequired() noexcept
{
    return fz_needs_password(m_ctx, m_doc);
}

bool
Model::authenticate(const QString& pwd) noexcept
{
    return fz_authenticate_password(m_ctx, m_doc, pwd.toStdString().c_str());
}

fz_outline*
Model::getOutline() noexcept
{
    return fz_load_outline(m_ctx, m_doc);
}

fz_context*
Model::clonedContext() noexcept
{
    if (m_ctx)
        return fz_clone_context(m_ctx);
}

void
Model::closeFile() noexcept
{
    m_filename.clear();

    if (!m_ctx)
        return;

    fz_drop_stext_page(m_ctx, m_text_page);
    fz_drop_page(m_ctx, m_page);
    fz_drop_document(m_ctx, m_doc);

    m_pdfpage   = nullptr;
    m_pdfdoc    = nullptr;
    m_text_page = nullptr;
    m_page      = nullptr;
    m_doc       = nullptr;
}

bool
Model::openFile(const QString& fileName)
{
    m_filename = fileName;
    fz_try(m_ctx)
    {
        m_doc = fz_open_document(m_ctx, CSTR(fileName));

        if (!m_doc)
        {
            qWarning("Unable to open document");
            return false;
        }
    }
    fz_catch(m_ctx)
    {
        return false;
    }

    m_pdfdoc = pdf_specifics(m_ctx, m_doc);
    if (!m_pdfdoc)
    {
        qWarning("Unable to open pdf document");
        return false;
    }

    initSaveOptions();
    return true;
}

void
Model::reloadDocument() noexcept
{
    if (!m_doc)
        return;

    fz_drop_document(m_ctx, m_doc);

    // load accelerator
    fz_try(m_ctx)
    {
        m_doc = fz_open_document(m_ctx, CSTR(m_filename));
        m_pdfdoc = pdf_specifics(m_ctx, m_doc);

        if (!m_doc)
        {
            qWarning("Unable to open document");
            return;
        }

    }
    fz_catch(m_ctx)
    {
        qWarning() << "Exception when opening the document";
        return;
    }
}

void
Model::loadColorProfile(const QString& profileName) noexcept
{
    fz_buffer* profile_data = nullptr;
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

bool
Model::valid()
{
    return m_doc;
}

int
Model::numPages()
{
    if (m_doc)
        return fz_count_pages(m_ctx, m_doc);
    else
        return -1;
}

void
Model::setLinkBoundary(bool state)
{
    m_link_boundary = state;
}

QPixmap
Model::renderPage(int pageno, float zoom, float rotation, bool renderonly) noexcept
{
    QPixmap qpix;

    if (!m_ctx)
        return qpix;

    float scale = zoom * m_dpr;
    fz_device* dev{nullptr};
    fz_pixmap* pix{nullptr};

    fz_try(m_ctx)
    {
        if (!m_page || !renderonly)
        {
            fz_drop_page(m_ctx, m_page);
            m_page = fz_load_page(m_ctx, m_doc, pageno);
            fz_drop_stext_page(m_ctx, m_text_page);
        }

        if (!m_page)
            return qpix;

        fz_rect bounds;
        bounds = fz_bound_page(m_ctx, m_page);
        m_page_height = bounds.y1 - bounds.y0;

        m_transform         = fz_transform_page(bounds, scale, rotation);
        fz_rect transformed = fz_transform_rect(bounds, m_transform);
        fz_irect bbox       = fz_round_rect(transformed);

        m_text_page = fz_new_stext_page_from_page(m_ctx, m_page, nullptr);

        pix = fz_new_pixmap_with_bbox(m_ctx, m_colorspace, bbox, nullptr, 1);
        if (!pix)
        {
            return qpix;
        }

        fz_clear_pixmap_with_value(m_ctx, pix, 0xFFFFFF); // 255 = white

        dev = fz_new_draw_device(m_ctx, fz_identity, pix);

        if (!dev)
        {
            fz_drop_pixmap(m_ctx, pix);
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
        m_width                = fz_pixmap_width(m_ctx, pix);
        m_height               = fz_pixmap_height(m_ctx, pix);
        unsigned char* samples = fz_pixmap_samples(m_ctx, pix);
        int stride             = fz_pixmap_stride(m_ctx, pix);
        int n                  = fz_pixmap_components(m_ctx, pix);
        QImage image;

        switch (n)
        {
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

        for (int y = 0; y < m_height; ++y)
            memcpy(image.scanLine(y), samples + y * stride, m_width * n);

        image.setDotsPerMeterX(static_cast<int>((m_dpi * 1000) / 25.4));
        image.setDotsPerMeterY(static_cast<int>((m_dpi * 1000) / 25.4));
        image.setDevicePixelRatio(m_dpr);
        qpix = QPixmap::fromImage(image);
        qpix.setDevicePixelRatio(m_dpr);
    }
    fz_always(m_ctx)
    {
        fz_close_device(m_ctx, dev);
        fz_drop_device(m_ctx, dev);
        fz_drop_pixmap(m_ctx, pix);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Render failed for page" << pageno;
        return qpix;
    }

    return qpix;
}

QList<Model::SearchResult>
Model::searchHelper(int pageno, const QString& term, bool caseSensitive)
{
    QList<SearchResult> results;

    if (!m_ctx)
        return {};

    fz_try(m_ctx)
    {
        fz_stext_page *text_page = fz_new_stext_page_from_page_number(m_ctx, m_doc, pageno, nullptr);
        for (fz_stext_block* block = text_page->first_block; block; block = block->next)
        {
            if (block->type != FZ_STEXT_BLOCK_TEXT)
                continue;

            for (fz_stext_line* line = block->u.t.first_line; line; line = line->next)
            {
                QString currentWord;
                fz_quad currentQuad = {{0}};

                for (fz_stext_char* ch = line->first_char; ch; ch = ch->next)
                {
                    if (!ch->c)
                        continue;

                    QChar qch = QChar(ch->c);

                    if (term.length() == 1)
                    {
                        if ((caseSensitive && qch == term[0]) || (!caseSensitive && qch.toLower() == term[0].toLower()))
                        {
                            results.append({pageno, ch->quad, m_match_count++});
                        }
                        continue;
                    }

                    if (qch.isSpace())
                    {
                        if (!currentWord.isEmpty())
                        {
                            QString wordToCheck = caseSensitive ? currentWord : currentWord.toLower();
                            QString termToCheck = caseSensitive ? term : term.toLower();

                            if (wordToCheck == termToCheck)
                            {
                                results.append({pageno, currentQuad, m_match_count++});
                            }
                        }

                        currentWord.clear();
                        memset(&currentQuad, 0, sizeof(currentQuad));
                    }
                    else
                    {
                        if (currentWord.isEmpty())
                        {
                            currentWord = qch;
                            currentQuad = ch->quad;
                        }
                        else
                        {
                            currentWord += qch;
                            currentQuad = union_quad(currentQuad, ch->quad);
                        }

                        QString wordToCheck = caseSensitive ? currentWord : currentWord.toLower();
                        QString termToCheck = caseSensitive ? term : term.toLower();

                        if (wordToCheck == termToCheck)
                        {
                            results.append({pageno, currentQuad, m_match_count++});
                            currentWord.clear();
                            memset(&currentQuad, 0, sizeof(currentQuad));
                        }
                    }
                }

                if (!currentWord.isEmpty())
                {
                    QString wordToCheck = caseSensitive ? currentWord : currentWord.toLower();
                    QString termToCheck = caseSensitive ? term : term.toLower();
                    if (wordToCheck == termToCheck)
                        results.append({pageno, currentQuad, m_match_count++});
                }
            }
        }
    }
    fz_catch(m_ctx)
    {
        qWarning() << "MuPDF exception during text search on page" << pageno;
    }

    return results;
}

void
Model::searchAll(const QString& term, bool caseSensitive)
{
    QMap<int, QList<Model::SearchResult>> resultsMap;
    m_match_count = 0;

    for (int pageNum = 0; pageNum < numPages(); ++pageNum)
    {
        QList<Model::SearchResult> results;

        results = searchHelper(pageNum, term, caseSensitive);

        if (!results.isEmpty())
            resultsMap[pageNum] = results;
    }

    emit searchResultsReady(resultsMap, m_match_count);
}

QList<HighlightItem*>
Model::getAnnotations() noexcept
{
    QList<HighlightItem*> annots;
    int index = 0;

    fz_try(m_ctx)
    {
        m_pdfpage = pdf_page_from_fz_page(m_ctx, m_page);

        if (!m_pdfpage)
            return annots;

        pdf_annot* annot = pdf_first_annot(m_ctx, m_pdfpage);
        while (annot)
        {
            fz_rect bbox = pdf_bound_annot(m_ctx, annot);
            bbox         = fz_transform_rect(bbox, m_transform);
            QRectF qrect(bbox.x0 * m_inv_dpr, bbox.y0 * m_inv_dpr, (bbox.x1 - bbox.x0) * m_inv_dpr,
                         (bbox.y1 - bbox.y0) * m_inv_dpr);
            HighlightItem* annot_item = new HighlightItem(qrect, index);
            annot_item->setData(0, "annot");
            annot_item->setData(1, index);
            annots.push_back(annot_item);
            annot = pdf_next_annot(m_ctx, annot);
            ++index;
        }
    }
    fz_catch(m_ctx)
    {
        qDebug() << "Unable to get annotations";
    }

    return annots;
}

pdf_annot*
Model::get_annot_by_index(int index) noexcept
{
    if (!m_pdfpage)
        return nullptr;

    pdf_annot* annot = pdf_first_annot(m_ctx, m_pdfpage);
    int i            = 0;
    while (annot && i < index)
    {
        annot = pdf_next_annot(m_ctx, annot);
        ++i;
    }
    return annot; // nullptr if not found
}

QList<pdf_annot*>
Model::get_annots_by_indexes(const QSet<int> &indexes) noexcept
{
    QList<pdf_annot*> result;

    if (!m_pdfpage || indexes.isEmpty())
        return result;

    pdf_annot* annot = pdf_first_annot(m_ctx, m_pdfpage);
    int i = 0;
    while (annot)
    {
        if (indexes.contains(i))
            result.append(annot);

        annot = pdf_next_annot(m_ctx, annot);
        ++i;
    }

    return result;
}

QList<BrowseLinkItem*>
Model::getLinks() noexcept
{
    QList<BrowseLinkItem*> items;
    fz_try(m_ctx)
    {
        fz_link* head = fz_load_links(m_ctx, m_page);
        if (!head)
            return items;

        fz_link* link = head;

        while (link)
        {
            if (link->uri)
            {
                fz_rect r = fz_transform_rect(link->rect, m_transform);

                float x = r.x0 * m_inv_dpr;
                float y = r.y0 * m_inv_dpr;
                float w = (r.x1 - r.x0) * m_inv_dpr;
                float h = (r.y1 - r.y0) * m_inv_dpr;

                QRectF qtRect(x, y, w, h);
                BrowseLinkItem* item{nullptr};
                QString link_str(link->uri);
                bool external = fz_is_external_link(m_ctx, link->uri);
                if (!external)
                {
                    if (link_str.startsWith("#page"))
                    {
                        int page = link_str.mid(6).toInt();
                        item = new BrowseLinkItem(qtRect, link_str, BrowseLinkItem::LinkType::Page, m_link_boundary);
                        item->setGotoPageNo(page - 1);
                        connect(item, &BrowseLinkItem::jumpToPageRequested, this, &Model::jumpToPageRequested);
                    }
                    else
                    {
                        fz_link_dest dest = fz_resolve_link_dest(m_ctx, m_doc, link->uri);
                        int pageno        = dest.loc.page;
                        switch (dest.type)
                        {

                            case FZ_LINK_DEST_FIT:
                            {
                            }
                            break;

                            case FZ_LINK_DEST_FIT_B:
                                break;
                            case FZ_LINK_DEST_FIT_H:
                            {
                                item = new BrowseLinkItem(qtRect, link_str, BrowseLinkItem::LinkType::FitH,
                                                          m_link_boundary);
                                item->setGotoPageNo(pageno);
                                item->setXYZ({.x = 0, .y = dest.y, .zoom = 0});
                                connect(item, &BrowseLinkItem::horizontalFitRequested, this,
                                        &Model::horizontalFitRequested);
                            }
                            break;

                            case FZ_LINK_DEST_FIT_BH:
                                break;
                            case FZ_LINK_DEST_FIT_V:
                            {
                                item = new BrowseLinkItem(qtRect, link_str, BrowseLinkItem::LinkType::FitV,
                                                          m_link_boundary);
                                item->setGotoPageNo(pageno);
                                connect(item, &BrowseLinkItem::verticalFitRequested, this,
                                        &Model::verticalFitRequested);
                            }
                            break;

                            case FZ_LINK_DEST_FIT_BV:
                                break;

                            case FZ_LINK_DEST_FIT_R:
                                {
                                    item = new BrowseLinkItem(qtRect, link_str, BrowseLinkItem::LinkType::Section,
                                            m_link_boundary);
                                    item->setGotoPageNo(pageno);
                                    item->setXYZ({.x = dest.x, .y = (m_page_height - dest.y), .zoom = dest.zoom});
                                    connect(item, &BrowseLinkItem::jumpToLocationRequested, this,
                                            &Model::jumpToLocationRequested);
                                }
                                break;

                            case FZ_LINK_DEST_XYZ:
                            {
                                item = new BrowseLinkItem(qtRect, link_str, BrowseLinkItem::LinkType::Section,
                                                          m_link_boundary);
                                item->setGotoPageNo(pageno);
                                item->setXYZ({.x = dest.x, .y = dest.y, .zoom = dest.zoom});
                                connect(item, &BrowseLinkItem::jumpToLocationRequested, this,
                                        &Model::jumpToLocationRequested);
                            }
                            break;

                            default:
                                qWarning() << "Unknown goto destination type";
                                break;
                        }
                    }
                }
                else
                {
                    item = new BrowseLinkItem(qtRect, link_str, BrowseLinkItem::LinkType::External, m_link_boundary);
                }
                if (item)
                {
                    item->setData(0, "link");
                    // m_scene->addItem(item);
                    items.append(item);
                }
            }
            link = link->next;
        }
        fz_drop_link(m_ctx, head);
    }

    fz_catch(m_ctx)
    {
        qWarning() << "MuPDF error in renderlink: " << fz_caught_message(m_ctx);
    }

    return items;
}

void
Model::addRectAnnotation(const QRectF& pdfRect) noexcept
{
    auto bbox = convertToMuPdfRect(pdfRect);
    fz_try(m_ctx)
    {
        pdf_annot* annot = pdf_create_annot(m_ctx, m_pdfpage, pdf_annot_type::PDF_ANNOT_SQUARE);

        if (!annot)
            return;

        float yellow[] = {1.0f, 1.0f, 0.0f};
        int n          = 3;

        pdf_set_annot_rect(m_ctx, annot, bbox);
        pdf_set_annot_interior_color(m_ctx, annot, n, yellow);
        pdf_set_annot_opacity(m_ctx, annot, 0.2);
        pdf_update_annot(m_ctx, annot);
        pdf_drop_annot(m_ctx, annot);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Cannot add highlight annotation!: " << fz_caught_message(m_ctx);
    }
}

bool
Model::save() noexcept
{
    fz_try(m_ctx)
    {
        if (m_pdfdoc)
        {
            pdf_save_document(m_ctx, m_pdfdoc, CSTR(m_filename), &m_write_opts);
            reloadDocument();
        }
        else
            qWarning() << "No PDF document opened!";
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Cannot save file: " << fz_caught_message(m_ctx);
        return false;
    }

    emit documentSaved();
    return true;
}

bool
Model::saveAs(const char* filename) noexcept
{
    if (!filename)
        return false;

    fz_try(m_ctx)
    {
        pdf_save_document(m_ctx, m_pdfdoc, filename, &m_write_opts);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Cannot save file: " << fz_caught_message(m_ctx);
        return false;
    }

    return true;
}

fz_rect
Model::convertToMuPdfRect(const QRectF& qtRect) noexcept
{
    // Undo DPI scaling
    float x0 = qtRect.left() * m_dpr;
    float y0 = qtRect.top() * m_dpr;
    float x1 = qtRect.right() * m_dpr;
    float y1 = qtRect.bottom() * m_dpr;

    fz_point p0 = fz_make_point(x0, y0);
    fz_point p1 = fz_make_point(x1, y1);

    // Invert the matrix used during rendering
    fz_matrix inverse = fz_invert_matrix(m_transform);

    // Transform points back into PDF space
    p0 = fz_transform_point(p0, inverse);
    p1 = fz_transform_point(p1, inverse);

    // Return rectangle
    return fz_make_rect(p0.x, p0.y, p1.x, p1.y);
}

void
Model::visitLinkKB(int pageno, float zoom) noexcept
{
    m_hint_to_link_map.clear();
    fz_try(m_ctx)
    {
        // fz_page *page = fz_load_page(m_ctx, m_doc, pageno);
        fz_link* head = fz_load_links(m_ctx, m_page);
        fz_link* link = head;

        if (!link)
        {
            return;
        }

        int i = -1;
        while (link)
        {
            if (link->uri)
            {
                fz_rect r = fz_transform_rect(link->rect, m_transform);
                float x   = r.x0 * m_inv_dpr;
                float h   = (r.y1 - r.y0) * m_inv_dpr;
                float y   = (r.y1 - h) * m_inv_dpr;

                QString hint             = QString::number(i++);
                auto dest                = fz_resolve_link_dest(m_ctx, m_doc, link->uri);
                m_hint_to_link_map[hint] = {.uri = QString::fromUtf8(link->uri), .dest = dest};

                float w = 0.25 * zoom;
                QRect rect(x, y, w, w);
                LinkHint* linkHint = new LinkHint(rect, m_link_hint_bg, m_link_hint_fg, hint, w * 0.5);
                m_scene->addItem(linkHint);
            }
            link = link->next;
            i++;
        }

        fz_drop_link(m_ctx, head);
    }

    fz_catch(m_ctx)
    {
        qWarning() << "MuPDF error in renderlink: " << fz_caught_message(m_ctx);
    }
}

void
Model::clearKBHintsOverlay() noexcept
{
    for (auto& link : m_scene->items())
    {
        if (link->data(0).toString() == "kb_link_overlay")
            m_scene->removeItem(link);
    }
}

void
Model::followLink(const LinkInfo& info) noexcept
{
    QString link_str = info.uri;
    auto link_dest   = info.dest;

    if (link_str.startsWith("#"))
    {
        if (link_str.startsWith("#page"))
        {
            int pageno = link_str.mid(6).toInt() - 1;
            emit jumpToPageRequested(pageno);
        }
        else
        {
            qWarning() << "Not yet supported";
            // TODO: Handle sections etc.
        }
    }
    else
    {
        QDesktopServices::openUrl(QUrl(link_str));
    }
}

bool
Model::hasUnsavedChanges() noexcept
{
    if (m_pdfdoc && pdf_has_unsaved_changes(m_ctx, m_pdfdoc))
        return true;
    return false;
}

QList<QPair<QString, QString>>
Model::extractPDFProperties() noexcept
{
    QList<QPair<QString, QString>> props;

    if (!m_ctx || !m_doc)
        return props;

    pdf_document* pdfdoc = pdf_specifics(m_ctx, m_doc);

    if (!pdfdoc)
        return props;

    // ========== Info Dictionary ==========
    pdf_obj* info = pdf_dict_get(m_ctx, pdf_trailer(m_ctx, pdfdoc), PDF_NAME(Info));
    if (info && pdf_is_dict(m_ctx, info))
    {
        int len = pdf_dict_len(m_ctx, info);
        for (int i = 0; i < len; ++i)
        {
            pdf_obj* keyObj = pdf_dict_get_key(m_ctx, info, i);
            pdf_obj* valObj = pdf_dict_get_val(m_ctx, info, i);

            if (!pdf_is_name(m_ctx, keyObj))
                continue;

            QString key = QString::fromLatin1(pdf_to_name(m_ctx, keyObj));
            QString val;

            if (pdf_is_string(m_ctx, valObj))
            {
                const char* s = pdf_to_str_buf(m_ctx, valObj);
                int slen      = pdf_to_str_len(m_ctx, valObj);

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

            props.append({key, val});
        }
    }

    // ========== Add Derived Properties ==========
    props.append(qMakePair("Page Count", QString::number(pdf_count_pages(m_ctx, pdfdoc))));
    props.append(qMakePair("Encrypted", pdf_needs_password(m_ctx, pdfdoc) ? "Yes" : "No"));
    props.append(qMakePair("PDF Version", QString("%1.%2").arg(pdfdoc->version / 10).arg(pdfdoc->version % 10)));
    props.append(qMakePair("File Path", m_filename));

    return props;
}

void
Model::toggleInvertColor() noexcept
{
    m_invert_color_mode = !m_invert_color_mode;
}

void
Model::apply_night_mode(fz_pixmap* pixmap) noexcept
{
    unsigned char* samples = fz_pixmap_samples(m_ctx, pixmap);
    int n                  = fz_pixmap_components(m_ctx, pixmap); // usually 4 for RGBA
    int w                  = fz_pixmap_width(m_ctx, pixmap);
    int h                  = fz_pixmap_height(m_ctx, pixmap);
    int stride             = fz_pixmap_stride(m_ctx, pixmap);

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

void
Model::highlightHelper(const QPointF& selectionStart, const QPointF& selectionEnd, fz_point& a, fz_point& b) noexcept
{
    a = {static_cast<float>(selectionStart.x()), static_cast<float>(selectionStart.y())};
    b = {static_cast<float>(selectionEnd.x()), static_cast<float>(selectionEnd.y())};

    fz_point topLeft, bottomRight;

    topLeft.x     = std::min(a.x, b.x);
    topLeft.y     = std::min(a.y, b.y);
    bottomRight.x = std::max(a.x, b.x);
    bottomRight.y = std::max(a.y, b.y);

    a = topLeft;
    b = bottomRight;

    fz_matrix inv_transform = fz_invert_matrix(m_transform);
    a                       = fz_transform_point(a, inv_transform);
    b                       = fz_transform_point(b, inv_transform);

    fz_matrix dpr_mat = fz_scale(m_dpr, m_dpr);

    a = fz_transform_point(a, dpr_mat);
    b = fz_transform_point(b, dpr_mat);
}

void
Model::highlightTextSelection(const QPointF& selectionStart, const QPointF& selectionEnd) noexcept
{
    for (auto* object : m_scene->items())
    {
        if (object->data(0).toString() == "selection")
        {
            m_scene->removeItem(object);
        }
    }

    fz_point a, b;

    highlightHelper(selectionStart, selectionEnd, a, b);

    static fz_quad hits[1000];
    int count = 0;

    fz_try(m_ctx)
    {
        // fz_snap_selection(m_ctx, m_text_page, &a, &b, FZ_SELECT_CHARS);
        count = fz_highlight_selection(m_ctx, m_text_page, a, b, hits, 1000);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Selection failed";
        return;
    }

    QBrush brush(QColor::fromRgba(m_selection_color));

    for (int i = 0; i < count; ++i)
    {
        fz_quad q = fz_transform_quad(hits[i], m_transform);

        QPolygonF poly;
        poly << QPointF(q.ll.x * m_inv_dpr, q.ll.y * m_inv_dpr) << QPointF(q.lr.x * m_inv_dpr, q.lr.y * m_inv_dpr)
             << QPointF(q.ur.x * m_inv_dpr, q.ur.y * m_inv_dpr) << QPointF(q.ul.x * m_inv_dpr, q.ul.y * m_inv_dpr);

        QGraphicsPolygonItem* item = m_scene->addPolygon(poly, Qt::NoPen, brush);
        item->setData(0, "selection");
        // item->setZValue(10); // Ensure it draws over the page
        item->setFlag(QGraphicsItem::ItemIsSelectable, false);
        item->setFlag(QGraphicsItem::ItemIgnoresTransformations, false);
    }
}

QString
Model::getSelectionText(const QPointF& selectionStart, const QPointF& selectionEnd) noexcept
{
    QString text;
    fz_point a, b;
    highlightHelper(selectionStart, selectionEnd, a, b);
    char* selected{nullptr};

    fz_try(m_ctx)
    {
        // fz_snap_selection(m_ctx, m_text_page, &a, &b, FZ_SELECT_WORDS);
        selected = fz_copy_selection(m_ctx, m_text_page, a, b, 0);
        text     = QString::fromUtf8(selected);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Selection failed";
    }

    if (selected)
        delete[] selected;
    return text;
}

void
Model::annotHighlightSelection(const QPointF& selectionStart, const QPointF& selectionEnd) noexcept
{
    fz_point a, b;
    highlightHelper(selectionStart, selectionEnd, a, b);

    static fz_quad hits[1000];
    int count = 0;

    fz_try(m_ctx)
    {
        // fz_snap_selection(m_ctx, m_text_page, &a, &b, FZ_SELECT_WORDS);
        count = fz_highlight_selection(m_ctx, m_text_page, a, b, hits, 1000);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Selection failed";
        return;
    }

    fz_try(m_ctx)
    {
        for (int i = 0; i < count; ++i)
        {
            pdf_annot* annot = pdf_create_annot(m_ctx, m_pdfpage, pdf_annot_type::PDF_ANNOT_HIGHLIGHT);

            if (!annot)
            {
                return;
            }

            // fz_quad q = fz_transform_quad(hits[i], m_transform);
            fz_quad q = hits[i];
            pdf_set_annot_quad_points(m_ctx, annot, 1, &q);
            pdf_set_annot_color(m_ctx, annot, 3, m_highlight_color);
            pdf_set_annot_opacity(m_ctx, annot, 0.2);
            pdf_update_annot(m_ctx, annot);
            pdf_drop_annot(m_ctx, annot);
        }
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Cannot add highlight annotation!: " << fz_caught_message(m_ctx);
    }
}

void
Model::annotDeleteRequested(int index) noexcept
{
    pdf_annot* annot = get_annot_by_index(index);
    if (annot)
        pdf_delete_annot(m_ctx, m_pdfpage, annot);
}

QSet<int>
Model::getAnnotationsInArea(const QRectF& area) noexcept
{
    QSet<int> results;

    if (!m_ctx || !m_pdfpage)
        return results;

    pdf_annot* annot = pdf_first_annot(m_ctx, m_pdfpage);

    if (!annot)
        return results;

    int index = 0;

    fz_matrix inv_mat = fz_scale(m_inv_dpr, m_inv_dpr);
    fz_matrix mat = fz_concat(inv_mat, m_transform);

    while (annot)
    {
        fz_rect bbox = pdf_bound_annot(m_ctx, annot);
        // bbox = fz_transform_rect(bbox, m_transform);
        // bbox = fz_transform_rect(bbox, inv_mat);
        bbox = fz_transform_rect(bbox, mat);
        QRectF annotRect(bbox.x0, bbox.y0, (bbox.x1 - bbox.x0), (bbox.y1 - bbox.y0));

        if (area.intersects(annotRect))
            results.insert(index);
        annot = pdf_next_annot(m_ctx, annot);
        index++;
    }

    return results;
}

// Select all text from a page

QString
Model::selectAllText(const QPointF& start, const QPointF& end) noexcept
{
    QString text;
    fz_point a, b;
    highlightHelper(start, end, a, b);
    char* selected{nullptr};
    fz_rect area = fz_make_rect(a.x, a.y, b.x, b.y);

    fz_try(m_ctx)
    {
        selected = fz_copy_rectangle(m_ctx, m_text_page, area, 0);
        text     = QString::fromUtf8(selected);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Selection failed";
    }

    if (selected)
        delete[] selected;
    return text;
}

void
Model::deleteAnnots(const QSet<int> &indexes) noexcept
{
    QList<pdf_annot*> annots = get_annots_by_indexes(indexes);
    for (const auto &annot : annots)
    {
        if (annot)
            pdf_delete_annot(m_ctx, m_pdfpage, annot);
    }
}
