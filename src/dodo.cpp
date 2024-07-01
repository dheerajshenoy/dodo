#include "dodo.hpp"
#include <mupdf/fitz/geometry.h>

Dodo::Dodo(int argc, char** argv, QWidget *parent)
{
    //m_layout->addWidget(m_label);
    /*m_layout->addWidget(m_gview);*/
    /*m_gview->setScene(m_gscene);*/
    /*m_gscene->addWidget(m_label);*/
    m_layout->addWidget(m_label);

    m_widget->setLayout(m_layout);
    m_label->setAlignment(Qt::AlignmentFlag::AlignCenter);

    this->setCentralWidget(m_widget);
    INIT_PDF();
    SetKeyBinds();
    /*Open("/home/neo/test.pdf", 15);*/
    this->show();

    /*qDebug() << argv[1];*/
    if (argc > 1)
        Open(QString(argv[1]));
}

void Dodo::SetKeyBinds()
{
    QShortcut *kb_zoomin = new QShortcut(QKeySequence("="), this, [=]() {
        this->Zoom(+10);
    });

    QShortcut *kb_zoomout = new QShortcut(QKeySequence("-"), this, [=]() {
        this->Zoom(-10);
    });

    QShortcut *kb_zoomreset = new QShortcut(QKeySequence("+"), this, [=]() {
        this->ZoomReset();
    });

    QShortcut *kb_open = new QShortcut(QKeySequence("Ctrl+o"), this, [=]() {
        QString filename = QFileDialog::getOpenFileName(this, "Open file", nullptr, "PDF Files (*.pdf);;");
        this->Open(filename);
    });

    QShortcut *kb_next_page = new QShortcut(QKeySequence("j"), this, [=]() {
        this->GotoPage(1);
    });

    QShortcut *kb_prev_page = new QShortcut(QKeySequence("k"), this, [=]() {
        this->GotoPage(-1);
    });

}

void Dodo::GotoPage(int pinterval)
{
    // Check for out of bounds
    if (m_page_number + pinterval > m_page_count - 1 ||
        m_page_number + pinterval < 0)
    {
        return;
    }

    m_page_number += pinterval;
    Render();
}

void Dodo::Render()
{
    m_ctm = fz_scale(m_zoom / 100, m_zoom / 100);
    m_ctm = fz_pre_rotate(m_ctm, m_rotate);

    /* Render page to an RGB pixmap. */
    fz_try(m_ctx)
    {
        m_pix = fz_new_pixmap_from_page_number(m_ctx, m_doc, m_page_number, m_ctm, fz_device_rgb(m_ctx), 0);
        QImage img(m_pix->samples, m_pix->w, m_pix->h, m_pix->stride, QImage::Format_RGB888);
        m_label->setPixmap(QPixmap::fromImage(img));
    }
    fz_catch(m_ctx)
    {
        fprintf(stderr, "cannot render page: %s\n", fz_caught_message(m_ctx));
        fz_drop_document(m_ctx, m_doc);
        fz_drop_context(m_ctx);
        return;
    }
}

void Dodo::ZoomReset()
{
    m_zoom = 100.0f;
    Render();
}

void Dodo::Zoom(float r)
{
    m_zoom += r;
    Render();
}

bool Dodo::INIT_PDF()
{
    return true;
}

bool Dodo::Open(QString filename, int page_number)
{
    m_ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);

    /* Create a context to hold the exception stack and various caches. */
    if (!m_ctx)
    {
        fprintf(stderr, "cannot create mupdf context\n");
        qFatal("Cannot create mupdf context");
        return false;
    }

    /* Register the default file types to handle. */
    fz_try(m_ctx)
    {
        fz_register_document_handlers(m_ctx);
    }
    fz_catch(m_ctx)
    {
        fz_report_error(m_ctx);
        fprintf(stderr, "cannot register document handlers: %s", fz_caught_message(m_ctx));
        qFatal("Cannot register document handlers");
        fz_drop_context(m_ctx);
        return false;
    }
    

    m_filename = filename.toStdString();

    fz_try(m_ctx)
    {
        m_doc = fz_open_document(m_ctx, m_filename.c_str());
    }

    fz_catch(m_ctx)
    {
        fprintf(stderr, "cannot open document: %s\n", fz_caught_message(m_ctx));
        fz_drop_context(m_ctx);
        fz_drop_document(m_ctx, m_doc);
        return false;
    }

    // Count the number of pages
    fz_try(m_ctx)
    {
        m_page_count = fz_count_pages(m_ctx, m_doc);
    }
    fz_catch(m_ctx)
    {
        fprintf(stderr, "cannot count number of pages: %s\n", fz_caught_message(m_ctx));
        fz_drop_document(m_ctx, m_doc);
        fz_drop_context(m_ctx);
        return false;
    }

    if (page_number < 0 || page_number > m_page_count)
    {
        fprintf(stderr, "incorrect page number");
        fz_drop_document(m_ctx, m_doc);
        fz_drop_context(m_ctx);
        return false;
    }

    m_page_number = page_number;

    /* Compute a transformation matrix for the zoom and rotation desired. */
    /* The default resolution without scaling is 72 dpi. */
    m_ctm = fz_scale(m_zoom / 100, m_zoom / 100);
    m_ctm = fz_pre_rotate(m_ctm, m_rotate);

    /* Render page to an RGB pixmap. */
    fz_try(m_ctx)
    {
        /*m_pix = fz_new_pixmap_from_page_number(m_ctx, m_doc, m_page_number, m_ctm, fz_device_rgb(m_ctx), 0);*/
        fz_page *page = fz_load_page(m_ctx, m_doc, 0);
    
        if (!page)
        {
    
            fz_drop_page(m_ctx, page);
            exit(0);
        }


        fz_matrix transform = fz_identity;
        fz_rotate(m_rotate);
        fz_pre_scale(transform, m_zoom, m_zoom);


        // get transformed page size
        fz_rect bounds;
        fz_bound_page(m_ctx, page);
        fz_transform_rect(bounds, transform);
        fz_round_rect(bounds);

        m_pix =fz_new_pixmap_from_page_number(m_ctx, m_doc, m_page_number, transform, fz_device_bgr(m_ctx), 1);

        auto samples = fz_pixmap_samples(m_ctx, m_pix);
        auto width = fz_pixmap_width(m_ctx, m_pix);
        auto height = fz_pixmap_height(m_ctx, m_pix);

        unsigned char *copied_samples = NULL;

        copied_samples = samples;

        m_image = QImage(copied_samples, width, height, QImage::Format_RGBA8888, NULL, copied_samples);
        m_label->setPixmap(QPixmap::fromImage(m_image));
        m_label->resize(m_label->sizeHint());
    }
    fz_catch(m_ctx)
    {
        fprintf(stderr, "cannot render page: %s\n", fz_caught_message(m_ctx));
        fz_drop_document(m_ctx, m_doc);
        fz_drop_context(m_ctx);
        return false;
    }

    return true;
}

Dodo::~Dodo()
{
    /* Clean up. */

    if (m_pix)
        fz_drop_pixmap(m_ctx, m_pix);

    if (m_doc)
        fz_drop_document(m_ctx, m_doc);

    if (m_ctx)
        fz_drop_context(m_ctx);
}
