#include "dodo.hpp"

dodo::dodo() noexcept
{
    initConfig();
    initGui();
    initKeybinds();
    openFile("~/Downloads/test2.pdf");
    gotoPage(0);

    m_pixmapCache.setMaxCost(5);

    DPI_FRAC = m_dpix / LOW_DPI;
}

dodo::~dodo() noexcept
{}

void dodo::initConfig() noexcept
{
    QDir config_dir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    auto config_file_path = config_dir.filePath("config.toml");

    auto toml = toml::parse_file(config_file_path.toStdString());

    auto ui = toml["ui"];
    std::string theme         = ui["theme"].value_or("dark");
    std::string accent_color  = ui["accent_color"].value_or("#3daee9");
    bool show_toolbar         = ui["show_toolbar"].value_or(true);
    bool show_statusbar       = ui["show_statusbar"].value_or(true);
    bool fullscreen           = ui["fullscreen"].value_or(false);
    double zoom_level         = ui["zoom_level"].value_or(1.0);

    auto rendering = toml["rendering"];
    double dpi_x              = rendering["dpi_x"].value_or(144.0);
    double dpi_y              = rendering["dpi_y"].value_or(144.0);
    bool antialiasing         = rendering["antialiasing"].value_or(true);
    int cache_pages           = rendering["cache_pages"].value_or(50);

    auto behavior = toml["behavior"];
    bool remember_last_page   = behavior["remember_last_page"].value_or(true);
    std::string scroll_mode   = behavior["scroll_mode"].value_or("vertical");
    bool enable_prefetch      = behavior["enable_prefetch"].value_or(true);
    int prefetch_distance     = behavior["prefetch_distance"].value_or(2);

    auto keys = toml["keybindings"];
    std::string next_page     = keys["next_page"].value_or("Right");
    std::string prev_page     = keys["prev_page"].value_or("Left");
    std::string zoom_in       = keys["zoom_in"].value_or("Ctrl+Plus");
    std::string zoom_out      = keys["zoom_out"].value_or("Ctrl+Minus");
    std::string toggle_fs     = keys["toggle_fullscreen"].value_or("F11");
    std::string quit          = keys["quit"].value_or("Ctrl+Q");

    auto session = toml["session"];
    std::string last_file     = session["last_file"].value_or("");
    int last_page             = session["last_page"].value_or(0);
    int window_width          = session["window_width"].value_or(1024);
    int window_height         = session["window_height"].value_or(768);

}

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
    m_layout->addWidget(m_panel);
    m_gview->setScene(m_gscene);
    this->setCentralWidget(widget);

    // Setup graphics view
    m_gscene->addItem(m_pix_item);
    m_gview->setResizeAnchor(QGraphicsView::AnchorUnderMouse);

    m_gview->setBackgroundBrush(QColor::fromString(m_default_bg));
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
    m_panel->setTotalPageCount(m_total_pages);
    m_panel->setFileName(m_filename);
}

/*
void dodo::gotoPage(const int &pageno) noexcept
{
    m_pageno = pageno;

    m_panel->setPageNo(m_pageno + 1);
    // TODO: Handle file content change detection

    if (QPixmap *cached = m_pixmapCache.object(pageno))
    {
        m_pix_item->setPixmap(*cached);
        return;
    }

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
*/

void dodo::gotoPage(const int &pageno) noexcept
{
    m_pageno = pageno;

    m_panel->setPageNo(m_pageno + 1);
    // TODO: Handle file content change detection

    if (QPixmap *cached = m_pixmapCache.object(pageno))
    {
        m_pix_item->setPixmap(*cached);
        return;
    }

    renderPage(pageno);

    prefetchPages({pageno - 2, pageno - 1, pageno + 1, pageno + 2 });
}

void dodo::renderPage(const int &pageno) noexcept
{
    QThread *thread = new QThread;
    auto *worker = new RenderWorker(m_document.get(), pageno, m_dpi_x, m_dpi_y);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &RenderWorker::process);
    connect(worker, &RenderWorker::finished, this, [=](int pageNum, const QImage &image) {

        thread->quit();
        thread->wait();
        worker->deleteLater();
        thread->deleteLater();

        if (m_pageno != pageNum)
            return;

        if (!image.isNull())
        {
            QPixmap pix = QPixmap::fromImage(image);
            m_pixmapCache.insert(pageNum, new QPixmap(pix));
            if (pageNum == m_pageno) {
                m_pix_item->setPixmap(pix);  // display if it's the current page
            }
        }
    });

    thread->start();
}

void dodo::prefetchPages(const std::vector<int> &pages) noexcept
{
    for (const int &page : pages)
    {
        if (page < 0 || page >= m_document->numPages()) // boundary
            continue;

        if (m_pixmapCache.contains(page))
            continue;  // already cached

        QThread *thread = new QThread;
        auto *worker = new RenderWorker(m_document.get(), page, m_dpi_x, m_dpi_y);
        worker->moveToThread(thread);

        connect(thread, &QThread::started, worker, &RenderWorker::process);
        connect(worker,
                &RenderWorker::finished,
                this, [=](int pageNum, const QImage &image) {
                thread->quit();
                thread->wait();
                worker->deleteLater();
                thread->deleteLater();

                if (!image.isNull())
                {
                QPixmap pix = QPixmap::fromImage(image);
                m_pixmapCache.insert(pageNum, new QPixmap(pix));
                }
                });

        thread->start();
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
