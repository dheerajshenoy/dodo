#include "Model.hpp"
#include "BrowseLinkItem.hpp"
#include <QGraphicsScene>
#include <QImage>
#include <QDebug>
#include <QGraphicsRectItem>
#include <mupdf/fitz/context.h>
#include <mupdf/pdf/annot.h>
#define CSTR(x) x.toStdString().c_str()

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

    fz_register_document_handlers(m_ctx);

}

Model::~Model()
{
    fz_drop_document(m_ctx, m_doc);
    fz_drop_context(m_ctx);
}

bool Model::openFile(const QString &fileName)
{
    m_doc = fz_open_document(m_ctx, CSTR(fileName));
    if (!m_doc)
    {
        qWarning("Unable to open document");
        return false;
    }

    return true;
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
    qDebug() << render_dpi;
    if (lowQuality)
    {
        m_transform = fz_scale(m_dpi / m_low_dpi, m_dpi / m_low_dpi);
    } else {
        m_transform = fz_scale(scale, scale);
    }

    auto *ctx = fz_clone_context(m_ctx);
    if (ctx)
    {
        RenderTask *task = new RenderTask(ctx, m_doc, pageno, m_transform);

        connect(task, &RenderTask::finished, this, [&](int page, QImage img) {
            if (page == pageno)
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

    fz_try(m_ctx)
    {
        page = fz_load_page(m_ctx, m_doc, pageno);
        textPage = fz_new_stext_page_from_page(m_ctx, page, nullptr);

        if (!textPage)
        {
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
                // flush last word at end of line
                // if (!currentWord.isEmpty())
                //     words.append({ currentWord, QRectF(x0, y0, x1 - x0, y1 - y0) });
            }
        }

    }
    fz_catch(m_ctx)
    {

    }

    return results;
}

void Model::searchAll(const QString &term)
{
    QFuture<void> future = QtConcurrent::run([=]() {
        int matchCount = 0;
        QMap<int, QList<QRectF>> resultsMap;

        for (int pageNum = 0; pageNum < numPages(); ++pageNum) {
            auto results = searchHelper(pageNum, term);

            if (!results.isEmpty())
            {
                resultsMap[pageNum] = results;
                matchCount += results.length();
            }
        }

        QMetaObject::invokeMethod(this, [=]() {
            emit searchResultsReady(resultsMap, matchCount);
        }, Qt::QueuedConnection);

    });
}

void Model::renderLinks(int pageno, const fz_matrix& transform)
{
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
                    item = new BrowseLinkItem(qtRect,
                                              link->uri,
                                              BrowseLinkItem::LinkType::Internal);
                else
                    item = new BrowseLinkItem(qtRect,
                                              link->uri,
                                              BrowseLinkItem::LinkType::External);
                m_scene->addItem(item);
                qDebug() << link->uri;
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

void Model::annotHighlight(int pageno) noexcept
{
    fz_page *page = fz_load_page(m_ctx, m_doc, pageno);

    pdf_page *pdf_page = pdf_page_from_fz_page(m_ctx, page);

    pdf_annot *annot = pdf_create_annot(m_ctx, pdf_page,
                                        pdf_annot_type::PDF_ANNOT_HIGHLIGHT);

    // Set the highlight quad(s)
    pdf_set_annot_quadding(m_ctx, annot, 0);

    float yellow[3] = { 1.0f, 1.0f, 1.0f };
    pdf_set_annot_color(m_ctx, annot, 3, yellow);

    pdf_update_annot(m_ctx, annot);

    pdf_drop_annot(m_ctx, annot);
    pdf_drop_page(m_ctx, pdf_page);

}
