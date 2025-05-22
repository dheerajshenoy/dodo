#include "dodo.hpp"
#include "GraphicsView.hpp"
#include <qpointingdevice.h>

dodo::dodo() noexcept
{
    initGui();
    this->show();
    initConfig();
    initDB();
    DPI_FRAC = m_dpi / m_low_dpi;
    initKeybinds();
    QThreadPool::globalInstance()->setMaxThreadCount(QThread::idealThreadCount());
    openFile("~/Downloads/protected.pdf"); // FOR DEBUG PURPOSE ONLY
    m_HQRenderTimer->setSingleShot(true);
    m_page_history_list.reserve(m_page_history_limit);
    initConnections();
    m_pix_item->setScale(m_scale_factor);
}

dodo::~dodo() noexcept
{}

void dodo::initConnections() noexcept
{
    connect(m_HQRenderTimer, &QTimer::timeout, this, [&]() {
        renderPage(m_pageno, false);
    });

    connect(m_model, &Model::searchResultsReady, this, [&](const QMap<int, QList<QRectF>> &maps, int matchCount) {
        m_searchRectMap = maps;
        m_search_match_count = matchCount;
        m_panel->setSearchCount(m_search_match_count);
        if (maps.isEmpty())
            return;
        auto page = maps.firstKey();
        highlightHitsInPage(page);
        jumpToHit(page, 0);
    });

    connect(m_gview, &GraphicsView::highlightDrawn, this, [=](const QRectF &pdfRect) {
        m_model->addHighlightAnnotation(m_pageno, pdfRect);
    });

    connect(m_model, &Model::horizontalFitRequested, this, [&]() {
        FitToWidth();
    });

    connect(m_model, &Model::verticalFitRequested, this, [&]() {
        FitToHeight();
    });

    connect(m_model, &Model::jumpToPageRequested, this, [&](int pageno) {
        gotoPage(pageno);
    });

    connect(m_model, &Model::jumpToLocationRequested, this, [&](int pageno, const BrowseLinkItem::Location &loc) {
        gotoPage(pageno);
        scrollToXY(loc.x, loc.y);
    });

}

void dodo::scrollToXY(float x, float y) noexcept
{
    if (!m_pix_item || !m_gview)
        return;

    m_gview->centerOn(QPointF(x * DPI_FRAC, y * DPI_FRAC));
}

void dodo::initDB() noexcept
{
    if (!m_remember_last_visited)
        return;

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(m_config_dir.filePath("last_pages.db"));
    db.open();

    QSqlQuery query;

    // FIXME: Maybe add some unique hashing so that this works even when you move a file
    query.exec("CREATE TABLE IF NOT EXISTS last_visited ("
           "file_path TEXT PRIMARY KEY, "
           "page_number INTEGER, "
           "last_accessed TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");
}

void dodo::initConfig() noexcept
{
    m_config_dir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    auto config_file_path = m_config_dir.filePath("config.toml");

    auto toml = toml::parse_file(config_file_path.toStdString());

    auto ui = toml["ui"];
    m_panel->setVisible(ui["panel_shown"].value_or(true));

    if (ui["fullscreen"].value_or(false))
        this->showFullScreen();

    m_gview->verticalScrollBar()->setVisible(ui["vscrollbar"].value_or(true));
    m_gview->horizontalScrollBar()->setVisible(ui["hscrollbar"].value_or(true));

    m_scale_factor = ui["zoom_level"].value_or(1.0);
    bool compact = ui["compact"].value_or(false);
    m_model->setLinkBoundaryBox(ui["link_boundary_box"].value_or(false));

    auto colors = toml["colors"];
    auto search_index = colors["search_index"].value_or("#3daee944");
    auto search_match = colors["search_match"].value_or("#FFFF8844");
    auto accent = colors["accent"].value_or("#FF500044");
    auto background = colors["background"].value_or("#FFFFFF");

    m_colors["search_index"] = search_index;
    m_colors["search_match"] = search_match;
    m_colors["accent"] = accent;
    m_colors["background"] = background;

    m_gview->setBackgroundBrush(QColor(m_colors["background"]));

    auto rendering = toml["rendering"];
    auto backend = rendering["backend"].value_or("mupdf");
    m_dpi = rendering["dpi"].value_or(300.0);
    m_low_dpi = rendering["low_dpi"].value_or(72.0);
    m_model->setDPI(m_dpi);
    m_model->setLowDPI(m_low_dpi);
    int cache_pages = rendering["cache_pages"].value_or(50);

    auto behavior = toml["behavior"];
    m_remember_last_visited = behavior["remember_last_visited"].value_or(true);
    m_prefetch_enabled = behavior["enable_prefetch"].value_or(true);
    m_prefetch_distance = behavior["prefetch_distance"].value_or(2);
    m_page_history_limit = behavior["page_history"].value_or(100);

    auto keys = toml["keybindings"];

    if (keys.is_value())
    {
        for (auto &[action, value] : *keys.as_table())
        {
            if (value.is_value())
                setupKeybinding(action.str(), value.value_or<std::string>(""));
        }
    }

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

        { "b", [this]() { GotoPage(); } },
        { "f", [this]() { VisitLinkKB(); } },
        { "Ctrl+s", [this]() { SaveFile(); } },
        { "Alt+1", [this]() { ToggleHighlight(); } },
        { "t", [this]() { TableOfContents(); } },
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
    m_gview->setPixmapItem(m_pix_item);


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

    m_model = new Model(m_gscene);
    connect(m_model, &Model::imageRenderRequested, this, &dodo::handleRenderResult);
    updateUiEnabledState();
}

