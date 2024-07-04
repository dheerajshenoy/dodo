#include "dodo.hpp"
#include <mupdf/pdf.h>
#include <mupdf/pdf/annot.h>
#include <mupdf/pdf/document.h>

Dodo::Dodo(int argc, char** argv, QWidget *parent)
{
    //m_layout->addWidget(m_label);
    m_layout->addWidget(m_scrollarea);
    m_layout->addWidget(m_statusbar);

    m_commandbar->hide();
    m_layout->addWidget(m_commandbar);

    m_scrollarea->setWidget(m_label);
    m_scrollarea->setAlignment(Qt::AlignmentFlag::AlignCenter);
    m_scrollarea->setWidgetResizable(true);

    m_widget->setLayout(m_layout);
    m_label->setAlignment(Qt::AlignmentFlag::AlignCenter);
    this->setCentralWidget(m_widget);

    connect(m_commandbar, &CommandBar::searchMode, this, [&](QString searchText) {
        Dodo::SearchText(searchText);
    });

    connect(m_commandbar, &CommandBar::searchClear, this, [&]() {
        Dodo::SearchReset();
    });

    HandleMenubar();
    HandleActions();
    SetKeyBinds();
    this->show();

    if (argc > 1)
        Open(QString(argv[1]));

}

void Dodo::FitToWidth()
{
    /*swaping pix_width and pix_width when pdf is rotated either 90,-90,-270,270 degrees
     * in this case, as the pdf is rotated,the pdf hight and widht will interchange
     */
    float tem_pix_height=0.0f,tem_pix_width=0.0f;
    if(m_rotate==90 || m_rotate==-270 || m_rotate==270||m_rotate==-90)
    {
        tem_pix_height=m_pix->w;
        tem_pix_width=m_pix->h;

        m_pix->h=tem_pix_height;
        m_pix->w=tem_pix_width;

    }

    //calculating the m_scale value for which page width fits the window hight
    float window_width=this->width();
    float scale=window_width - 200;
    scale=scale/m_pix->w;

    m_zoom = int(scale);

    //checking if previous page had any string to search and then searching same string on the current page.
    Render();

    /*restoring the pix_height and pix_width value back to the original as the fitting is done now.*/
    if(m_rotate==90 || m_rotate==-270 || m_rotate==270||m_rotate==-90)
    {
        m_pix->h=tem_pix_width;
        m_pix->w=tem_pix_height;
    }
}

void Dodo::FitToHeight()
{

}

void Dodo::FitToWindow()
{

}

void Dodo::SetKeyBinds()
{
    QShortcut *kb_zoomin = new QShortcut(QKeySequence("="), this, [&]() {
        this->Zoom(0.3);
    });

    QShortcut *kb_zoomout = new QShortcut(QKeySequence("-"), this, [&]() {
        this->Zoom(-0.3);
    });

    QShortcut *kb_zoomreset = new QShortcut(QKeySequence("+"), this, [&]() {
        this->ZoomReset();
    });

    QShortcut *kb_goto_top = new QShortcut(QKeySequence("g,g"), this, [&]() {
        this->GotoFirstPage();
    });

    QShortcut *kb_goto_bottom = new QShortcut(QKeySequence("Shift+g"), this, [&]() {
        this->GotoLastPage();
    });

    QShortcut *kb_open = new QShortcut(QKeySequence("Ctrl+o"), this, [&]() {
        QString filename = QFileDialog::getOpenFileName(this, "Open file", nullptr, "PDF Files (*.pdf);;");
        this->Open(filename);
    });

    QShortcut *kb_next_page = new QShortcut(QKeySequence("Shift+j"), this, [&]() {
        this->NextPage();
    });

    QShortcut *kb_prev_page = new QShortcut(QKeySequence("Shift+k"), this, [&]() {
        this->PrevPage();
    });

    QShortcut *kb_scroll_down = new QShortcut(QKeySequence("j"), this, [&]() {
        this->ScrollVertical(1, 30);
    });

    QShortcut *kb_scroll_down_more = new QShortcut(QKeySequence("Ctrl+d"), this, [&]() {
        this->ScrollVertical(1, 300);
    });

    QShortcut *kb_scroll_up = new QShortcut(QKeySequence("k"), this, [&]() {
        this->ScrollVertical(-1, 30);
    });

    QShortcut *kb_scroll_up_more = new QShortcut(QKeySequence("Ctrl+u"), this, [&]() {
        this->ScrollVertical(-1, 300);
    });

    QShortcut *kb_scroll_left = new QShortcut(QKeySequence("h"), this, [&]() {
        this->ScrollHorizontal(-1, 30);
    });

    QShortcut *kb_scroll_right = new QShortcut(QKeySequence("l"), this, [&]() {
        this->ScrollHorizontal(1, 30);
    });

    QShortcut *kb_rotate_clock = new QShortcut(QKeySequence(","), this, [&]() {
        this->Rotate(90);
    });

    QShortcut *kb_rotate_anticlock = new QShortcut(QKeySequence("."), this, [&]() {
        this->Rotate(-90);
    });

    QShortcut *kb_reset_view = new QShortcut(QKeySequence("Shift+R"), this, [&]() {
        this->ResetView();
    });

    QShortcut *kb_fit_width = new QShortcut(QKeySequence("w"), this, [&]() {
        this->FitToWidth();
    });

    QShortcut *kb_search = new QShortcut(QKeySequence("/"), this, [&]() {
        m_commandbar->Search();
    });

}

