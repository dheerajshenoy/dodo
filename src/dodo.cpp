#include "dodo.hpp"
#include "qlogging.h"

Dodo::Dodo(QWidget *parent)
{
    ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    m_layout->addWidget(m_label);
    m_widget->setLayout(m_layout);
    m_layout->setAlignment(Qt::AlignmentFlag::AlignCenter);
    m_label->show();
    this->setCentralWidget(m_widget);
    this->show();
    INIT_PDF();
}

int Dodo::INIT_PDF()
{
    /* Create a context to hold the exception stack and various caches. */
    if (!ctx)
    {
        fprintf(stderr, "cannot create mupdf context\n");
        return EXIT_FAILURE;
    }

    /* Register the default file types to handle. */
    fz_try(ctx)
    fz_register_document_handlers(ctx);
    fz_catch(ctx)
    {
        fprintf(stderr, "cannot register document handlers: %s\n", fz_caught_message(ctx));
        fz_drop_context(ctx);
        return EXIT_FAILURE;
    }

    /* Open the document. */
    fz_try(ctx)
        doc = fz_open_document(ctx, input.c_str());
    fz_catch(ctx)
    {
        fprintf(stderr, "cannot open document: %s\n", fz_caught_message(ctx));
        fz_drop_context(ctx);
        return EXIT_FAILURE;
    }

    /* Count the number of pages. */
    fz_try(ctx)
    {
        page_count = fz_count_pages(ctx, doc);
        qDebug() << page_count;
    }
    fz_catch(ctx)
    {
        fprintf(stderr, "cannot count number of pages: %s\n", fz_caught_message(ctx));
        fz_drop_document(ctx, doc);
        fz_drop_context(ctx);
        return EXIT_FAILURE;
    }

    if (page_number < 0 || page_number >= page_count)
    {
        fprintf(stderr, "page number out of range: %d (page count %d)\n", page_number + 1, page_count);
        fz_drop_document(ctx, doc);
        fz_drop_context(ctx);
        return EXIT_FAILURE;
    }

    /* Compute a transformation matrix for the zoom and rotation desired. */
    /* The default resolution without scaling is 72 dpi. */
    ctm = fz_scale(zoom / 100, zoom / 100);
    ctm = fz_pre_rotate(ctm, rotate);

    /* Render page to an RGB pixmap. */
    fz_try(ctx)
    {
        pix = fz_new_pixmap_from_page_number(ctx, doc, 15, ctm, fz_device_rgb(ctx), 0);
        QImage img(pix->samples, pix->w, pix->h, pix->stride, QImage::Format_RGB888);
        m_label->setPixmap(QPixmap::fromImage(img));
    }
    fz_catch(ctx)
    {
        fprintf(stderr, "cannot render page: %s\n", fz_caught_message(ctx));
        fz_drop_document(ctx, doc);
        fz_drop_context(ctx);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

Dodo::~Dodo()
{
    /* Clean up. */
    fz_drop_pixmap(ctx, pix);
    fz_drop_document(ctx, doc);
    fz_drop_context(ctx);
}