void dodo::handleRenderResult(int pageno, QImage image, bool lowQuality)
{
    if (pageno != m_pageno || image.isNull())
    {
        qDebug() << "Old, ignoring";
        return;
    }

    QPixmap pix = QPixmap::fromImage(image);

    // if (!lowQuality) {
    //     m_highResCache.insert(pageno, new QPixmap(pix));
    //     // m_pix_item->setScale(m_scale_factor);
    // } else {
    //     m_pixmapCache.insert(pageno, new QPixmap(pix));
    //     // m_pix_item->setScale(DPI_FRAC);
    // }

    m_pix_item->setPixmap(pix);

    // Normalize scale: use logical size instead of raw pixels

    // if (m_prefetch_enabled)
    // {
    //     prefetchAround(m_pageno);
    // }
    m_panel->setPageNo(m_pageno + 1);
    renderLinks();
}

void dodo::updateUiEnabledState() noexcept
{
    const bool hasFile = m_model->valid();

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
    }

}

/* Function for opening the file using the model.
 For internal usage only */
void dodo::openFile(const QString &fileName) noexcept
{
    m_filename = fileName;
    m_filename.replace("~", QString::fromStdString(getenv("HOME")));

    if (!QFile::exists(m_filename))
    {
        qWarning("file does not exist!: ");
        return;
    }

    if (!m_model->openFile(m_filename))
        return;

    if (m_model->passwordRequired())
        if(!askForPassword())
            return;

    m_total_pages = m_model->numPages();
    m_panel->setTotalPageCount(m_total_pages);
    m_panel->setFileName(m_filename);

    if (m_remember_last_visited)
    {
        QSqlQuery q;
        q.prepare("SELECT page_number FROM last_visited WHERE file_path = ?");
        q.addBindValue(m_filename);
        q.exec();

        if (q.next()) {
            int page = q.value(0).toInt();
            gotoPage(page);
        } else {
            gotoPage(0);
        }
    } else
        gotoPage(0);

    updateUiEnabledState();
}

bool dodo::askForPassword() noexcept
{
    bool ok;
    auto pwd = QInputDialog::getText(this, "Document is locked", "Enter password",
                                     QLineEdit::EchoMode::Password, QString(), &ok);
    if (!ok)
        return false;

    auto correct = m_model->authenticate(pwd);
    if (correct)
        return true;
    else
    {
        QMessageBox::critical(this, "Password", "Password is incorrect");
        return false;
    }
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
    if (!m_model->valid())
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

    // TODO: Handle file content change detection

    // if (m_highResCache.contains(pageno))
    // {
    //     QPixmap *cached = m_highResCache.object(pageno);
    //     m_pix_item->setPixmap(*cached);
    //     // m_pix_item->setScale(m_scale_factor);
    //     return;
    // }
    //
    m_HQRenderTimer->stop();
    m_HQRenderTimer->start(100);

    if (m_highlights_present)
    {
        clearHighlights();
        clearIndexHighlights();
    }

    renderPage(pageno, true);
}

void dodo::renderPage(int pageno,
                      bool lowQuality) noexcept
{

    // if (!lowQuality)
    // {
    //     if (m_highResCache.contains(pageno)) {
    //         m_pix_item->setPixmap(*m_highResCache.object(pageno));
    //         // m_pix_item->setScale(m_scale_factor);
    //         return;
    //     }
    // } else {
    //     if (m_pixmapCache.contains(pageno)) {
    //         m_pix_item->setPixmap(*m_pixmapCache.object(pageno));
    //         // m_pix_item->setScale(DPI_FRAC);
    //         return;
    //     }
    // }

    m_model->renderPage(pageno, lowQuality);
}