void Dodo::GotoPage(int pagenum)
{
    // Check for out of bounds
    if (pagenum > m_page_count - 1 || pagenum < 0)
        return;

    m_cur_page_num = pagenum;
    m_statusbar->SetCurrentPage(m_cur_page_num);
    Render();
}

void Dodo::Render(float dpi)
{

    if (dpi != -1)
    {
        m_zoom = dpi / 72;
        m_ctm = fz_pre_scale(m_ctm, m_zoom, m_zoom);
    }

    else {
        m_ctm = fz_scale(m_zoom, m_zoom);
        m_ctm = fz_pre_rotate(m_ctm, m_rotate);
    }

    m_page = fz_load_page(m_ctx, m_doc, m_cur_page_num);
    if (!m_page)
    {
        qFatal("Could not load page! Exiting");
        fz_drop_page(m_ctx, m_page);
        exit(-1);
    }

    fz_rect bounds = fz_bound_page(m_ctx, m_page);
    bounds = fz_transform_rect(bounds, m_ctm);
    fz_irect bbox = fz_round_rect(bounds);

    float w = bbox.x1 - bbox.x0;
    float h = bbox.y1 - bbox.y0;

    w *= m_zoom;
    h *= m_zoom;

    // Render page to an RGB pixmap.
    fz_try(m_ctx)
    {
        /*m_pix = fz_new_pixmap_from_page_number(m_ctx, m_doc, m_cur_page_num, m_ctm, fz_device_rgb(m_ctx), 1);*/
        m_pix = fz_new_pixmap(m_ctx, fz_device_rgb(m_ctx), w, h, nullptr, 1);
        /*m_pix = fz_new_pixmap_with_bbox(m_ctx, fz_device_rgb(m_ctx), bbox, nullptr, 1);*/
        fz_clear_pixmap_with_value(m_ctx, m_pix, 0xff);
        fz_device *dev = fz_new_draw_device(m_ctx, m_ctm, m_pix);
        fz_run_page(m_ctx, m_page, dev, m_ctm, nullptr);
        fz_drop_device(m_ctx, dev);

        m_image = QImage(m_pix->samples, w, h, m_pix->stride, QImage::Format_RGBA8888);

        m_label->setFixedSize(m_image.size());

        if (!m_search_text.isNull() && !m_search_text.isEmpty())
        {
            SearchText(m_search_text);
        }
        else m_label->setPixmap(QPixmap::fromImage(m_image));
    }
    fz_catch(m_ctx)
    {
        fprintf(stderr, "cannot render page: %s\n", fz_caught_message(m_ctx));
        fz_drop_document(m_ctx, m_doc);
        fz_drop_context(m_ctx);
        exit(0);
    }
    
    fz_drop_page(m_ctx, m_page);
    fz_drop_pixmap(m_ctx, m_pix);

}
void Dodo::ZoomReset()
{
    m_zoom = 1.0f;
    Render();
}

void Dodo::Zoom(float r)
{
    if (m_zoom + r <= 0) return;

    m_zoom += r;
    Render();
}

