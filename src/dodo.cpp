#include "dodo.hpp"

dodo::dodo() noexcept
{
    initConfig();
    initGui();
    initKeybinds();
    // openFile("~/Downloads/test2.pdf");
    m_pixmapCache.setMaxCost(5);
    DPI_FRAC = m_dpix / LOW_DPI;
    QThreadPool::globalInstance()->setMaxThreadCount(QThread::idealThreadCount());
    gotoPage(0);
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
    bool compact              = ui["compact"].value_or(false);

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

    if (compact)
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_panel->layout()->setContentsMargins(0, 0, 0, 0);
        this->setContentsMargins(0, 0, 0, 0);
    }
}

void dodo::initKeybinds() noexcept
{
    std::vector<std::pair<QString, std::function<void()>>> shortcuts = {
        { "o", [this]() { OpenFile(); } },
        { "j", [this]() { ScrollDown(); } },
        { "k", [this]() { ScrollUp(); } },
        { "h", [this]() { ScrollLeft(); } },
        { "l", [this]() { ScrollRight(); } },
        { "Shift+j", [this]() { NextPage(); } },
        { "Shift+k", [this]() { PrevPage(); } },
        { "g,g", [this]() { FirstPage(); } },
        { "Shift+g", [this]() { LastPage(); } },
        { "=", [this]() { ZoomIn(); } },
        { "-", [this]() { ZoomOut(); } },
    };

    for (const auto& [key, func] : shortcuts) {
        auto *sc = new QShortcut(QKeySequence(key), this);
        connect(sc, &QShortcut::activated, func);
    }
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

void dodo::OpenFile() noexcept
{
    QString filepath = QFileDialog::getOpenFileName(this,
                                                    "Open File",
                                                    "",
                                                    "PDF Files (*.pdf);; All Files (*)");
    if (!filepath.isEmpty())
    {
        openFile(filepath);
        // TODO: Remember file location
        gotoPage(0);
    }

}

void dodo::openFile(const QString &fileName) noexcept
{
    m_filename = fileName;
    m_filename.replace("~", QString::fromStdString(getenv("HOME")));
    if (!QFile::exists(m_filename))
    {
        qWarning("file does not exist!: ");
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

void dodo::gotoPage(const int &pageno) noexcept
{
    if (!m_document)
    {
        qWarning("Trying to go to page but no document loaded");
        return;
    }

    m_pageno = pageno;

    m_panel->setPageNo(m_pageno + 1);
    // TODO: Handle file content change detection

    if (m_highResCache.contains(pageno))
    {
        QPixmap *cached = m_highResCache.object(pageno);
        m_pix_item->setPixmap(*cached);
        m_pix_item->setScale(1.0);
        return;
    }

    renderPage(pageno, LOW_DPI);

    // Schedule high-DPI render after short delay
    QTimer::singleShot(300, this, [=]() {
        if (pageno == m_pageno) {
            renderPage(pageno, m_dpix, false);
        }
    });

    //prefetchAround(m_pageno);
}

void dodo::handleRenderResult(int pageno, QImage image, bool lowQuality)
{
    if (pageno != m_pageno || image.isNull())
        return;


    QPixmap pix = QPixmap::fromImage(image);
    if (lowQuality) {
        m_pixmapCache.insert(pageno, new QPixmap(pix));
        m_pix_item->setScale(DPI_FRAC);
    } else {
        m_highResCache.insert(pageno, new QPixmap(pix));
        m_pix_item->setScale(1.0);
    }

    m_pix_item->setPixmap(pix);
}

void dodo::renderPage(const int &pageno,
                      const float &dpi,
                      const bool &lowQuality) noexcept
{

    if (!lowQuality && m_highResCache.contains(pageno)) {
        m_pix_item->setPixmap(*m_highResCache.object(pageno));
        m_pix_item->setScale(1.0);
        return;
    }

    if (lowQuality && m_pixmapCache.contains(pageno)) {
        m_pix_item->setPixmap(*m_pixmapCache.object(pageno));
        m_pix_item->setScale(DPI_FRAC);
        return;
    }

    auto *task = new RenderTask(m_document.get(), pageno, dpi, dpi, lowQuality, this);
    connect(task, &RenderTask::finished, this,
            &dodo::handleRenderResult,
            Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(task);
}

bool dodo::isPrefetchPage(int page, int currentPage) noexcept {
    // Prefetch next and previous page only
    return page == currentPage + 1 || page == currentPage - 1;
}

void dodo::prefetchAround(int currentPage) noexcept {
    for (int offset = -1; offset <= 1; ++offset) {
        int page = currentPage + offset;
        if (page >= 0 && page < m_document->numPages()) {
            renderPage(page, LOW_DPI);
        }
    }
}

void dodo::FirstPage() noexcept
{
    gotoPage(0);
}

void dodo::LastPage() noexcept
{
    gotoPage(m_total_pages - 1);
}


void dodo::NextPage() noexcept
{
    if (m_pageno < m_total_pages - 1)
    {
        gotoPage(m_pageno + 1);
    }
}

void dodo::PrevPage() noexcept
{
    if (m_pageno > 0)
    {
        gotoPage(m_pageno - 1);
    }
}


void dodo::ZoomIn() noexcept
{
    if (m_scale_factor < 5.0)
    {
        m_gview->scale(1.1, 1.1);
        m_scale_factor *= 1.1;
    }
}

void dodo::ZoomOut() noexcept
{
    if (m_scale_factor > 0.2)
    {
        m_gview->scale(0.9, 0.9);
        m_scale_factor *= 0.9;
    }
}


void dodo::ScrollDown() noexcept
{
    auto vscrollbar = m_gview->verticalScrollBar();
    vscrollbar->setValue(vscrollbar->value() + 50);
}

void dodo::ScrollUp() noexcept
{
    auto vscrollbar = m_gview->verticalScrollBar();
    vscrollbar->setValue(vscrollbar->value() - 50);
}

void dodo::ScrollLeft() noexcept
{
    auto hscrollbar = m_gview->horizontalScrollBar();
    hscrollbar->setValue(hscrollbar->value() - 50);
}

void dodo::ScrollRight() noexcept
{
    auto hscrollbar = m_gview->horizontalScrollBar();
    hscrollbar->setValue(hscrollbar->value() + 50);
}
