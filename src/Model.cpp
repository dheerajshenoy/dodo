#include "Model.hpp"
#include "BrowseLinkItem.hpp"
#include <QGraphicsScene>
#include <QImage>
#include <QDebug>
#include <QGraphicsRectItem>
#include <mupdf/fitz.h>
#include <mupdf/fitz/context.h>
#include <mupdf/fitz/document.h>
#include <mupdf/fitz/structured-text.h>
#include <mupdf/pdf/annot.h>
#include <mupdf/pdf/document.h>
#include <qgraphicsitem.h>


QString generateHint(int index) noexcept
{
    QString hint;
    const QString alphabet = "asdfghjkl";
    int base = alphabet.length();
    do {
        hint.prepend(alphabet[index % base]);
        index = index / base - 1;
    } while (index >= 0);
    return hint;
}


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

    m_ctx = fz_new_context(nullptr, &lock_context, FZ_STORE_DEFAULT);
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

OutlineWidget* Model::tableOfContents() noexcept
{
    auto owidget = new OutlineWidget(m_ctx, m_doc);
    return owidget;
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

void Model::renderPage(int pageno, bool lowQuality)
{
    float render_dpi = lowQuality ? m_low_dpi : m_dpi;
    float scale = render_dpi / 72.0f;
    if (lowQuality)
    {
        m_transform = fz_scale(m_dpi / m_low_dpi, m_dpi / m_low_dpi);
    } else {
        m_transform = fz_scale(scale, scale);
    }

    auto *ctx = fz_clone_context(m_ctx);
    if (ctx)
    {
        RenderTask *task = new RenderTask(ctx, m_doc, m_colorspace, pageno, m_transform);

        connect(task, &RenderTask::finished, this, [&](int page, QImage img) {
            emit imageRenderRequested(page, img, lowQuality);
        });
        QThreadPool::globalInstance()->start(task);
    } else {
        qWarning("Unable to clone context");
    }
}

QList<QRectF> Model::searchHelper(int pageno, const QString &term)
{
    QList<QRectF> results;
    fz_page *page = nullptr;
    fz_stext_page *textPage = nullptr;

    if (!m_ctx)
        return {};

    fz_try(m_ctx)
    {
        page = fz_load_page(m_ctx, m_doc, pageno);
        textPage = fz_new_stext_page_from_page(m_ctx, page, nullptr);

        if (!textPage)
        {
            fz_drop_page(m_ctx, page);
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

                    auto qch = QChar(ch->c);

                    // Get character bounds
                    QRectF charRect(
                        ch->quad.ul.x,
                        ch->quad.ul.y,
                        ch->quad.ur.x - ch->quad.ul.x,
                        ch->quad.ll.y - ch->quad.ul.y
                    );

                    // Detect word breaks
                    bool isSpace = qch.isSpace();

                    if (isSpace)
                    {
                        if (inWord)
                        {

                            if (currentWord == term)
                                results.append(currentRect);

                            currentWord.clear();
                            currentRect = QRectF();
                            inWord = false;
                        }
                    } else {
                        if (!inWord)
                        {
                            currentWord = qch;
                            currentRect = charRect;
                            inWord = true;
                        } else {
                            currentWord += qch;
                            currentRect = currentRect.united(charRect);
                        }
                    }

                }

                if (inWord && currentWord == term)
                    results.append(currentRect);
            }
        }

    }
    fz_always(m_ctx)
    {
        if (page)
            fz_drop_page(m_ctx, page);

        if (textPage)
            fz_drop_stext_page(m_ctx, textPage);
    }
    fz_catch(m_ctx)
    {

    }

    return results;
}

void Model::searchAll(const QString &term)
{
    int matchCount = 0;
    QMap<int, QList<QRectF>> resultsMap;

    for (int pageNum = 0; pageNum < numPages(); ++pageNum) {
        QList<QRectF> results;

        results = searchHelper(pageNum, term);

        if (!results.isEmpty())
        {
            resultsMap[pageNum] = results;
            matchCount += results.length();
        }
    }

    if (matchCount > 0)
        emit searchResultsReady(resultsMap, matchCount);
}

