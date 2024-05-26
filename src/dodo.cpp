#include "dodo.hpp"

Dodo::Dodo(QWidget *parent)
{
    m_layout->addWidget(m_label);
    m_widget->setLayout(m_layout);
    m_layout->setAlignment(Qt::AlignmentFlag::AlignCenter);
    this->setCentralWidget(m_widget);
    INIT_PDF();
    SetKeyBinds();
    Open("/home/neo/test.pdf", 15);
    this->show();
}

void Dodo::SetKeyBinds()
{
    QShortcut *kb_zoomin = new QShortcut(QKeySequence("="), this, [=]() {
        this->Zoom(+5);
    });

    QShortcut *kb_zoomout = new QShortcut(QKeySequence("-"), this, [=]() {
        this->Zoom(-5);
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
    if (m_page_number + pinterval > m_page_count)
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
        fz_drop_context(m_ctx);
        return false;
    }

    m_filename = filename.toStdString();

    // Open the document
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
        m_pix = fz_new_pixmap_from_page_number(m_ctx, m_doc, m_page_number, m_ctm, fz_device_rgb(m_ctx), 0);
        QImage img(m_pix->samples, m_pix->w, m_pix->h, m_pix->stride, QImage::Format_RGB888);
        m_label->setPixmap(QPixmap::fromImage(img));
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
    fz_drop_pixmap(m_ctx, m_pix);
    fz_drop_document(m_ctx, m_doc);
    fz_drop_context(m_ctx);
}
