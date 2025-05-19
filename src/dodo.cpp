#include "dodo.hpp"
#include "GotoLinkItem.hpp"

dodo::dodo() noexcept
{
    initGui();
    initConfig();
    DPI_FRAC = m_dpix / LOW_DPI;
    initKeybinds();
    QThreadPool::globalInstance()->setMaxThreadCount(QThread::idealThreadCount());
    openFile("~/Downloads/math.pdf");
    gotoPage(0);

    m_page_history_list.reserve(m_page_history_limit);
}

dodo::~dodo() noexcept
{}

void dodo::initConfig() noexcept
{
    QDir config_dir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    auto config_file_path = config_dir.filePath("config.toml");

    auto toml = toml::parse_file(config_file_path.toStdString());

    auto ui = toml["ui"];
    std::string theme = ui["theme"].value_or("dark");
    std::string accent_color = ui["accent_color"].value_or("#3daee9");
    bool show_toolbar = ui["show_toolbar"].value_or(true);
    bool show_statusbar = ui["show_statusbar"].value_or(true);
    bool fullscreen = ui["fullscreen"].value_or(false);
    double zoom_level = ui["zoom_level"].value_or(1.0);
    bool compact = ui["compact"].value_or(false);
    m_link_boundary_box_enabled = ui["link_boundary_box"].value_or(false);

    auto rendering = toml["rendering"];
    m_dpix = rendering["dpix"].value_or(300.0);
    m_dpiy = rendering["dpiy"].value_or(300.0);
    int cache_pages = rendering["cache_pages"].value_or(50);

    auto behavior = toml["behavior"];
    bool remember_last_page = behavior["remember_last_page"].value_or(true);
    m_prefetch_enabled = behavior["enable_prefetch"].value_or(true);
    m_prefetch_distance = behavior["prefetch_distance"].value_or(2);
    m_page_history_limit = behavior["page_history"].value_or(100);

    auto keys = toml["keybindings"];

    for (auto &[action, value] : *keys.as_table())
    {
        if (value.is_value())
            setupKeybinding(action.str(), value.value_or<std::string>(""));
    }

    auto session = toml["session"];
    std::string last_file     = session["last_file"].value_or("");
    int last_page             = session["last_page"].value_or(0);
    int window_width          = session["window_width"].value_or(1024);
    int window_height         = session["window_height"].value_or(768);

    if (compact)
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        // m_panel->layout()->setContentsMargins(0, 0, 0, 0);
        m_panel->setContentsMargins(0, 0, 0, 0);
        this->setContentsMargins(0, 0, 0, 0);
    }

    m_pixmapCache.setMaxCost(cache_pages);
    m_highResCache.setMaxCost(cache_pages);
}

void dodo::initKeybinds() noexcept
{
    std::vector<std::pair<QString, std::function<void()>>> shortcuts = {

        { "<escape>", [this]() { Escape(); } },
        { "/", [this]() { Search(); } },
        { "n", [this]() { nextHit(); } },
        { "Shift+n", [this]() { prevHit(); } },
        { "Ctrl+o", [this]() { GoBackHistory(); } },
        { "o", [this]() { OpenFile(); } },
        { "j", [this]() { ScrollDown(); } },
        { "k", [this]() { ScrollUp(); } },
        { "h", [this]() { ScrollLeft(); } },
        { "l", [this]() { ScrollRight(); } },
        { "Shift+j", [this]() { NextPage(); } },
        { "Shift+k", [this]() { PrevPage(); } },
        { "g,g", [this]() { FirstPage(); } },
        { "Shift+g", [this]() { LastPage(); } },
        { "0", [this]() { ZoomReset(); } },
        { "=", [this]() { ZoomIn(); } },
        { "-", [this]() { ZoomOut(); } },
        { "<", [this]() { RotateAntiClock(); } },
        { ">", [this]() { RotateClock(); } },
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


    // Menu Bar
    QMenuBar *menubar = menuBar();

    // --- File Menu ---
    QMenu *fileMenu = menubar->addMenu("File");
    fileMenu->addAction("Open", this, &dodo::OpenFile);
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, &QMainWindow::close);

    // --- View Menu ---
    QMenu *viewMenu = menubar->addMenu("View");
    m_actionZoomIn = viewMenu->addAction("Zoom In", this, &dodo::ZoomIn);
    m_actionZoomOut = viewMenu->addAction("Zoom Out", this, &dodo::ZoomOut);
    viewMenu->addSeparator();
    m_actionFitWidth = viewMenu->addAction("Fit to Width", this, &dodo::FitToWidth);
    m_actionFitHeight = viewMenu->addAction("Fit to Height", this, &dodo::FitToHeight);

    // --- Navigation Menu ---
    QMenu *navMenu = menubar->addMenu("Navigation");
    m_actionFirstPage = navMenu->addAction("First Page\tg,g", this, &dodo::FirstPage);
    m_actionPrevPage = navMenu->addAction("Previous Page\tShift+k", this, &dodo::PrevPage);
    m_actionNextPage = navMenu->addAction("Next Page\tShift+j", this, &dodo::NextPage);
    m_actionLastPage = navMenu->addAction("Last Page\tShift+g", this, &dodo::LastPage);

    updateUiEnabledState();
}

