#include "dodo.hpp"

dodo::dodo() noexcept
{
    initGui();
    initKeybinds();
    openFile("~/Downloads/test.pdf");
    gotoPage(0);

    m_pixmapCache.setMaxCost(50);
}

dodo::~dodo() noexcept
{}

void dodo::initKeybinds() noexcept
{
    QShortcut *sc__scroll_down = new QShortcut(QKeySequence("j"), this);
    QShortcut *sc__scroll_up = new QShortcut(QKeySequence("k"), this);
    QShortcut *sc__scroll_left = new QShortcut(QKeySequence("h"), this);
    QShortcut *sc__scroll_right = new QShortcut(QKeySequence("l"), this);
    QShortcut *sc__next_page = new QShortcut(QKeySequence("Shift+j"), this);
    QShortcut *sc__prev_page = new QShortcut(QKeySequence("Shift+k"), this);
    QShortcut *sc__first_page = new QShortcut(QKeySequence("g,g"), this);
    QShortcut *sc__last_page = new QShortcut(QKeySequence("Shift+g"), this);
    QShortcut *sc__zoom_in = new QShortcut(QKeySequence("="), this);
    QShortcut *sc__zoom_out = new QShortcut(QKeySequence("-"), this);

    connect(sc__scroll_left, &QShortcut::activated, this, &dodo::scrollLeft);
    connect(sc__scroll_right, &QShortcut::activated, this, &dodo::scrollRight);
    connect(sc__scroll_down, &QShortcut::activated, this, &dodo::scrollDown);
    connect(sc__scroll_up, &QShortcut::activated, this, &dodo::scrollUp);
    connect(sc__next_page, &QShortcut::activated, this, &dodo::nextPage);
    connect(sc__prev_page, &QShortcut::activated, this, &dodo::prevPage);
    connect(sc__first_page, &QShortcut::activated, this, &dodo::firstPage);
    connect(sc__last_page, &QShortcut::activated, this, &dodo::lastPage);
    connect(sc__zoom_in, &QShortcut::activated, this, &dodo::zoomIn);
    connect(sc__zoom_out, &QShortcut::activated, this, &dodo::zoomOut);
}

void dodo::initGui() noexcept
{
    QWidget *widget = new QWidget();
    widget->setLayout(m_layout);
    m_layout->addWidget(m_gview);
    m_gview->setScene(m_gscene);
    this->setCentralWidget(widget);

    // Setup graphics view
    m_gscene->addItem(m_pix_item);
}

void dodo::openFile(const QString &fileName) noexcept
{
    m_filename = fileName;
    m_filename.replace("~", QString::fromStdString(getenv("HOME")));
    if (!QFile::exists(m_filename))
    {
        qDebug() << "file does not exist!: " << fileName << "\n";
        return;
    }

    m_document = Poppler::Document::load(m_filename);
    if (!m_document)
    {
        qWarning("Failed to load PDF");
        return;
    }

    if (m_document->isLocked())
    {
        // TODO: Handle locked pdf files
        qDebug() << "File is password protected!";
        return;
    }

    m_total_pages = m_document->numPages();
}

void dodo::gotoPage(const int &pageno) noexcept
{
    m_pageno = pageno;
    QPixmap *cached = m_pixmapCache.object(pageno);

    // TODO: Handle file content change detection

    if (cached)
    {
        m_pix_item->setPixmap(*cached);
        return;
    } else {
        auto page = m_document->page(pageno);
        if (!page)
        {
            qWarning("Page not found!");
            return;
        }
        QImage image = page->renderToImage(m_dpi_x, m_dpi_y);

        if (image.isNull())
        {
            qWarning("Failed to render page");
            return;
        }

        auto pix = QPixmap::fromImage(image);
        m_pixmapCache.insert(pageno, new QPixmap(pix));
        m_pix_item->setPixmap(pix);
    }

}

void dodo::firstPage() noexcept
{
    gotoPage(0);
}

void dodo::lastPage() noexcept
{
    gotoPage(m_total_pages - 1);
}


void dodo::nextPage() noexcept
{
    if (m_pageno < m_total_pages - 1)
    {
        m_pageno++;
        gotoPage(m_pageno);
    }
}

void dodo::prevPage() noexcept
{
    if (m_pageno > 0)
    {
        m_pageno--;
        gotoPage(m_pageno);
    }
}


void dodo::zoomIn() noexcept
{
    if (m_scale_factor < 5.0)
    {
        m_gview->scale(1.1, 1.1);
        m_scale_factor *= 1.1;
    }
}

void dodo::zoomOut() noexcept
{
    if (m_scale_factor > 0.2)
    {
        m_gview->scale(0.9, 0.9);
        m_scale_factor *= 0.9;
    }
}


void dodo::scrollDown() noexcept
{
    auto vscrollbar = m_gview->verticalScrollBar();
    vscrollbar->setValue(vscrollbar->value() + 50);
}

void dodo::scrollUp() noexcept
{
    auto vscrollbar = m_gview->verticalScrollBar();
    vscrollbar->setValue(vscrollbar->value() - 50);
}

void dodo::scrollLeft() noexcept
{
    auto hscrollbar = m_gview->horizontalScrollBar();
    hscrollbar->setValue(hscrollbar->value() - 50);
}

void dodo::scrollRight() noexcept
{
    auto hscrollbar = m_gview->horizontalScrollBar();
    hscrollbar->setValue(hscrollbar->value() + 50);
}