bool Dodo::Open(QString filename, int page_number)
{
    m_ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);

    /* Create a context to hold the exception stack and various caches. */
    if (!m_ctx)
    {
        fprintf(stderr, "cannot create mupdf context\n");
        qFatal("Cannot create mupdf context");
        fz_drop_context(m_ctx);
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

    m_filename = QString::fromStdString(filename.toStdString());

    fz_try(m_ctx)
    {
        m_doc = fz_open_document(m_ctx, m_filename.toStdString().c_str());
    }

    fz_catch(m_ctx)
    {
        fprintf(stderr, "cannot open document: %s\n", fz_caught_message(m_ctx));
        fz_drop_context(m_ctx);
        fz_drop_document(m_ctx, m_doc);
        return false;
    }

    if (fz_needs_password(m_ctx, m_doc))
    {
        bool ok;

        QString password;

        do {
            QInputDialog get_password(this);
            password = QInputDialog::getText(this, tr("Enter Password"), tr("Password"), QLineEdit::Normal, "", &ok);

            if (!ok)
                exit(0);
        } while (!fz_authenticate_password(m_ctx, m_doc, password.toLatin1().data()));

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

    m_cur_page_num = page_number;
    m_statusbar->SetFileName(m_filename);
    m_statusbar->SetFilePageCount(m_page_count);
    m_statusbar->SetCurrentPage(page_number);

    Render();
    Annotate();

    return true;
}

void Dodo::Rotate(float angle)
{
    m_rotate += angle;
    m_rotate=(float)((int)m_rotate % 360); 

    this->Render();
}

void Dodo::ResetView()
{
    m_rotate = 0.0f;
    m_zoom = 1.0f;

    this->Render();
}

void Dodo::ScrollVertical(int direction, int amount)
{
    if (direction == 1)
    {
        m_vscroll->setValue(m_vscroll->value() + amount);
        /*m_scrollarea->scroll(0, -m_scroll_len);*/
    } else {
        m_vscroll->setValue(m_vscroll->value() - amount);
    }
}

void Dodo::ScrollHorizontal(int direction, int amount)
{

    if (direction == 1)
    {
        m_hscroll->setValue(m_hscroll->value() + amount);
    } else {
        m_hscroll->setValue(m_hscroll->value() - amount);
    }
}

int Dodo::TotalSearchCount(QString text)
{
    int search_count = 0;


    // Iterate through each page
    for (int i = 0; i < m_page_count; ++i) {

        // Search for the text
        int count = fz_search_page_number(m_ctx, m_doc, i, text.toLatin1().data(), nullptr, nullptr, HIT_MAX_COUNT);

        // Add to total count
        search_count += count;
    }

    return search_count;
}

int Dodo::SearchText(QString text)
{

    m_search_text = text;
    m_g_image = m_image;
    /*m_statusbar->SetTotalSearchCount(TotalSearchCount(text));*/
    fz_quad hit_box[HIT_MAX_COUNT];
    /*int *hit_mark[HIT_MAX_COUNT];*/

    m_search_count = fz_search_page_number(m_ctx, m_doc, m_cur_page_num, text.toLatin1().data(), nullptr, hit_box, HIT_MAX_COUNT);

    if (m_search_count > 0)
    {
        for(int i=0; i < m_search_count; i++)
        {
            QPainter painter(&m_g_image);
            double width,height;
            double label_x,label_y;
            double label_width,label_height;

            //using pen to hide the border of the rect to used with the qpainter
            QPen pen;
            pen.setStyle(Qt::NoPen);
            painter.setPen(pen);

            //calculating the width and heigth or the search text
            width = hit_box[i].lr.x - hit_box[i].ul.x;
            height = hit_box[i].lr.y - hit_box[i].ul.y;

            //calculation the x and y cordinate where qpainter will be set
            if(m_rotate == 0)
            {
                label_x = hit_box[i].ll.x * m_zoom;
                label_y = hit_box[i].ul.y * m_zoom;
                label_width = width * m_zoom;
                label_height = height * m_zoom;
            }

            else if(m_rotate == 90 || m_rotate == -270)//if rotated clockwise or anti clockwise
            {
                label_width = height * m_zoom;
                label_height = width * m_zoom;

                label_x = m_label->width() - hit_box[i].ul.y * m_zoom - label_width;
                label_y = hit_box[i].ul.x * m_zoom;
            }

            else if(m_rotate == 180 || m_rotate == -180)//if rotated clockwise or anti clockwise
            {
                label_x = m_label->width() - hit_box[i].ul.x * m_zoom - width * m_zoom;
                label_y = m_label->height() - hit_box[i].ul.y * m_zoom - height * m_zoom;
                label_width = width * m_zoom;
                label_height = height  * m_zoom;
            }

            else if(m_rotate == 270 || m_rotate == -90)//if rotated clockwise or anti clockwise
            {
                label_width= height * m_zoom;
                label_height = width * m_zoom;
                label_x = hit_box[i].ul.y * m_zoom;
                label_y = m_label->height() - hit_box[i].ul.x * m_zoom - label_height;
            }

            QRectF rect(label_x, label_y, label_width, label_height);

            painter.drawRect(rect);
            painter.fillRect(rect, QBrush(QColor(255, 0, 0, 128)));
            painter.end();
        }

        m_label->setPixmap(QPixmap::fromImage(m_g_image));
    }

    return m_search_count;
}

void Dodo::SearchReset()
{
    qDebug() << "DD";
    m_search_count = 0;
    m_search_index = -1;
    m_search_text = "";
    Render();
}

void Dodo::Search_Goto_Next()
{
    if (m_search_index >= m_search_count) return;

    m_search_index++;
}

void Dodo::Search_Goto_Prev()
{
    if (m_search_index <= 0) return;

    m_search_index--;

}

void Dodo::GotoLastPage()
{
    GotoPage(m_page_count - 1);
}

void Dodo::GotoFirstPage()
{
    GotoPage(0);
}

void Dodo::NextPage()
{
    GotoPage(GetCurrentPage() + 1);
}

void Dodo::PrevPage()
{
    GotoPage(GetCurrentPage() - 1);
}

int Dodo::GetCurrentPage()
{
    return m_cur_page_num;
}

void Dodo::Search_Highlight_Next_Item()
{

}

void Dodo::Search_Highlight_Prev_Item()
{

}

void Dodo::Annotate()
{
    pdf_page *page = pdf_load_page(m_ctx, (pdf_document*) m_doc, m_cur_page_num);

    switch(m_annot_type)
    {
        case AnnotType::HIGHLIGHT:
            {
                fz_point a = {206, 69} , b = {291, 69};
                pdf_annot *annot = pdf_create_annot(m_ctx, page, pdf_annot_type::PDF_ANNOT_HIGHLIGHT);

                /*qDebug() << pdf_annot_contents(m_ctx, annot);*/
            }
            break;

        case AnnotType::UNDERLINE:
            break;

        case AnnotType::STRIKETHROUGH:
            break;


    }
}

void Dodo::SetAnnotColor(const QColor color)
{
    m_annot_color = color;
}

QColor Dodo::GetAnnotColor()
{
    return m_annot_color;
}

void Dodo::SetAnnotType(AnnotType type)
{
    m_annot_type = type;
}

AnnotType Dodo::GetAnnotType()
{
    return m_annot_type;
}

void Dodo::HandleMenubar()
{

    m_menubar = new QMenuBar();
    m_menu__file = new QMenu("File");
    m_menu__edit = new QMenu("Edit");
    m_menu__view = new QMenu("View");
    m_menu__tools = new QMenu("Tools");
    m_menu__about = new QMenu("About");

    m_menubar->addMenu(m_menu__file);
    m_menubar->addMenu(m_menu__edit);
    m_menubar->addMenu(m_menu__view);
    m_menubar->addMenu(m_menu__tools);
    m_menubar->addMenu(m_menu__about);

    this->setMenuBar(m_menubar);

}

void Dodo::HandleActions()
{
    HandleFileMenuActions();
    HandleEditMenuActions();
    HandleViewMenuActions();
}

void Dodo::HandleFileMenuActions()
{
    m_action__file_open = new QAction("Open");
    m_action__file_open_recents = new QAction("Open Recent");
    m_action__file_properties = new QAction("Properties");
    m_action__file_close = new QAction("Open Recent");
    m_action__file_quit = new QAction("Quit");

    m_menu__file->addAction(m_action__file_open);
    m_menu__file->addAction(m_action__file_open_recents);
    m_menu__file->addAction(m_action__file_properties);
    m_menu__file->addAction(m_action__file_close);
    m_menu__file->addAction(m_action__file_quit);
}

void Dodo::HandleEditMenuActions()
{
}

void Dodo::HandleViewMenuActions()
{

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