void dodo::updateUiEnabledState() noexcept
{
    const bool hasFile = m_document.get();

    m_actionZoomIn->setEnabled(hasFile);
    m_actionZoomOut->setEnabled(hasFile);
    m_actionFitWidth->setEnabled(hasFile);
    m_actionFitHeight->setEnabled(hasFile);

    m_actionFirstPage->setEnabled(hasFile);
    m_actionPrevPage->setEnabled(hasFile);
    m_actionNextPage->setEnabled(hasFile);
    m_actionLastPage->setEnabled(hasFile);
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

    updateUiEnabledState();
}


void dodo::RotateClock() noexcept
{
    if (!m_pix_item)
        return;

    m_rotation = (m_rotation + 90) % 360;
    m_pix_item->setRotation(m_rotation);
    m_gview->centerOn(m_pix_item);
}

void dodo::RotateAntiClock() noexcept
{
    if (!m_pix_item)
        return;

    m_rotation = (m_rotation + 270) % 360;
    // m_pix_item->setRotation(m_rotation);
    m_pix_item->setTransformOriginPoint(m_pix_item->boundingRect().center());
    m_pix_item->setRotation(m_rotation);    // Set rotation on the item
    m_gview->centerOn(m_pix_item);
}

void dodo::gotoPage(const int &pageno) noexcept
{
    if (!m_document)
    {
        qWarning("Trying to go to page but no document loaded");
        return;
    }

    if (pageno < 0 || pageno >= m_total_pages) {
        qWarning("Page number %d out of range", pageno);
        return;
    }

    if (pageno == m_pageno) {
        return; // No-op
    }

    // boundary condition
    if (!m_suppressHistory && m_pageno >= 0)
    {
        m_page_history_list.push_back(m_pageno);
        if (m_page_history_list.size() > m_page_history_limit)
            m_page_history_list.removeFirst();
    }

    gotoPageInternal(pageno);
}