bool dodo::isPrefetchPage(int page, int currentPage) noexcept
{
    // Prefetch next and previous page only
    return page == currentPage + 1 || page == currentPage - 1;
}

void dodo::cachePage(int pageno) noexcept
{
    if (m_highResCache.contains(pageno) || m_pixmapCache.contains(pageno))
    {
        return;
    }

    m_model->renderPage(pageno, false);
    // auto *pix = new QPixmap(QPixmap::fromImage(img));
    // m_highResCache.insert(pageno, pix);
}

void dodo::prefetchAround(int currentPage) noexcept
{
    for (int offset = -m_prefetch_distance; offset <= m_prefetch_distance; ++offset) {
        int page = currentPage + offset;
        if (page >= 0 && page < m_total_pages) {
            cachePage(page);
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
    if (m_scale_factor * 0.9 != 0)
    {
        m_gview->scale(0.9, 0.9);
        m_scale_factor *= 0.9;
    }
}

void dodo::Zoom(float factor) noexcept
{
    // TODO: Add constraints here
    m_gview->scale(factor, factor);
    m_scale_factor *= factor;
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

void dodo::renderLinks() noexcept
{
    m_model->renderLinks(m_pageno, m_model->transform());
}

// Single page search
void dodo::search(const QString &term) noexcept
{
    m_last_search_term = term;
    // m_pdf_backend->search();
}

void dodo::searchAll(const QString &term) noexcept
{

    m_searchRectMap.clear();
    m_search_index = -1;
    m_search_match_count = 0;
    clearIndexHighlights();
    clearHighlights();

    if (term.isEmpty() || term.isNull())
        return;

    m_last_search_term = term;

    m_model->searchAll(term);
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

void dodo::highlightHitsInPage(int pageno)
{
    if (pageno != m_pageno) return;

    clearIndexHighlights();

    float scale = m_dpi / 72.0;
    auto rects = m_searchRectMap.value(pageno);

    for (const auto &rect : rects)
    {
        QRectF scaledRect = QRectF(
            rect.left() * scale,
            rect.top() * scale,
            rect.width() * scale,
            rect.height() * scale
        );

        auto *highlight = new QGraphicsRectItem(scaledRect);

        highlight->setBrush(QColor::fromString(m_colors["search_match"]));
        highlight->setPen(Qt::NoPen);
        highlight->setData(0, "searchHighlight");

        m_gscene->addItem(highlight);
        // m_gview->centerOn(highlight);
    }
    m_highlights_present = true;
}

void dodo::highlightSingleHit(int page, const QRectF &rect)
{
    if (page != m_pageno) return;

    clearIndexHighlights();

    float scale = m_dpi / 72.0;
    QRectF scaledRect = QRectF(
        rect.left() * scale,
        rect.top() * scale,
        rect.width() * scale,
        rect.height() * scale
    );

    auto *highlight = new QGraphicsRectItem(scaledRect);
    highlight->setBrush(QColor::fromString(m_colors["search_index"]));
    highlight->setPen(Qt::NoPen);
    highlight->setData(0, "searchIndexHighlight");

    m_gscene->addItem(highlight);
    m_gview->centerOn(highlight);
    m_highlights_present = true;
}

void dodo::clearIndexHighlights()
{
    for (auto item : m_gscene->items()) {
        if (auto rect = qgraphicsitem_cast<QGraphicsRectItem *>(item)) {
            if (rect->data(0).toString() == "searchIndexHighlight") {
                m_gscene->removeItem(rect);
                delete rect;
            }
        }
    }
    m_highlights_present = false;
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
    m_highlights_present = false;
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
        m_suppressHistory = true;
        gotoPage(lastPage);
        m_suppressHistory = false;
    } else {
        qInfo("No page history available");
    }
}

void dodo::closeEvent(QCloseEvent *e)
{

    if (m_remember_last_visited && m_model->valid())
    {
        QSqlQuery q;
        q.prepare("INSERT OR REPLACE INTO last_visited(file_path, page_number) VALUES (?, ?)");
        q.addBindValue(m_filename);
        q.addBindValue(m_pageno);
        q.exec();
    }

    e->accept();
}

void dodo::scrollToNormalizedTop(const double &ntop) noexcept
{
    // 1. Get scene Y position of the top of the page
    qreal pageSceneY = m_pix_item->sceneBoundingRect().top();

    // 2. Get full height of the page
    qreal pageHeight = m_pix_item->boundingRect().height();

    // 3. Compute desired Y position in scene coords
    qreal targetSceneY = pageSceneY + ntop * pageHeight;

    // 4. Convert scene Y to viewport Y
    QPointF targetViewportPoint = m_gview->mapFromScene(QPointF(0, targetSceneY));

    // 5. Compute the scroll position required
    int scrollPos = m_gview->verticalScrollBar()->value() + targetViewportPoint.y();

    m_gview->verticalScrollBar()->setValue(scrollPos);

}

void dodo::TableOfContents() noexcept
{
    if (!m_owidget)
    {
        m_owidget = m_model->tableOfContents();
        m_owidget->setParent(this);
        m_owidget->setWindowFlag(Qt::Window); // Makes it a standalone window
        connect(m_owidget, &OutlineWidget::jumpToPageRequested, this, [=](int pageno) {
            gotoPage(pageno);
        });
        m_owidget->open();

    } else {
        if (m_owidget->isVisible())
            m_owidget->close();
        else
            m_owidget->open();
    }
}

void dodo::ToggleHighlight() noexcept
{
    m_gview->setDrawingMode(!m_gview->drawingMode());
}

void dodo::SaveFile() noexcept
{
    m_model->save();
    m_model->renderPage(m_pageno, false);
}

void dodo::VisitLinkKB() noexcept
{
    this->installEventFilter(this);
    m_model->visitLinkKB(m_pageno);
    m_linkHintMode = true;
    m_link_hint_map = m_model->hintToLinkMap();
}

void dodo::CopyLinkKB() noexcept
{
    m_model->copyLinkKB(m_pageno);
}

// void dodo::keyPressEvent(QKeyEvent *e)
// {
//     if (e->key() == Qt::Key_F) {
//         e->accept();
//
//         // ✅ Defer activation until after the current event is done
//         QTimer::singleShot(0, this, [this]() {
//             m_linkHintMode = true;
//             qDebug() << "Link hint mode activated";
//         });
//
//         return;
//     }
//
//     QMainWindow::keyPressEvent(e);
// }

// void dodo::keyReleaseEvent(QKeyEvent *e)
// {
//     if (m_linkHintMode)
//     {
//         // e->ignore();
//         e->ignore();
//         qDebug() << "DD";
//
//         // m_linkHintMode = false;
//         return;
//     }
//     QMainWindow::keyPressEvent(e);
// }

bool dodo::eventFilter(QObject *obj, QEvent *event)
{
    if (m_linkHintMode)
    {
        if (m_link_hint_map.isEmpty())
        {
            m_linkHintMode = false;
            m_currentHintInput.clear();
            qWarning() << "No links found to show hints";
            // m_model->clearKBHintsOverlay();
            return true;
        }
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape)
            {
                m_linkHintMode = false;
                m_currentHintInput.clear();
                m_model->clearKBHintsOverlay();
                this->removeEventFilter(this);
                return true;
            } else if (keyEvent->key() == Qt::Key_Backspace)
            {
                m_currentHintInput.removeLast();
                return true;
            }

            m_currentHintInput += keyEvent->text();
            if (m_link_hint_map.contains(m_currentHintInput))
            {
                Model::LinkInfo info = m_link_hint_map[m_currentHintInput];
                m_model->followLink(info);
                m_linkHintMode = false;
                m_currentHintInput.clear();
                m_link_hint_map.clear();
                m_model->clearKBHintsOverlay();
                this->removeEventFilter(this);
                return true;
            }
            keyEvent->accept();
            // Accept and swallow the key event to prevent QShortcut activation
            // keyEvent->timestamp();
            // qDebug() << "Blocked key in link hint mode:" << keyEvent->text();
            return true;
        }

        if (event->type() == QEvent::ShortcutOverride)
        {
            QShortcutEvent *stEvent = static_cast<QShortcutEvent *>(event);
            stEvent->accept();
            return true;
        }
    }

    // Let other events pass through
    return QObject::eventFilter(obj, event);
}

void dodo::GotoPage() noexcept
{
    int pageno = QInputDialog::getInt(this, "Goto Page",
                                      QString("Enter page number ({} to {})").arg(0, m_total_pages),
                                      0, 1, m_total_pages);
    gotoPage(pageno - 1);
}