void Model::clearLinks() noexcept
{
    for (auto &link : m_scene->items())
    {
        if (link->data(0).toString() == "link")
            m_scene->removeItem(link);
    }
}

void Model::renderLinks(int pageno, const fz_matrix& transform)
{
    clearLinks();
    fz_try(m_ctx)
    {
        fz_page *page = fz_load_page(m_ctx, m_doc, pageno);
        fz_link *head = fz_load_links(m_ctx, page);
        fz_link *link = head;

        if (!link)
        {
            fz_drop_page(m_ctx, page);
            return;
        }

        while (link)
        {
            if (link->uri) {
                fz_rect r = fz_transform_rect(link->rect, transform);

                float x = r.x0;
                float w = r.x1 - r.x0;
                float h = r.y1 - r.y0;
                float y = r.y1 - h;

                QRectF qtRect(x, y, w, h);
                BrowseLinkItem *item;
                QString link_str(link->uri);
                if (link_str.startsWith("#"))
                {
                    if (link_str.startsWith("#page"))
                    {
                        int page = link_str.mid(6).toInt();
                        item = new BrowseLinkItem(qtRect,
                                                  link_str,
                                                  BrowseLinkItem::LinkType::Internal_Page);
                        item->setGotoPageNo(page);
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
        fz_drop_page(m_ctx, page);
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
        fz_page *page = fz_load_page(m_ctx, m_doc, pageno);
        if (!page)
        {
            fz_drop_page(m_ctx, page);
            return;
        }

        pdf_page *pdf_page = pdf_page_from_fz_page(m_ctx, page);
        if (!pdf_page)
        {
            fz_drop_page(m_ctx, page);
            pdf_drop_page(m_ctx, pdf_page);
            return;
        }

        // Convert to quads (Highlight annotations require quads)

        pdf_annot *annot = pdf_create_annot(m_ctx, pdf_page,
                                            pdf_annot_type::PDF_ANNOT_SQUARE);

        if (!annot)
        {
            pdf_drop_page(m_ctx, pdf_page);
            fz_drop_page(m_ctx, page);
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
        fz_drop_page(m_ctx, page);
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
    qDebug() << qtRect;
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

void Model::visitLinkKB(int pageno) noexcept
{
    m_hint_to_link_map.clear();
    fz_try(m_ctx)
    {
        fz_page *page = fz_load_page(m_ctx, m_doc, pageno);
        fz_link *head = fz_load_links(m_ctx, page);
        fz_link *link = head;

        if (!link)
        {
            fz_drop_page(m_ctx, page);
            return;
        }

        int i=0;
        while (link)
        {
            if (link->uri) {
                fz_rect r = fz_transform_rect(link->rect, m_transform);
                float x = r.x0;
                float h = r.y1 - r.y0;
                float y = r.y1 - h;

                QString hint = generateHint(i++);
                auto dest = fz_resolve_link_dest(m_ctx, m_doc, link->uri);
                m_hint_to_link_map[hint] = { .uri = QString::fromUtf8(link->uri),
                    .dest = dest };

                QRect rect(x, y, 40, h);

                QGraphicsRectItem *item = new QGraphicsRectItem(rect);
                item->setBrush(QColor(255, 255, 0, 100));
                item->setPen(Qt::NoPen);
                item->setData(0, "kb_link_overlay");
                m_scene->addItem(item);

                // Add text overlay
                QGraphicsSimpleTextItem *text = new QGraphicsSimpleTextItem(hint);
                text->setBrush(Qt::black);
                text->setPos(rect.topLeft());
                text->setZValue(item->zValue() + 1);
                text->setData(0, "kb_link_overlay");
                m_scene->addItem(text);
            }
            link = link->next;
            i++;
        }

        fz_drop_link(m_ctx, head);
        fz_drop_page(m_ctx, page);
    }

    fz_catch(m_ctx)
    {
        qWarning() << "MuPDF error in renderlink: " << fz_caught_message(m_ctx);
    }
}

void Model::copyLinkKB(int pageno) noexcept
{

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

    qDebug() << link_str;

    if (link_str.startsWith("#"))
    {
        if (link_str.startsWith("#page"))
        {

        }

        // TODO: Handle sections etc.
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