void dodo::gotoPageInternal(const int &pageno) noexcept
{

    if (pageno < 0 || pageno >= m_total_pages)
    {
        qWarning("Page number %d out of range", pageno);
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

    renderLinks();

    if (m_prefetch_enabled)
    {
        prefetchAround(m_pageno);
    }
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
    for (int offset = -m_prefetch_distance; offset <= m_prefetch_distance; ++offset) {
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
    gotoPage(m_pageno + 1);
}

void dodo::PrevPage() noexcept
{
    gotoPage(m_pageno - 1);
}


void dodo::ZoomIn() noexcept
{
    if (m_scale_factor < 5.0)
    {
        m_gview->scale(1.1, 1.1);
        m_scale_factor *= 1.1;
    }
}

void dodo::ZoomReset() noexcept
{
    m_scale_factor = 1.0;
    m_gview->resetTransform();
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

void dodo::FitToHeight() noexcept
{
    if (!m_pix_item || m_pix_item->pixmap().isNull())
        return;

    int pixmapHeight = m_pix_item->pixmap().height();
    int viewHeight = m_gview->viewport()->height();

    qreal scale = static_cast<qreal>(viewHeight) / pixmapHeight;
    m_gview->resetTransform();
    m_gview->scale(scale, scale);
    m_scale_factor = scale;
}

void dodo::FitToWidth() noexcept
{
    if (!m_pix_item || m_pix_item->pixmap().isNull())
        return;

    int pixmapWidth = m_pix_item->pixmap().width();
    int viewWidth = m_gview->viewport()->width();

    qreal scale = static_cast<qreal>(viewWidth) / pixmapWidth;
    m_gview->resetTransform();
    m_gview->scale(scale, scale);
    m_scale_factor = scale;
}

QRectF dodo::mapPdfRectToScene(const QRectF& pdfRect,
                               const QSizeF& pageSizePoints,
                               float dpi) noexcept
{
    const float scale = dpi / 72.0f;

    // Flip Y-axis and scale to pixels
    qreal x = (pdfRect.left() * pageSizePoints.width()) * scale;
    qreal y = (pageSizePoints.height() * (pdfRect.top() + pdfRect.height())) * scale;
    qreal width = pdfRect.width() * pageSizePoints.width() * scale;
    qreal height = -pdfRect.height() * pageSizePoints.height() * scale;

    return QRectF(x, y, width, height);
}

void dodo::renderLinks() noexcept
{
    float scale = m_dpix / 72.0f;  // 72 points per inch standard
    auto page = m_document->page(m_pageno);
    auto links = page->links();
    QSizeF pageSizePoints = page->pageSizeF();

    for (const auto &link : links)
    {
        QRectF rect = link->linkArea();  // In page coordinates
        if (rect.isNull())
            continue;


        QRectF linkRectScene = mapPdfRectToScene(rect, pageSizePoints, m_dpix);

        linkRectScene = QRectF(linkRectScene.topLeft(), linkRectScene.bottomRight());

        switch(link->linkType())
         {

            case Poppler::Link::Goto:
                {
                    auto gotoLink = static_cast<Poppler::LinkGoto*>(link.get());
                    GotoLinkItem *linkItem = new GotoLinkItem(linkRectScene,
                                                                  gotoLink->destination());
                    connect(linkItem, &GotoLinkItem::jumpToPageRequested, this, [&](int pageno) {
                        gotoPage(pageno);
                    });
                    if (m_link_boundary_box_enabled)
                        linkItem->setPen(QPen(Qt::red, 1));
                    m_gscene->addItem(linkItem);
                }
                break;

            case Poppler::Link::Browse:
                {
                    auto browseLink = static_cast<Poppler::LinkBrowse*>(link.get());
                    BrowseLinkItem *linkItem = new BrowseLinkItem(linkRectScene,
                                                                  browseLink->url());
                    if (m_link_boundary_box_enabled)
                        linkItem->setPen(QPen(Qt::red, 1));
                    m_gscene->addItem(linkItem);
                }
                break;

            default:
                qDebug() << "Link not yet implemented";
        }


    }

}

// Single page search
void dodo::search(const QString &term) noexcept
{
    m_last_search_term = term;

    auto page = m_document->page(m_pageno);
    auto results = page->search(term, m_search_flags);
    QSizeF pageSizePoints = page->pageSizeF();
    float scale = m_dpix / 72.0;

    for (const auto &result : results)
    {

        // Flip Y axis and scale
        QRectF rect(
            result.left() * scale,
            result.top() * scale,
            result.width() * scale,
            result.height() * scale
        );

        auto* highlight = new QGraphicsRectItem(rect);
        highlight->setBrush(QColor(255, 255, 0, 100)); // semi-transparent yellow
        highlight->setPen(Qt::NoPen);

        m_gscene->addItem(highlight);
    }
}

void dodo::searchAll(const QString &term) noexcept
{
    m_last_search_term = term;
    m_searchRectMap.clear();
    m_search_index = -1;
    m_search_match_count = 0;

    clearHighlights();

    QFuture<void> future = QtConcurrent::run([=]() {
        QMap<int, QList<QRectF>> resultsMap;

        for (int pageNum = 0; pageNum < m_total_pages; ++pageNum) {
            auto page = m_document->page(pageNum);
            auto results = page->search(term, m_search_flags);

            if (!results.isEmpty())
            {
                resultsMap[pageNum] = results;
                m_search_match_count += results.length();
                m_panel->setSearchCount(m_search_match_count);
            }
        }

        QMetaObject::invokeMethod(this, [=]() {
            m_searchRectMap = resultsMap;
            if (!resultsMap.isEmpty()) {
                m_search_index = m_searchRectMap.firstKey();
                jumpToHit(m_search_index, 0);
            }
        }, Qt::QueuedConnection);


    });

    m_searchWatcher.setFuture(future);
}


void dodo::jumpToHit(int page, int index)
{
    if (!m_searchRectMap.contains(page) ||
        index < 0 ||
        index >= m_searchRectMap[page].size())
        return;

    m_search_index = index;
    m_search_hit_page = page;

    gotoPage(page);  // Render page
    m_panel->setSearchIndex(index + 1);

    highlightSingleHit(page, m_searchRectMap[page][index]);
}

void dodo::highlightSingleHit(int page, const QRectF &rect)
{
    if (page != m_pageno) return;

    clearHighlights();

    float scale = m_dpix / 72.0;
    QRectF scaledRect = QRectF(
        rect.left() * scale,
        rect.top() * scale,
        rect.width() * scale,
        rect.height() * scale
    );

    auto *highlight = new QGraphicsRectItem(scaledRect);
    highlight->setBrush(QColor(255, 255, 0, 100));
    highlight->setPen(Qt::NoPen);
    highlight->setData(0, "searchHighlight");

    m_gscene->addItem(highlight);
    m_gview->centerOn(highlight);
}

void dodo::clearHighlights()
{
    for (auto item : m_gscene->items()) {
        if (auto rect = qgraphicsitem_cast<QGraphicsRectItem *>(item)) {
            if (rect->data(0).toString() == "searchHighlight") {
                m_gscene->removeItem(rect);
                delete rect;
            }
        }
    }
}


void dodo::Search() noexcept
{
    auto term = QInputDialog::getText(this, "Search", "Search for");
    m_panel->setSearchMode(true);
    searchAll(term);
}

void dodo::nextHit()
{
    if (m_search_hit_page == -1)
        return;

    QList<int> pages = m_searchRectMap.keys();
    int currentPageIdx = pages.indexOf(m_search_hit_page);

    // Try next hit on current page
    if (m_search_index + 1 < m_searchRectMap[m_search_hit_page].size()) {
        jumpToHit(m_search_hit_page, m_search_index + 1);
        return;
    }

    // Try next page
    if (currentPageIdx + 1 < pages.size()) {
        int nextPage = pages[currentPageIdx + 1];
        jumpToHit(nextPage, 0);
    }
}

void dodo::prevHit()
{
    if (m_search_hit_page == -1) return;

    QList<int> pages = m_searchRectMap.keys();
    int currentPageIdx = pages.indexOf(m_search_hit_page);

    // Try previous hit on current page
    if (m_search_index - 1 >= 0) {
        jumpToHit(m_search_hit_page, m_search_index - 1);
        return;
    }

    // Try previous page
    if (currentPageIdx - 1 >= 0) {
        int prevPage = pages[currentPageIdx - 1];
        int lastHitIndex = m_searchRectMap[prevPage].size() - 1;
        jumpToHit(prevPage, lastHitIndex);
    }
}

void dodo::setupKeybinding(const std::string_view &action,
                           const std::string &key) noexcept
{
    // TODO
}

void dodo::Escape() noexcept
{
    // TODO
}

void dodo::GoBackHistory() noexcept
{
    if (!m_page_history_list.isEmpty()) {
        int lastPage = m_page_history_list.takeLast(); // Pop last page
        qDebug() << lastPage << m_pageno;
        m_suppressHistory = true;
        gotoPage(lastPage);
        m_suppressHistory = false;
    } else {
        qInfo("No page history available");
    }
}
