#include "dodo.hpp"

#include "BrowseLinkItem.hpp"
#include "EditLastPagesWidget.hpp"
#include "GraphicsView.hpp"
#include "PropertiesWidget.hpp"

#include <algorithm>
#include <iterator>
#include <qcolordialog.h>
#include <qcontainerinfo.h>
#include <qgraphicsitem.h>
#include <qthread.h>

inline uint
qHash(const dodo::CacheKey &key, uint seed = 0)
{
    seed ^= uint(key.page) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= uint(key.rotation) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= uint(key.scale) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

dodo::dodo() noexcept
{
    setAttribute(Qt::WA_NativeWindow); // This is necessary for DPI updates
}

dodo::~dodo() noexcept
{
    m_last_pages_db.close();
    synctex_scanner_free(m_synctex_scanner);
}

void
dodo::construct() noexcept
{
    initGui();
    initConfig();
    if (m_load_default_keybinding)
        initKeybinds();
    initMenubar();
    initDB();

    m_page_history_list.reserve(m_page_history_limit);
    initConnections();
    m_gview->setPageNavWithMouse(m_page_nav_with_mouse);
    m_pix_item->setScale(m_scale_factor);
    populateRecentFiles();
    m_jump_marker = new JumpMarker(m_colors["jump_marker"]);
    m_gscene->addItem(m_jump_marker);

    setMinimumSize(600, 400);
    this->show();
}

void
dodo::initMenubar() noexcept
{

    // --- File Menu ---
    QMenu *fileMenu = m_menuBar->addMenu("&File");
    fileMenu->addAction(QString("Open File\t%1").arg(m_shortcuts_map["open_file"]), this, &dodo::OpenFile);
    m_actionFileProperties = fileMenu->addAction(QString("File Properties\t%1").arg(m_shortcuts_map["file_properties"]),
                                                 this, &dodo::FileProperties);

    m_actionSaveFile =
        fileMenu->addAction(QString("Save File\t%1").arg(m_shortcuts_map["save"]), this, &dodo::SaveFile);

    m_actionSaveAsFile =
        fileMenu->addAction(QString("Save As File\t%1").arg(m_shortcuts_map["save_as"]), this, &dodo::SaveAsFile);

    m_actionCloseFile =
        fileMenu->addAction(QString("Close File\t%1").arg(m_shortcuts_map["close_file"]), this, &dodo::CloseFile);

    m_recentFilesMenu = fileMenu->addMenu("Recent Files");
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, &QMainWindow::close);

    QMenu *editMenu = m_menuBar->addMenu("&Edit");
    editMenu->addAction(QString("Last Pages\t%1").arg(m_shortcuts_map["edit_last_pages"]), this, &dodo::editLastPages);

    // --- View Menu ---
    QMenu *viewMenu    = m_menuBar->addMenu("&View");
    m_actionFullscreen = viewMenu->addAction(QString("Fullscreen\t%1").arg(m_shortcuts_map["fullscreen"]), this,
                                             &dodo::ToggleFullscreen);
    m_actionFullscreen->setCheckable(true);

    m_actionZoomIn = viewMenu->addAction(QString("Zoom In\t%1").arg(m_shortcuts_map["zoom_in"]), this, &dodo::ZoomIn);
    m_actionZoomOut =
        viewMenu->addAction(QString("Zoom Out\t%1").arg(m_shortcuts_map["zoom_out"]), this, &dodo::ZoomOut);

    viewMenu->addSeparator();

    m_fitMenu = viewMenu->addMenu("Fit");

    m_actionFitNone = m_fitMenu->addAction(QString("None\t%1").arg(m_shortcuts_map["fit_none"]), this, &dodo::FitNone);

    m_actionFitWidth =
        m_fitMenu->addAction(QString("Width\t%1").arg(m_shortcuts_map["fit_width"]), this, &dodo::FitWidth);

    m_actionFitHeight =
        m_fitMenu->addAction(QString("Height\t%1").arg(m_shortcuts_map["fit_height"]), this, &dodo::FitHeight);

    m_actionFitWindow =
        m_fitMenu->addAction(QString("Window\t%1").arg(m_shortcuts_map["fit_window"]), this, &dodo::FitWindow);

    m_fitMenu->addSeparator();

    // Auto Resize toggle (independent)
    m_actionAutoresize = m_fitMenu->addAction(QString("Auto Resize\t%1").arg(m_shortcuts_map["auto_resize"]), this,
                                              &dodo::ToggleAutoResize);
    m_actionAutoresize->setCheckable(true);
    m_actionAutoresize->setChecked(m_auto_resize); // default on or off

    viewMenu->addSeparator();

    m_actionToggleOutline =
        viewMenu->addAction(QString("Outline\t%1").arg(m_shortcuts_map["outline"]), this, &dodo::TableOfContents);

    m_actionToggleMenubar =
        viewMenu->addAction(QString("Menubar\t%1").arg(m_shortcuts_map["toggle_menubar"]), this, &dodo::ToggleMenubar);
    m_actionToggleMenubar->setCheckable(true);
    m_actionToggleMenubar->setChecked(!m_menuBar->isHidden());

    m_actionTogglePanel =
        viewMenu->addAction(QString("Panel\t%1").arg(m_shortcuts_map["toggle_panel"]), this, &dodo::TogglePanel);
    m_actionTogglePanel->setCheckable(true);
    m_actionTogglePanel->setChecked(!m_panel->isHidden());

    m_actionInvertColor =
        viewMenu->addAction(QString("Invert Color\t%1").arg(m_shortcuts_map["invert_color"]), this, &dodo::InvertColor);
    m_actionInvertColor->setCheckable(true);
    m_actionInvertColor->setChecked(m_model->invertColor());

    QMenu *toolsMenu = m_menuBar->addMenu("Tools");

    QActionGroup *selectionActionGroup = new QActionGroup(this);
    selectionActionGroup->setExclusive(true);

    m_actionTextSelect = toolsMenu->addAction(QString("Text Selection"), this, &dodo::TextSelectionMode);
    m_actionTextSelect->setCheckable(true);
    m_actionTextSelect->setChecked(true);
    selectionActionGroup->addAction(m_actionTextSelect);

    m_actionTextHighlight = toolsMenu->addAction(QString("Text Highlight\t%1").arg(m_shortcuts_map["text_highlight"]),
                                                 this, &dodo::ToggleTextHighlight);
    m_actionTextHighlight->setCheckable(true);
    selectionActionGroup->addAction(m_actionTextHighlight);

    m_actionAnnotRect = toolsMenu->addAction(QString("Annotate Rectangle\t%1").arg(m_shortcuts_map["annot_rect"]), this,
                                             &dodo::ToggleRectAnnotation);
    m_actionAnnotRect->setCheckable(true);
    selectionActionGroup->addAction(m_actionAnnotRect);

    m_actionAnnotEdit = toolsMenu->addAction(QString("Edit Annotations\t%1").arg(m_shortcuts_map["annot_edit"]), this,
                                             &dodo::ToggleAnnotSelect);
    m_actionAnnotEdit->setCheckable(true);
    selectionActionGroup->addAction(m_actionAnnotEdit);

    // --- Navigation Menu ---
    QMenu *navMenu = m_menuBar->addMenu("&Navigation");
    m_actionFirstPage =
        navMenu->addAction(QString("First Page\t%1").arg(m_shortcuts_map["first_page"]), this, &dodo::FirstPage);

    m_actionPrevPage =
        navMenu->addAction(QString("Previous Page\t%1").arg(m_shortcuts_map["prev_page"]), this, &dodo::PrevPage);

    m_actionNextPage =
        navMenu->addAction(QString("Next Page\t%1").arg(m_shortcuts_map["next_page"]), this, &dodo::NextPage);
    m_actionLastPage =
        navMenu->addAction(QString("Last Page\t%1").arg(m_shortcuts_map["last_page"]), this, &dodo::LastPage);

    m_actionPrevLocation = navMenu->addAction(QString("Previous Location\t%1").arg(m_shortcuts_map["prev_location"]),
                                              this, &dodo::GoBackHistory);

    QMenu *helpMenu = m_menuBar->addMenu("&Help");
    m_actionAbout   = helpMenu->addAction(QString("About\t%1").arg(m_shortcuts_map["about"]), this, &dodo::ShowAbout);
    helpMenu->addAction(QString("Keybindings\t%1").arg(m_shortcuts_map["keybindings"]), this, &dodo::ShowKeybindings);

    updateUiEnabledState();
}

void
dodo::initConnections() noexcept
{

    connect(m_gview, &GraphicsView::scrollRequested, this, &dodo::mouseWheelScrollRequested);
    connect(m_gview, &GraphicsView::populateContextMenuRequested, this, &dodo::populateContextMenu);

    connect(m_model, &Model::documentSaved, this, [&]() { setDirty(false); });

    connect(m_model, &Model::searchResultsReady, this,
            [&](const QMap<int, QList<Model::SearchResult>> &maps, int matchCount)
    {
        if (matchCount == 0)
        {
            QMessageBox::information(this, "Search Result", QString("No match found for %1").arg(m_last_search_term));
            return;
        }
        m_searchRectMap = maps;
        m_panel->setSearchCount(matchCount);
        auto page = maps.firstKey();
        jumpToHit(page, 0);
    });

    connect(m_gview, &GraphicsView::highlightDrawn, m_model, [&](const QRectF &rect)
    {
        m_model->addRectAnnotation(rect);
        setDirty(true);
        renderPage(m_pageno);
    });

    connect(m_model, &Model::horizontalFitRequested, this, [&](int pageno, const BrowseLinkItem::Location &location)
    {
        gotoPage(pageno);
        FitWidth();
        scrollToNormalizedTop(location.y);
    });

    connect(m_model, &Model::verticalFitRequested, this, [&](int pageno, const BrowseLinkItem::Location &location)
    {
        gotoPage(pageno);
        FitHeight();
        scrollToNormalizedTop(location.y);
    });

    connect(m_model, &Model::jumpToPageRequested, this, [&](int pageno)
    {
        gotoPage(pageno);
        TopOfThePage();
    });

    connect(m_model, &Model::jumpToLocationRequested, this, [&](int pageno, const BrowseLinkItem::Location &loc)
    {
        gotoPage(pageno);
        scrollToXY(loc.x, loc.y);
    });

    connect(m_gview, &GraphicsView::synctexJumpRequested, this, [&](QPointF loc)
    {
        if (m_synctex_scanner)
        {
            fz_point pt = mapToPdf(loc);
            if (synctex_edit_query(m_synctex_scanner, m_pageno + 1, pt.x, pt.y) > 0)
            {
                synctex_node_p node;
                while ((node = synctex_scanner_next_result(m_synctex_scanner)))
                    synctexLocateInFile(synctex_node_get_name(node), synctex_node_line(node));
            }
            else
            {
                qDebug() << "No matching source found!";
                QMessageBox::warning(this, "SyncTeX Error", "No matching source found!");
            }
        }
        else
        {
            QMessageBox::warning(this, "SyncTex", "Not a valid synctex document");
        }
    });

    connect(m_gview, &GraphicsView::textSelectionRequested, m_model, [&](const QPointF &start, const QPointF &end)
    {
        m_model->highlightTextSelection(start, end);
        m_selection_present = true;
    });

    connect(m_gview, &GraphicsView::textHighlightRequested, m_model, [&](const QPointF &start, const QPointF &end)
    {
        m_model->annotHighlightSelection(start, end);
        setDirty(true);
        renderPage(m_pageno);
    });

    connect(m_gview, &GraphicsView::textSelectionDeletionRequested, this, &dodo::clearTextSelection);

    connect(m_gview, &GraphicsView::annotSelectRequested, m_model, [&](const QRectF &area)
    {
        m_selected_annots = m_model->getAnnotationsInArea(area);
        // TODO: Show number of annots selected ?
        selectAnnots();
    });

    connect(m_gview, &GraphicsView::annotSelectClearRequested, this, &dodo::clearAnnotSelection);
    connect(m_gview, &GraphicsView::zoomInRequested, this, &dodo::ZoomIn);
    connect(m_gview, &GraphicsView::zoomOutRequested, this, &dodo::ZoomOut);
}

void
dodo::selectAnnots() noexcept
{
    for (const auto &item : m_gscene->items())
    {
        if (item->data(0).toString() == "annot" && m_selected_annots.contains(item->data(1).toInt()))
        {
            HighlightItem *gitem = dynamic_cast<HighlightItem *>(item);
            if (!gitem)
                continue;

            gitem->select(Qt::red);
        }
    }
    m_annot_selection_present = true;
}

void
dodo::clearTextSelection() noexcept
{
    if (!m_selection_present)
        return;

    for (auto &object : m_gscene->items())
    {
        if (object->data(0).toString() == "selection")
            m_gscene->removeItem(object);
    }
    m_selection_present = false;
}

void
dodo::scrollToXY(float x, float y) noexcept
{
    if (!m_pix_item || !m_gview)
        return;

    fz_point point = {.x = x * m_inv_dpr, .y = y * m_inv_dpr};
    point          = fz_transform_point(point, m_model->transform());

    if (m_jump_marker_shown)
    {
        m_jump_marker->setRect(QRectF(point.x, point.y + 10, 10, 10));

        m_jump_marker->show();
        QTimer::singleShot(1000, [=]() { fadeJumpMarker(m_jump_marker); });
    }
    m_gview->centerOn(QPointF(point.x, point.y));
}

void
dodo::initDB() noexcept
{
    if (!m_remember_last_visited)
        return;

    m_last_pages_db = QSqlDatabase::addDatabase("QSQLITE");
    m_last_pages_db.setDatabaseName(m_config_dir.filePath("last_pages.db"));
    m_last_pages_db.open();

    QSqlQuery query;

    // FIXME: Maybe add some unique hashing so that this works even when you move a file
    query.exec("CREATE TABLE IF NOT EXISTS last_visited ("
               "file_path TEXT PRIMARY KEY, "
               "page_number INTEGER, "
               "last_accessed TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");
}

void
dodo::initDefaults() noexcept
{
    m_jump_marker_shown       = true;
    m_full_file_path_in_panel = false;
    m_scale_factor            = 1.0f;
    m_model->setLinkBoundary(false);
    m_window_title = "%1 - dodo";

    m_colors["search_index"] = QColor::fromString("#3daee944").rgba();
    m_colors["search_match"] = QColor::fromString("#55FF8844").rgba();
    m_colors["accent"]       = QColor::fromString("#FF500044").rgba();
    m_colors["background"]   = Qt::transparent;
    m_colors["link_hint_fg"] = QColor::fromString("#000000").rgba();
    m_colors["link_hint_bg"] = QColor::fromString("#FFFF00").rgba();
    m_colors["highlight"]    = QColor::fromString("#55FFFF00").rgba();
    m_colors["selection"]    = QColor::fromString("#550000FF").rgba();
    m_colors["jump_marker"]  = QColor::fromString("#FFFF0000").rgba();
    m_colors["annot_rect"]   = QColor::fromString("#55FF0000").rgba();

    m_model->setSelectionColor(m_colors["selection"]);
    m_model->setHighlightColor(m_colors["highlight"]);
    m_model->setAnnotRectColor(m_colors["annot_rect"]);
    m_model->setLinkHintBackground(m_colors["link_hint_bg"]);
    m_model->setLinkHintForeground(m_colors["link_hint_fg"]);
    m_gview->setBackgroundBrush(QColor::fromRgba(m_colors["background"]));

    m_dpi = 300.0f;
    m_model->setDPI(m_dpi);
    int cache_pages = 10;
    m_cache.setMaxCost(cache_pages);

    m_remember_last_visited = true;
    m_page_history_limit    = 10;
    m_model->setAntialiasingBits(8);
    m_auto_resize = false;
    m_zoom_by     = 1.25;
    m_model->enableICC();
    auto initial_fit = "width";

    if (strcmp(initial_fit, "height") == 0)
        m_initial_fit = FitMode::Height;
    else if (strcmp(initial_fit, "width") == 0)
        m_initial_fit = FitMode::Width;
    else if (strcmp(initial_fit, "window") == 0)
        m_initial_fit = FitMode::Window;
    else
        m_initial_fit = FitMode::None;

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_panel->layout()->setContentsMargins(5, 1, 5, 1);
    m_panel->setContentsMargins(0, 0, 0, 0);
    this->setContentsMargins(0, 0, 0, 0);
}

void
dodo::initConfig() noexcept
{
    m_config_dir          = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    auto config_file_path = m_config_dir.filePath("config.toml");

    if (!QFile::exists(config_file_path))
    {
        initDefaults();
        return;
    }

    toml::table toml;

    try
    {
        toml = toml::parse_file(config_file_path.toStdString());
    }
    catch (std::exception &e)
    {
        QMessageBox::critical(this, "Error in configuration file",
                              QString("There are one or more error(s) in your config "
                                      "file:\n%1\n\nLoading default config.")
                                  .arg(e.what()));
        return;
    }

    auto ui = toml["ui"];
    m_panel->setHidden(!ui["panel"].value_or(true));

    m_menuBar->setHidden(!ui["menubar"].value_or(true));

    if (ui["fullscreen"].value_or(false))
        ToggleFullscreen();

    auto vscrollbar_shown = ui["vscrollbar"].value_or(true);

    if (!vscrollbar_shown)
        m_gview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto hscrollbar_shown = ui["hscrollbar"].value_or(true);
    auto hscrollbar       = m_gview->horizontalScrollBar();
    if (!hscrollbar_shown)
        m_gview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_jump_marker_shown       = ui["jump_marker"].value_or(true);
    m_full_file_path_in_panel = ui["full_file_path_in_panel"].value_or(false);
    m_scale_factor            = ui["zoom_level"].value_or(1.0);
    bool compact              = ui["compact"].value_or(false);
    m_model->setLinkBoundary(ui["link_boundary"].value_or(false));

    m_window_title = ui["window_title"].value_or("{} - dodo");
    m_window_title.replace("{}", "%1");

    auto colors = toml["colors"];

    m_colors["search_index"] = QColor::fromString(colors["search_index"].value_or("#3daee944")).rgba();
    m_colors["search_match"] = QColor::fromString(colors["search_match"].value_or("#FFFF8844")).rgba();
    m_colors["accent"]       = QColor::fromString(colors["accent"].value_or("#FF500044")).rgba();
    m_colors["background"]   = QColor::fromString(colors["background"].value_or("#FFFFFF")).rgba();
    m_colors["link_hint_fg"] = QColor::fromString(colors["link_hint_fg"].value_or("#000000")).rgba();
    m_colors["link_hint_bg"] = QColor::fromString(colors["link_hint_bg"].value_or("#FFFF00")).rgba();
    m_colors["highlight"]    = QColor::fromString(colors["highlight"].value_or("#55FFFF00")).rgba();
    m_colors["selection"]    = QColor::fromString(colors["selection"].value_or("#550000FF")).rgba();
    m_colors["jump_marker"]  = QColor::fromString(colors["jump_marker"].value_or("#FFFF0000")).rgba();

    m_model->setSelectionColor(m_colors["selection"]);
    m_model->setHighlightColor(m_colors["highlight"]);
    m_model->setLinkHintBackground(m_colors["link_hint_bg"]);
    m_model->setLinkHintForeground(m_colors["link_hint_fg"]);
    m_gview->setBackgroundBrush(QColor::fromRgba(m_colors["background"]));

    auto rendering = toml["rendering"];
    m_dpi          = rendering["dpi"].value_or(300.0);
    m_model->setDPI(m_dpi);
    int cache_pages = rendering["cache_pages"].value_or(10);
    m_cache.setMaxCost(cache_pages);

    auto behavior           = toml["behavior"];
    m_remember_last_visited = behavior["remember_last_visited"].value_or(true);
    // m_prefetch_enabled = behavior["enable_prefetch"].value_or(true);
    // m_prefetch_distance = behavior["prefetch_distance"].value_or(2);
    m_page_history_limit = behavior["page_history"].value_or(100);
    m_model->setAntialiasingBits(behavior["antialasing_bits"].value_or(8));
    m_auto_resize               = behavior["auto_resize"].value_or(false);
    m_zoom_by                   = behavior["zoom_factor"].value_or(1.25);
    m_page_nav_with_mouse       = behavior["page_nav_with_mouse"].value_or(true);
    auto synctex_editor_command = behavior["synctex_editor_command"].value<std::string>();
    if (synctex_editor_command.has_value())
        m_synctex_editor_command = QString::fromStdString(synctex_editor_command.value());

    if (behavior["invert_mode"].value_or(false))
        m_model->toggleInvertColor();

    if (behavior["icc_color_profile"].value_or(true))
        m_model->enableICC();

    auto initial_fit = behavior["initial_fit"].value_or("width");

    if (strcmp(initial_fit, "height") == 0)
        m_initial_fit = FitMode::Height;
    else if (strcmp(initial_fit, "width") == 0)
        m_initial_fit = FitMode::Width;
    else if (strcmp(initial_fit, "window") == 0)
        m_initial_fit = FitMode::Window;
    else
        m_initial_fit = FitMode::None;

    if (toml.contains("keybindings"))
    {
        m_load_default_keybinding = false;
        auto keys                 = toml["keybindings"];
        m_actionMap               = {
            {"delete",
                           [this]()
                      {
            deleteKeyAction();
        }},
            {"keybindings",
                           [this]()
                      {
            ShowKeybindings();
        }},
            {"select_all",
                           [this]()
                      {
            SelectAll();
        }},
            {"save",
                           [this]()
                      {
            SaveFile();
        }},
            {"save_as",
                           [this]()
                      {
            SaveAsFile();
        }},
            {"yank",
                           [this]()
                      {
            YankSelection();
        }},
            {"cancel_selection",
                           [this]()
                      {
            clearTextSelection();
        }},
            {"about",
                           [this]()
                      {
            ShowAbout();
        }},
            {"link_hint_visit",
                           [this]()
                      {
            VisitLinkKB();
        }},
            {"link_hint_copy",
                           [this]()
                      {
            CopyLinkKB();
        }},
            {"outline",
                           [this]()
                      {
            TableOfContents();
        }},
            {"rotate_clock",
                           [this]()
                      {
            RotateClock();
        }},
            {"rotate_anticlock",
                           [this]()
                      {
            RotateAntiClock();
        }},
            {"prev_location",
                           [this]()
                      {
            GoBackHistory();
        }},
            {"scroll_down",
                           [this]()
                      {
            ScrollDown();
        }},
            {"scroll_up",
                           [this]()
                      {
            ScrollUp();
        }},
            {"scroll_left",
                           [this]()
                      {
            ScrollLeft();
        }},
            {"scroll_right",
                           [this]()
                      {
            ScrollRight();
        }},
            {"invert_color",
                           [this]()
                      {
            InvertColor();
        }},
            {"search",
                           [this]()
                      {
            Search();
        }},
            {"search_next",
                           [this]()
                      {
            nextHit();
        }},
            {"search_prev",
                           [this]()
                      {
            prevHit();
        }},
            {"next_page",
                           [this]()
                      {
            NextPage();
        }},
            {"prev_page",
                           [this]()
                      {
            PrevPage();
        }},
            {"first_page",
                           [this]()
                      {
            FirstPage();
        }},
            {"last_page",
                           [this]()
                      {
            LastPage();
        }},
            {"zoom_in",
                           [this]()
                      {
            ZoomIn();
        }},
            {"zoom_out",
                           [this]()
                      {
            ZoomOut();
        }},
            {"zoom_reset",
                           [this]()
                      {
            ZoomReset();
        }},
            {"annot_select",
                           [this]()
                      {
            ToggleAnnotSelect();
        }},
            {"text_highlight",
                           [this]()
                      {
            ToggleTextHighlight();
        }},
            {"annot_rect",
                           [this]()
                      {
            ToggleRectAnnotation();
        }},
            {"fullscreen",
                           [this]()
                      {
            ToggleFullscreen();
        }},
            {"file_properties",
                           [this]()
                      {
            FileProperties();
        }},
            {"open_file",
                           [this]()
                      {
            OpenFile();
        }},
            {"close_file",
                           [this]()
                      {
            CloseFile();
        }},
            {"fit_width",
                           [this]()
                      {
            FitWidth();
        }},
            {"fit_height",
                           [this]()
                      {
            FitHeight();
        }},
            {"fit_window",
                           [this]()
                      {
            FitWindow();
        }},
            {"auto_resize",
                           [this]()
                      {
            ToggleAutoResize();
        }},
            {"toggle_menubar",
                           [this]()
                      {
            ToggleMenubar();
        }},
            {"toggle_panel",
                           [this]()
                      {
            TogglePanel();
        }},

        };

        for (auto &[action, value] : *keys.as_table())
        {
            if (value.is_value())
                setupKeybinding(QString::fromStdString(std::string(action.str())),
                                QString::fromStdString(value.value_or<std::string>("")));
        }
    }

    if (compact)
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_panel->layout()->setContentsMargins(5, 1, 5, 1);
        m_panel->setContentsMargins(0, 0, 0, 0);
        this->setContentsMargins(0, 0, 0, 0);
    }
}

void
dodo::initKeybinds() noexcept
{
    m_shortcuts_map["toggle_menubar"]  = "Ctrl+Shift+m";
    m_shortcuts_map["invert_color"]    = "b";
    m_shortcuts_map["link_hint_visit"] = "f";
    m_shortcuts_map["save"]            = "Ctrl+s";
    m_shortcuts_map["text_highlight"]  = "Alt+1";
    m_shortcuts_map["annot_rect"]      = "Alt+2";
    m_shortcuts_map["annot_edit"]      = "Alt+3";
    m_shortcuts_map["outline"]         = "t";
    m_shortcuts_map["search"]          = "/";
    m_shortcuts_map["search_next"]     = "n";
    m_shortcuts_map["search_prev"]     = "Shift+n";
    m_shortcuts_map["zoom_in"]         = "+";
    m_shortcuts_map["zoom_out"]        = "-";
    m_shortcuts_map["zoom_reset"]      = "0";
    m_shortcuts_map["prev_location"]   = "Ctrl+o";
    m_shortcuts_map["open"]            = "o";
    m_shortcuts_map["scroll_left"]     = "h";
    m_shortcuts_map["scroll_down"]     = "j";
    m_shortcuts_map["scroll_up"]       = "k";
    m_shortcuts_map["scroll_right"]    = "l";
    m_shortcuts_map["next_page"]       = "Shift+j";
    m_shortcuts_map["prev_page"]       = "Shift+k";
    m_shortcuts_map["first_page"]      = "g,g";
    m_shortcuts_map["last_page"]       = "Shift+g";

    std::vector<std::pair<QString, std::function<void()>>> shortcuts = {
        {"Ctrl+Shift+m",
         [this]()
    {
        ToggleMenubar();
    }},
        {"i",
         [this]()
    {
        InvertColor();
    }},
        {"b",
         [this]()
    {
        GotoPage();
    }},
        {"f",
         [this]()
    {
        VisitLinkKB();
    }},
        {"Ctrl+s",
         [this]()
    {
        SaveFile();
    }},
        {"Alt+1",
         [this]()
    {
        ToggleTextHighlight();
    }},
        {"Alt+2",
         [this]()
    {
        ToggleRectAnnotation();
    }},
        {"Alt+3",
         [this]()
    {
        ToggleAnnotSelect();
    }},
        {"t",
         [this]()
    {
        TableOfContents();
    }},
        {"/",
         [this]()
    {
        Search();
    }},
        {"n",
         [this]()
    {
        nextHit();
    }},
        {"Shift+n",
         [this]()
    {
        prevHit();
    }},
        {"Ctrl+o",
         [this]()
    {
        GoBackHistory();
    }},
        {"o",
         [this]()
    {
        OpenFile();
    }},
        {"j",
         [this]()
    {
        ScrollDown();
    }},
        {"k",
         [this]()
    {
        ScrollUp();
    }},
        {"h",
         [this]()
    {
        ScrollLeft();
    }},
        {"l",
         [this]()
    {
        ScrollRight();
    }},
        {"Shift+j",
         [this]()
    {
        NextPage();
    }},
        {"Shift+k",
         [this]()
    {
        PrevPage();
    }},
        {"g,g",
         [this]()
    {
        FirstPage();
    }},
        {"Shift+g",
         [this]()
    {
        LastPage();
    }},
        {"0",
         [this]()
    {
        ZoomReset();
    }},
        {"=",
         [this]()
    {
        ZoomIn();
    }},
        {"-",
         [this]()
    {
        ZoomOut();
    }},
        {"<",
         [this]()
    {
        RotateAntiClock();
    }},
        {">",
         [this]()
    {
        RotateClock();
    }},
    };

    for (const auto &[key, func] : shortcuts)
    {
        auto *sc = new QShortcut(QKeySequence(key), this);
        connect(sc, &QShortcut::activated, func);
    }
}

void
dodo::initGui() noexcept
{
    QWidget *widget = new QWidget();
    // Panel
    m_panel = new Panel(this);
    m_panel->setMode(GraphicsView::Mode::TextSelection);

    widget->setLayout(m_layout);
    m_layout->addWidget(m_gview);
    m_layout->addWidget(m_panel);
    m_gview->setRenderHint(QPainter::Antialiasing);
    m_gview->setScene(m_gscene);
    this->setCentralWidget(widget);

    m_gscene->addItem(m_pix_item);
    m_gview->setPixmapItem(m_pix_item);
    m_gview->setEnabled(false);
    m_model      = new Model(m_gscene);
    m_vscrollbar = m_gview->verticalScrollBar();

    auto win = m_gview->window()->windowHandle();
    if (win)
    {
        connect(win, &QWindow::screenChanged, m_gview, [&](QScreen *screen)
        {
            m_cache.clear();
            auto dpr = m_gview->window()->devicePixelRatioF();
            m_model->setDPR(dpr);
            m_dpr     = dpr;
            m_inv_dpr = 1 / dpr;
            if (m_pageno != -1)
                renderPage(m_pageno, true);
        });
    }

    m_menuBar = this->menuBar(); // initialize here so that the config visibility works
}

void
dodo::updateUiEnabledState() noexcept
{
    const bool hasOpenedFile = m_model->valid();

    m_actionZoomIn->setEnabled(hasOpenedFile);
    m_actionZoomOut->setEnabled(hasOpenedFile);
    m_actionFirstPage->setEnabled(hasOpenedFile);
    m_actionPrevPage->setEnabled(hasOpenedFile);
    m_actionNextPage->setEnabled(hasOpenedFile);
    m_actionLastPage->setEnabled(hasOpenedFile);
    m_actionFileProperties->setEnabled(hasOpenedFile);
    m_actionCloseFile->setEnabled(hasOpenedFile);
    m_fitMenu->setEnabled(hasOpenedFile);
    m_actionToggleOutline->setEnabled(hasOpenedFile);
    m_actionInvertColor->setEnabled(hasOpenedFile);
    m_actionSaveFile->setEnabled(hasOpenedFile);
    m_actionSaveAsFile->setEnabled(hasOpenedFile);
    m_actionPrevLocation->setEnabled(hasOpenedFile);
}

void
dodo::OpenFile() noexcept
{
    QString filepath = QFileDialog::getOpenFileName(this, "Open File", "", "PDF Files (*.pdf);; All Files (*)");
    if (filepath.isEmpty())
        return;

    openFile(filepath);
}

void
dodo::CloseFile() noexcept
{
    if (m_model->hasUnsavedChanges())
    {
        auto action =
            QMessageBox::question(this, "Unsaved changes detected",
                                  "There are unsaved changes in this document. Do you really want to close this file ?",
                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);

        switch (action)
        {
            case QMessageBox::StandardButton::Save:
                SaveFile();
                break;

            case QMessageBox::StandardButton::Discard:
                break;

            case QMessageBox::Cancel:
            default:
                return;
        }
    }

    m_cache.clear();

    clearLinks();

    if (m_panel->searchMode())
        m_panel->setSearchMode(false);

    if (m_highlights_present)
    {
        clearHighlights();
        clearIndexHighlights();
    }

    if (m_selection_present)
        clearTextSelection();

    if (m_annot_selection_present)
        clearAnnotSelection();

    m_filename.clear();
    m_panel->setFileName("");

    if (m_owidget)
    {
        m_owidget->close();
        m_owidget->deleteLater();
        m_owidget = nullptr;
    }

    if (m_propsWidget)
    {
        m_propsWidget->close();
        m_propsWidget->deleteLater();
        m_propsWidget = nullptr;
    }

    if (!m_pix_item->pixmap().isNull())
        m_pix_item->setPixmap(QPixmap());
    m_gview->setSceneRect(m_pix_item->boundingRect());
    m_model->closeFile();
    m_gview->setEnabled(false);
    updateUiEnabledState();
}

void
dodo::clearPixmapItems() noexcept
{
    if (m_pix_item->pixmap().isNull())
        return;

    QList<QGraphicsItem *> items = m_pix_item->childItems();
    for (const auto &item : items)
        m_gscene->removeItem(item);
    qDeleteAll(items);
}

/* Function for opening the file using the model.
   For internal usage only */
void
dodo::openFile(const QString &fileName) noexcept
{

    if (m_model->valid())
        CloseFile();

    m_filename = fileName;
    m_filename.replace("~", QString::fromStdString(getenv("HOME")));

    clearPixmapItems();

    if (!QFile::exists(m_filename))
    {
        qCritical() << "File does not exist: " << m_filename;
        return;
    }

    if (!m_model->openFile(m_filename))
    {
        QMessageBox::critical(this, "Error opening document", "Unable to open document for some reason");
        return;
    }

    if (m_model->passwordRequired())
        if (!askForPassword())
            return;

    // m_gview->setPixmapItem(m_pix_item);

    m_pageno      = -1;
    m_total_pages = m_model->numPages();
    m_panel->setTotalPageCount(m_total_pages);

    if (m_panel->searchMode())
        m_panel->setSearchMode(false);

    // Set the window title
    m_basename = QString(fz_basename(CSTR(m_filename)));
    this->setWindowTitle(QString(m_window_title).arg(m_basename));

    if (m_full_file_path_in_panel)
        m_panel->setFileName(m_filename);
    else
        m_panel->setFileName(fz_basename(CSTR(m_filename)));

    if (m_start_page_override >= 0)
        m_pageno = m_start_page_override;

    if (m_pageno != -1)
    {
        gotoPage(m_pageno);
        return;
    }

    int targetPage = 0;

    if (m_remember_last_visited)
    {
        QSqlQuery q;
        q.prepare("SELECT page_number FROM last_visited WHERE file_path = ?");
        q.addBindValue(m_filename);

        if (!q.exec())
        {
            qWarning() << "DB Error: " << q.lastError().text();
        }
        else if (q.next())
        {
            int page = q.value(0).toInt();
            if (page >= 0 && page < m_total_pages)
                targetPage = page;
        }
    }

    m_gview->setEnabled(true);
    gotoPage(targetPage);

    // Initialize zoom to be equal to fit to height
    int pixmapHeight = m_pix_item->pixmap().height();
    int viewHeight   = m_gview->viewport()->height() * m_dpr;

    qreal scale    = static_cast<qreal>(viewHeight) / pixmapHeight;
    m_default_zoom = scale;

    updateUiEnabledState();

    if (m_initial_fit != FitMode::None)
    {
        QTimer::singleShot(10, this, [this]()
        {
            switch (m_initial_fit)
            {
                case FitMode::Height:
                    FitHeight();
                    break;
                case FitMode::Width:
                    FitWidth();
                    break;
                case FitMode::Window:
                    FitWindow();
                    break;
                case FitMode::None:
                    break;
            }
        });
    }

    initSynctex();
}

bool
dodo::askForPassword() noexcept
{
    bool ok;
    auto pwd = QInputDialog::getText(this, "Document is locked", "Enter password", QLineEdit::EchoMode::Password,
                                     QString(), &ok);
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

void
dodo::RotateClock() noexcept
{
    if (!m_pix_item)
        return;

    m_rotation = (m_rotation + 90) % 360;
    m_cache.clear();
    renderPage(m_pageno, true);
    // m_gview->setSceneRect(m_pix_item->boundingRect());
    m_gview->centerOn(m_pix_item); // center view
}

void
dodo::RotateAntiClock() noexcept
{
    if (!m_pix_item)
        return;

    m_rotation = (m_rotation + 270) % 360;
    m_cache.clear();
    renderPage(m_pageno, true);
    // m_gview->setSceneRect(m_pix_item->boundingRect());
    m_gview->centerOn(m_pix_item); // center view
}

void
dodo::gotoPage(const int &pageno) noexcept
{
    if (!m_model->valid())
    {
        qWarning("Trying to go to page but no document loaded");
        return;
    }

    if (pageno < 0 || pageno >= m_total_pages)
    {
        qWarning("Page number %d out of range", pageno);
        return;
    }

    // boundary condition
    if (!m_suppressHistory)
    {
        m_page_history_list.push_back(m_pageno);
        if (m_page_history_list.size() > m_page_history_limit)
            m_page_history_list.removeFirst();
    }

    // TODO: Handle file content change detection

    if (m_highlights_present)
    {
        clearHighlights();
        clearIndexHighlights();
        // highlightHitsInPage();
        // highlightSingleHit();
    }

    if (m_annot_selection_present)
        clearAnnotSelection();

    if (m_selection_present)
        clearTextSelection();

    gotoPageInternal(pageno);
}

void
dodo::gotoPageInternal(const int &pageno) noexcept
{
    m_pageno = pageno;
    renderPage(pageno, false);
}

void
dodo::clearLinks() noexcept
{
    for (auto &link : m_gscene->items())
    {
        if (link->data(0).toString() == "link")
            m_gscene->removeItem(link);
    }
}

void
dodo::renderAnnotations(const QList<HighlightItem *> &annots) noexcept
{
    clearAnnots();
    for (auto *annot : annots)
    {
        connect(annot, &HighlightItem::annotDeleteRequested, m_model, [&](int index)
        {
            m_model->annotDeleteRequested(index);
            setDirty(true);
            renderPage(m_pageno, false);
        });

        connect(annot, &HighlightItem::annotColorChangeRequested, m_model, [&](int index)
        {
            auto color = QColorDialog::getColor(Qt::white, this, "Highlight Color",
                                                QColorDialog::ColorDialogOption::ShowAlphaChannel);
            if (color.isValid())
            {
                m_model->annotChangeColorForIndex(index, color);
                setDirty(true);
                renderPage(m_pageno);
            }
        });

        m_gscene->addItem(annot);
    }
}

void
dodo::clearAnnots() noexcept
{
    for (auto &link : m_gscene->items())
    {
        if (link->data(0).toString() == "annot")
            m_gscene->removeItem(link);
    }
}

void
dodo::renderLinks(const QList<BrowseLinkItem *> &links) noexcept
{
    clearLinks();
    for (auto *link : links)
    {
        connect(link, &BrowseLinkItem::linkCopyRequested, this, [&](const QString &link)
        {
            if (link.startsWith("#"))
            {
                auto equal_pos = link.indexOf("=");
                m_clipboard->setText(m_filename + "#" + link.mid(equal_pos + 1));
            }
            else
            {
                m_clipboard->setText(link);
            }
        });
        m_gscene->addItem(link);
    }
}

void
dodo::renderPage(int pageno, bool renderonly) noexcept
{
    if (!m_model->valid())
        return;

    CacheKey key{pageno, m_rotation, m_scale_factor};
    m_panel->setPageNo(m_pageno + 1);

    if (renderonly)
    {
        if (auto cached = m_cache.object(key))
        {
            qDebug() << "Using cached pixmap";
            renderPixmap(cached->pixmap);
            renderLinks(cached->links);
            return;
        }
    }

    auto pix              = m_model->renderPage(pageno, m_scale_factor, m_rotation, renderonly);
    auto links            = m_model->getLinks();
    auto annot_highlights = m_model->getAnnotations();

    m_cache.insert(key, new CacheValue(pix, links, annot_highlights));
    renderPixmap(pix);
    renderLinks(links);
    renderAnnotations(annot_highlights);

    // if (m_prefetch_enabled)
    //     prefetchAround(m_pageno);
}

void
dodo::renderPixmap(const QPixmap &pix) noexcept
{
    m_pix_item->setPixmap(pix);
}

void
dodo::prefetchAround(int currentPage) noexcept
{
    for (int offset = -m_prefetch_distance; offset <= m_prefetch_distance; ++offset)
    {
        int pageno = currentPage + offset;
        if (pageno >= 0 && pageno < m_total_pages)
        {
            qDebug() << "Prefetching page: " << pageno;
            CacheKey key{pageno, m_rotation, m_scale_factor};
            if (auto cached = m_cache.object(key))
            {
                qDebug() << pageno << " already cached; skipping prefetch";
                continue;
            }
            auto pix              = m_model->renderPage(pageno, m_scale_factor, m_rotation);
            auto links            = m_model->getLinks();
            auto annot_highlights = m_model->getAnnotations();

            m_cache.insert(key, new CacheValue(pix, links, annot_highlights));
        }
    }
}

void
dodo::FirstPage() noexcept
{
    gotoPage(0);
    TopOfThePage();
}

void
dodo::LastPage() noexcept
{
    gotoPage(m_total_pages - 1);
}

void
dodo::NextPage() noexcept
{
    gotoPage(m_pageno + 1);
}

void
dodo::PrevPage() noexcept
{
    gotoPage(m_pageno - 1);
}

void
dodo::ZoomIn() noexcept
{
    if (!m_model->valid())
        return;
    m_scale_factor *= m_zoom_by;
    zoomHelper();
}

void
dodo::ZoomReset() noexcept
{
    if (!m_model->valid())
        return;

    m_scale_factor = m_default_zoom;
    zoomHelper();
}

void
dodo::ZoomOut() noexcept
{
    if (!m_model->valid())
        return;

    if (m_scale_factor * 1 / m_zoom_by != 0)
    {
        // m_gview->scale(0.9, 0.9);
        m_scale_factor *= 1 / m_zoom_by;
        // m_gview->scale(1.1, 1.1);
        zoomHelper();
    }
}

void
dodo::Zoom(float factor) noexcept
{
    // TODO: Add constraints here
    if (!m_model->valid())
        return;

    // m_gview->scale(factor, factor);
    m_scale_factor = factor;
    zoomHelper();
}

void
dodo::zoomHelper() noexcept
{
    renderPage(m_pageno);
    if (m_highlights_present)
    {
        highlightSingleHit();
        highlightHitsInPage();
    }
    if (m_selection_present)
        clearTextSelection();
    m_gview->setSceneRect(m_pix_item->boundingRect());
    m_vscrollbar->setValue(m_vscrollbar->value());
    m_cache.clear();
}

void
dodo::ScrollDown() noexcept
{
    m_vscrollbar->setValue(m_vscrollbar->value() + 50);
}

void
dodo::ScrollUp() noexcept
{
    m_vscrollbar->setValue(m_vscrollbar->value() - 50);
}

void
dodo::ScrollLeft() noexcept
{
    auto hscrollbar = m_gview->horizontalScrollBar();
    hscrollbar->setValue(hscrollbar->value() - 50);
}

void
dodo::ScrollRight() noexcept
{
    auto hscrollbar = m_gview->horizontalScrollBar();
    hscrollbar->setValue(hscrollbar->value() + 50);
}

void
dodo::FitHeight() noexcept
{
    if (!m_pix_item || m_pix_item->pixmap().isNull())
        return;

    int pixmapHeight = m_pix_item->pixmap().height();
    int viewHeight   = m_gview->viewport()->height() * m_dpr;

    qreal scale = static_cast<qreal>(viewHeight) / pixmapHeight;
    m_scale_factor *= scale;
    zoomHelper();
    setFitMode(FitMode::Height);
    m_actionFitHeight->setChecked(true);
}

void
dodo::FitWidth() noexcept
{
    if (!m_pix_item || m_pix_item->pixmap().isNull())
        return;

    int pixmapWidth = m_pix_item->pixmap().width();
    int viewWidth   = m_gview->viewport()->width() * m_dpr;

    qreal scale = static_cast<qreal>(viewWidth) / pixmapWidth;
    m_scale_factor *= scale;
    zoomHelper();
    setFitMode(FitMode::Width);
    m_actionFitWidth->setChecked(true);
}

void
dodo::FitWindow() noexcept
{
    const int pixmapWidth  = m_pix_item->pixmap().width();
    const int pixmapHeight = m_pix_item->pixmap().height();
    const int viewWidth    = m_gview->viewport()->width() * m_dpr;
    const int viewHeight   = m_gview->viewport()->height() * m_dpr;

    // Calculate scale factors for both dimensions
    const qreal scaleX = static_cast<qreal>(viewWidth) / pixmapWidth;
    const qreal scaleY = static_cast<qreal>(viewHeight) / pixmapHeight;

    // Use the smaller scale to ensure the entire image fits in the window
    const qreal scale = std::min(scaleX, scaleY);

    m_scale_factor *= scale;
    zoomHelper();
    setFitMode(FitMode::Window);
    m_actionFitWindow->setChecked(true);
}

void
dodo::FitNone() noexcept
{
    setFitMode(FitMode::None);
    m_actionFitNone->setChecked(true);
}

void
dodo::setFitMode(const FitMode &mode) noexcept
{
    m_fit_mode = mode;
    switch (m_fit_mode)
    {
        case FitMode::Height:
            m_panel->setFitMode("Fit Height");
            break;

        case FitMode::Width:
            m_panel->setFitMode("Fit Width");
            break;

        case FitMode::Window:
            m_panel->setFitMode("Fit Window");
            break;

        case FitMode::None:
            m_panel->setFitMode("");
            break;
    }
}

// Single page search
void
dodo::search(const QString &term) noexcept
{
    m_last_search_term = term;
    // m_pdf_backend->search();
}

void
dodo::jumpToHit(int page, int index)
{
    if (m_searchRectMap[page].empty() || index < 0 || index >= m_searchRectMap[page].size())
        return;

    m_search_index    = index;
    m_search_hit_page = page;

    gotoPage(page); // Render page
    m_panel->setSearchIndex(m_searchRectMap[page][index].index + 1);

    highlightSingleHit();
    highlightHitsInPage();
}

void
dodo::highlightHitsInPage()
{
    clearHighlights();

    auto results = m_searchRectMap[m_pageno];

    for (const auto &result : results)
    {
        fz_quad quad      = result.quad;
        QRectF scaledRect = fzQuadToQRect(quad);
        auto *highlight   = new QGraphicsRectItem(scaledRect);

        highlight->setBrush(QColor::fromRgba(m_colors["search_match"]));
        highlight->setPen(Qt::NoPen);
        highlight->setData(0, "searchHighlight");
        m_gscene->addItem(highlight);
        // m_gview->centerOn(highlight);
    }
    m_highlights_present = true;
}

void
dodo::highlightSingleHit() noexcept
{
    if (m_highlights_present)
        clearIndexHighlights();

    fz_quad quad = m_searchRectMap[m_pageno][m_search_index].quad;

    QRectF scaledRect = fzQuadToQRect(quad);
    auto *highlight   = new QGraphicsRectItem(scaledRect);
    highlight->setBrush(QColor::fromRgba(m_colors["search_index"]));
    highlight->setPen(Qt::NoPen);
    highlight->setData(0, "searchIndexHighlight");

    m_gscene->addItem(highlight);
    m_gview->centerOn(highlight);
    m_highlights_present = true;
}

void
dodo::clearIndexHighlights()
{
    for (auto item : m_gscene->items())
    {
        if (auto rect = qgraphicsitem_cast<QGraphicsRectItem *>(item))
        {
            if (rect->data(0).toString() == "searchIndexHighlight")
            {
                m_gscene->removeItem(rect);
                delete rect;
            }
        }
    }
    m_highlights_present = false;
}

void
dodo::clearHighlights()
{
    for (auto item : m_gscene->items())
    {
        if (auto rect = qgraphicsitem_cast<QGraphicsRectItem *>(item))
        {
            if (rect->data(0).toString() == "searchHighlight")
            {
                m_gscene->removeItem(rect);
                delete rect;
            }
        }
    }
    m_highlights_present = false;
}

void
dodo::Search() noexcept
{
    if (!m_model->valid())
        return;

    auto term = QInputDialog::getText(this, "Search", "Search for");
    m_searchRectMap.clear();
    m_search_index = -1;
    if (term.isEmpty() || term.isNull())
    {
        m_panel->setSearchMode(false);
        if (m_highlights_present)
        {
            clearHighlights();
            clearIndexHighlights();
        }
        return;
    }

    m_last_search_term = term;
    m_panel->setSearchMode(true);
    m_search_index = 0;

    m_model->searchAll(term, hasUpperCase(term));
}

void
dodo::nextHit()
{
    if (m_search_hit_page == -1)
        return;

    QList<int> pages   = m_searchRectMap.keys();
    int currentPageIdx = pages.indexOf(m_search_hit_page);

    // Try next hit on current page
    if (m_search_index + 1 < m_searchRectMap[m_search_hit_page].size())
    {
        jumpToHit(m_search_hit_page, m_search_index + 1);
        return;
    }

    // Try next page
    if (currentPageIdx + 1 < pages.size())
    {
        if (!m_searchRectMap[pages[currentPageIdx + 1]].isEmpty())
            jumpToHit(pages[currentPageIdx + 1], 0);
    }
}

void
dodo::prevHit()
{
    if (m_search_hit_page == -1)
        return;

    QList<int> pages   = m_searchRectMap.keys();
    int currentPageIdx = pages.indexOf(m_search_hit_page);

    // Try previous hit on current page
    if (m_search_index - 1 >= 0)
    {
        jumpToHit(m_search_hit_page, m_search_index - 1);
        return;
    }

    // Try previous page
    if (currentPageIdx - 1 >= 0)
    {
        int prevPage     = pages[currentPageIdx - 1];
        int lastHitIndex = m_searchRectMap[prevPage].size() - 1;
        jumpToHit(prevPage, lastHitIndex);
    }
}

void
dodo::setupKeybinding(const QString &action, const QString &key) noexcept
{
    auto it = m_actionMap.find(action);
    if (it == m_actionMap.end())
        return;

    QShortcut *shortcut = new QShortcut(QKeySequence(key), this);
    connect(shortcut, &QShortcut::activated, this, [=]() { it.value()(); });

    m_shortcuts_map[action] = key;
}

void
dodo::GoBackHistory() noexcept
{
    if (!m_page_history_list.isEmpty())
    {
        int lastPage      = m_page_history_list.takeLast(); // Pop last page
        m_suppressHistory = true;
        gotoPage(lastPage);
        m_suppressHistory = false;
    }
}

void
dodo::closeEvent(QCloseEvent *e)
{
    if (!m_model)
        e->accept();

    if (m_remember_last_visited && !m_filename.isEmpty() && m_pageno >= 0)
    {
        QSqlQuery q;
        q.prepare("INSERT OR REPLACE INTO last_visited(file_path, page_number) VALUES (?, ?)");
        q.addBindValue(m_filename);
        q.addBindValue(m_pageno);
        q.exec();
    }

    if (m_model->hasUnsavedChanges())
    {
        auto reply = QMessageBox::question(
            this, "Unsaved Changes", "There are unsaved changes in this document. Do you want to quit ?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);

        switch (reply)
        {
            case QMessageBox::Save:
                SaveFile();
                break;

            case QMessageBox::Discard:
                break;

            case QMessageBox::Cancel:
                e->ignore();
                return;

            default:
                break;
        }
    }

    e->accept();
}

void
dodo::scrollToNormalizedTop(const double &ntop) noexcept
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
    int scrollPos = m_vscrollbar->value() + targetViewportPoint.y();

    m_vscrollbar->setValue(scrollPos);
}

void
dodo::TableOfContents() noexcept
{
    if (!m_model->valid())
        return;

    if (!m_owidget)
    {
        m_owidget = new OutlineWidget(m_model->clonedContext(), this);
        connect(m_owidget, &OutlineWidget::jumpToPageRequested, this, &dodo::gotoPage);
    }

    if (!m_owidget->hasOutline())
    {
        fz_outline *outline = m_model->getOutline();
        if (!outline)
        {
            QMessageBox::information(this, "Outline", "Document does not have outline information");
            return;
        }
        m_owidget->setOutline(outline);
    }
    m_owidget->open();
}

void
dodo::ToggleRectAnnotation() noexcept
{
    if (m_gview->mode() == GraphicsView::Mode::AnnotRect)
    {
        m_gview->setMode(GraphicsView::Mode::TextSelection);
        m_panel->setMode(GraphicsView::Mode::TextSelection);
        m_actionTextSelect->setChecked(true);
    }
    else
    {
        m_gview->setMode(GraphicsView::Mode::AnnotRect);
        m_panel->setHighlightColor(m_colors["annot_rect"]);
        m_panel->setMode(GraphicsView::Mode::AnnotRect);
        m_actionAnnotRect->setChecked(true);
    }
}

void
dodo::SaveFile() noexcept
{
    if (!m_model->valid())
        return;

    m_model->save();
    auto pix = m_model->renderPage(m_pageno, m_scale_factor, m_rotation);
    renderPixmap(pix);
}

void
dodo::SaveAsFile() noexcept
{
    if (!m_model->valid())
        return;

    auto filename = QFileDialog::getSaveFileName(this, "Save as", QString());

    if (filename.isEmpty())
        return;

    if (!m_model->saveAs(CSTR(filename)))
    {
        QMessageBox::critical(this, "Saving as failed", "Could not perform save as operation on the file");
    }
}

void
dodo::VisitLinkKB() noexcept
{
    if (!m_model->valid())
        return;

    this->installEventFilter(this);
    m_model->visitLinkKB(m_pageno, m_scale_factor);
    m_link_hint_mode = LinkHintMode::Visit;
    m_link_hint_map  = m_model->hintToLinkMap();
}

void
dodo::CopyLinkKB() noexcept
{
    if (!m_model->valid())
        return;

    this->installEventFilter(this);
    m_model->visitLinkKB(m_pageno, m_scale_factor);
    m_link_hint_mode = LinkHintMode::Copy;
    m_link_hint_map  = m_model->hintToLinkMap();
}

bool
dodo::eventFilter(QObject *obj, QEvent *event)
{
    if (m_link_hint_map.isEmpty())
    {
        m_currentHintInput.clear();
        qWarning() << "No links found to show hints";
        return true;
    }
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape)
        {
            m_currentHintInput.clear();
            m_model->clearKBHintsOverlay();
            this->removeEventFilter(this);
            return true;
        }
        else if (keyEvent->key() == Qt::Key_Backspace)
        {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            m_currentHintInput.removeLast();
#else
            if (!m_currentHintInput.isEmpty())
                m_currentHintInput.chop(1); // Removes the last character
#endif
            return true;
        }

        m_currentHintInput += keyEvent->text();
        if (m_link_hint_map.contains(m_currentHintInput))
        {
            Model::LinkInfo info = m_link_hint_map[m_currentHintInput];

            switch (m_link_hint_mode)
            {
                case LinkHintMode::None:
                    break;
                case LinkHintMode::Visit:
                    m_model->followLink(info);
                    break;

                case LinkHintMode::Copy:
                    m_clipboard->setText(info.uri);
                    break;
            }

            m_currentHintInput.clear();
            m_link_hint_map.clear();
            m_model->clearKBHintsOverlay();
            this->removeEventFilter(this);
            return true;
        }
        keyEvent->accept();
        return true;
    }

    if (event->type() == QEvent::ShortcutOverride)
    {
        QShortcutEvent *stEvent = static_cast<QShortcutEvent *>(event);
        stEvent->accept();
        return true;
    }

    // Let other events pass through
    return QObject::eventFilter(obj, event);
}

void
dodo::GotoPage() noexcept
{
    int pageno = QInputDialog::getInt(this, "Goto Page", QString("Enter page number ({} to {})").arg(0, m_total_pages),
                                      0, 1, m_total_pages);
    gotoPage(pageno - 1);
}

bool
dodo::hasUpperCase(const QString &text) noexcept
{
    for (const auto &c : text)
        if (c.isUpper())
            return true;

    return false;
}

void
dodo::FileProperties() noexcept
{
    if (!m_model->valid())
        return;

    if (!m_propsWidget)
    {
        m_propsWidget = new PropertiesWidget(this);
        auto props    = m_model->extractPDFProperties();
        m_propsWidget->setProperties(props);
    }
    m_propsWidget->exec();
}

// Puts the viewport to the top of the page
void
dodo::TopOfThePage() noexcept
{
    m_vscrollbar->setValue(0);
}

void
dodo::ToggleFullscreen() noexcept
{
    auto isFullscreen = this->isFullScreen();
    if (isFullscreen)
        this->showNormal();
    else
        this->showFullScreen();
    m_actionFullscreen->setChecked(!isFullscreen);
}

void
dodo::InvertColor() noexcept
{
    if (!m_model->valid())
        return;
    m_model->toggleInvertColor();
    renderPage(m_pageno);

    m_actionInvertColor->setChecked(m_model->invertColor());
}

void
dodo::ToggleAutoResize() noexcept
{
    m_auto_resize = !m_auto_resize;
    m_actionAutoresize->setChecked(m_auto_resize);
}

void
dodo::resizeEvent(QResizeEvent *e)
{
    if (m_auto_resize)
    {
        switch (m_fit_mode)
        {
            case FitMode::Height:
                FitHeight();
                break;

            case FitMode::Width:
                FitWidth();
                break;

            case FitMode::Window:
                FitWindow();
                break;
        }
    }

    e->accept();
}

void
dodo::TogglePanel() noexcept
{
    bool shown = !m_panel->isHidden();
    m_panel->setHidden(shown);
    m_actionTogglePanel->setChecked(!shown);
}

void
dodo::ToggleMenubar() noexcept
{
    bool shown = !m_menuBar->isHidden();
    m_menuBar->setHidden(shown);
    m_actionToggleMenubar->setChecked(!shown);
}

void
dodo::ShowAbout() noexcept
{
    AboutDialog *abw = new AboutDialog(this);
    abw->setAppInfo("v0.2.0", "A fast, unintrusive pdf reader");
    abw->exec();
}

void
dodo::readArgsParser(argparse::ArgumentParser &argparser) noexcept
{
    if (argparser["--version"] == true)
    {
        qInfo() << "dodo version: " << __DODO_VERSION;
        exit(0);
    }

    this->construct();
    if (argparser.is_used("--page"))
    {
        m_start_page_override = argparser.get<int>("--page");
        qDebug() << "Page number overriden with" << m_start_page_override;
    }

    if (argparser.is_used("--synctex-forward"))
    {
        m_start_page_override = -1; // do not override the page

        // Format: --synctex-forward={pdf}#{src}:{line}:{column}
        // Example: --synctex-forward=test.pdf#main.tex:14
        QString arg = QString::fromStdString(argparser.get<std::string>("--synctex-forward"));

        // Format: file.pdf#file.tex:line
        QRegularExpression re(R"(^(.*)#(.*):(\d+):(\d+)$)");
        QRegularExpressionMatch match = re.match(arg);

        if (match.hasMatch())
        {
            QString pdfPath = match.captured(1);
            pdfPath.replace("~", getenv("HOME"));
            QString texPath = match.captured(2);
            texPath.replace("~", getenv("HOME"));
            int line   = match.captured(3).toInt();
            int column = match.captured(4).toInt();

            qDebug() << "PDF:" << pdfPath << "Source:" << texPath << "Line:" << line << "Column:" << column;
            openFile(pdfPath);
            synctexLocateInPdf(texPath, line, column);
        }
        else
        {
            qWarning() << "Invalid --synctex-forward format. Expected file.pdf#file.tex:line:column";
        }
    }

    try
    {
        auto file = argparser.get<std::vector<std::string>>("files");
        if (!file.empty())
            openFile(QString::fromStdString(file[0]));
    }
    catch (...)
    {
    }
    m_start_page_override = -1;
}

QRectF
dodo::fzQuadToQRect(const fz_quad &q) noexcept
{
    fz_quad tq = fz_transform_quad(q, m_model->transform());

    return QRectF(tq.ul.x * m_inv_dpr, tq.ul.y * m_inv_dpr, (tq.ur.x - tq.ul.x) * m_inv_dpr,
                  (tq.ll.y - tq.ul.y) * m_inv_dpr);
}

void
dodo::initSynctex() noexcept
{
    m_synctex_scanner = synctex_scanner_new_with_output_file(CSTR(m_filename), nullptr, 1);
    if (!m_synctex_scanner)
    {
        qDebug() << "Unable to open SyncTeX scanner";
        return;
    }
}

void
dodo::synctexLocateInFile(const char *texFile, int line) noexcept
{
    auto tmp = m_synctex_editor_command;
    if (!tmp.contains("{file}") || !tmp.contains("{line}"))
    {
        QMessageBox::critical(this, "SyncTeX error",
                              "Invalid SyncTeX editor command: missing placeholders ({line} and/or {file}).");
        return;
    }

    auto args   = QProcess::splitCommand(tmp);
    auto editor = args.takeFirst();
    args.replaceInStrings("{line}", QString::number(line));
    args.replaceInStrings("{file}", texFile);

    QProcess::startDetached(editor, args);
}

void
dodo::synctexLocateInPdf(const QString &texFile, int line, int column) noexcept
{
    if (synctex_display_query(m_synctex_scanner, CSTR(texFile), line, column, -1) > 0)
    {
        synctex_node_p node = nullptr;
        int page;
        float x, y;
        while ((node = synctex_scanner_next_result(m_synctex_scanner)))
        {
            page = synctex_node_page(node);
            x    = synctex_node_visible_h(node);
            y    = synctex_node_visible_v(node);
        }
        gotoPage(page - 1);
        scrollToXY(x, y);
    }
}

fz_point
dodo::mapToPdf(QPointF loc) noexcept
{
    double x = loc.x() * m_dpr;
    double y = loc.y() * m_dpr;

    // Apply inverse of rendering transform
    fz_matrix invTransform;
    invTransform = fz_invert_matrix(m_model->transform());

    fz_point pt = {static_cast<float>(x), static_cast<float>(y)};
    pt          = fz_transform_point(pt, invTransform);
    return pt;
}

void
dodo::YankSelection() noexcept
{
    if (!m_model->valid())
        return;

    m_clipboard->setText(m_model->getSelectionText(m_gview->selectionStart(), m_gview->selectionEnd()));
    clearTextSelection();
}

void
dodo::ToggleTextHighlight() noexcept
{
    if (m_gview->mode() == GraphicsView::Mode::TextHighlight)
    {
        m_gview->setMode(GraphicsView::Mode::TextSelection);
        m_panel->setMode(GraphicsView::Mode::TextSelection);
        m_actionTextSelect->setChecked(true);
    }
    else
    {
        m_gview->setMode(GraphicsView::Mode::TextHighlight);
        m_panel->setHighlightColor(m_colors["highlight"]);
        m_panel->setMode(GraphicsView::Mode::TextHighlight);
        clearTextSelection();
        m_actionTextHighlight->setChecked(true);
    }
}

void
dodo::clearAnnotSelection() noexcept
{
    if (!m_annot_selection_present)
        return;

    for (const auto &item : m_gscene->items())
    {
        if (!m_selected_annots.contains(item->data(1).toInt()))
            continue;

        HighlightItem *gitem = dynamic_cast<HighlightItem *>(item);

        if (!gitem)
            continue;

        gitem->restoreBrushPen();
    }

    m_annot_selection_present = false;
}

void
dodo::ToggleAnnotSelect() noexcept
{
    if (m_gview->mode() == GraphicsView::Mode::AnnotSelect)
    {
        m_gview->setMode(GraphicsView::Mode::TextSelection);
        m_panel->setMode(GraphicsView::Mode::TextSelection);
        m_actionTextSelect->setChecked(true);
    }
    else
    {
        m_gview->setMode(GraphicsView::Mode::AnnotSelect);
        m_panel->setMode(GraphicsView::Mode::AnnotSelect);
        clearAnnotSelection();
        m_actionAnnotEdit->setChecked(true);
    }
}

void
dodo::SelectAll() noexcept
{
    auto end = m_pix_item->boundingRect().bottomRight();
    m_clipboard->setText(m_model->selectAllText(QPointF(0, 0), end));
}

void
dodo::populateRecentFiles() noexcept
{
    m_recentFilesMenu->clear();
    QSqlQuery query;
    if (query.exec("SELECT file_path, page_number, last_accessed "
                   "FROM last_visited "
                   "ORDER BY last_accessed DESC"))
    {
        while (query.next())
        {
            QString path = query.value(0).toString();
            if (path.isEmpty())
                continue;
            int page = query.value(1).toInt();
            // QDateTime accessed = query.value(2).toDateTime();
            QAction *fileAction = new QAction(path);
            connect(fileAction, &QAction::triggered, this, [&, path, page]()
            {
                openFile(path);
                gotoPage(page);
            });

            m_recentFilesMenu->addAction(fileAction);
        }
    }
    else
    {
        qWarning() << "Failed to query recent files:" << query.lastError().text();
    }

    if (m_recentFilesMenu->isEmpty())
        m_recentFilesMenu->setDisabled(true);
    else
        m_recentFilesMenu->setEnabled(true);
}

void
dodo::editLastPages() noexcept
{
    if (!m_remember_last_visited || !m_last_pages_db.isOpen() && !m_last_pages_db.isValid())
    {
        QMessageBox::information(this, "Edit Last Pages",
                                 "Couldn't find the database of last pages. Maybe "
                                 "`remember_last_visited` option is turned off in the config file");
        return;
    }

    EditLastPagesWidget *elpw = new EditLastPagesWidget(m_last_pages_db, this);
    elpw->show();
    connect(elpw, &EditLastPagesWidget::finished, this, &dodo::populateRecentFiles);
}

// Execution while pressing the "delete" action
// (not to be confused with the designated delete key on the keyboard)
void
dodo::deleteKeyAction() noexcept
{
    if (!m_selected_annots.empty())
    {
        m_model->deleteAnnots(m_selected_annots);
        setDirty(true);
        renderPage(m_pageno);
    }
}

void
dodo::setDirty(bool state) noexcept
{
    if (m_dirty == state)
        return;

    m_dirty = state;

    if (m_dirty)
    {
        if (!m_window_title.endsWith("*"))
            m_window_title.append("*");
    }
    else
    {
        if (m_window_title.endsWith("*"))
            m_window_title.chop(1);
    }

    this->setWindowTitle(QString(m_window_title).arg(m_basename));
}

void
dodo::TextSelectionMode() noexcept
{
    if (m_annot_selection_present)
        clearAnnotSelection();

    if (m_selection_present)
        clearTextSelection();

    m_gview->setMode(GraphicsView::Mode::TextSelection);
    m_panel->setMode(GraphicsView::Mode::TextSelection);

    m_actionTextSelect->setChecked(true);
}

void
dodo::fadeJumpMarker(JumpMarker *marker) noexcept
{
    auto *animation = new QPropertyAnimation(marker, "opacity");
    animation->setDuration(500); // 0.5 seconds fade
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    connect(animation, &QPropertyAnimation::finished, [marker]()
    {
        marker->hide();
        marker->setOpacity(1.0);
    });
}

void
dodo::ShowKeybindings() noexcept
{
    ShortcutsWidget *widget = new ShortcutsWidget(m_shortcuts_map, this);
    widget->show();
}

void
dodo::populateContextMenu(QMenu *menu) noexcept
{
    auto addAction = [menu, this](const QString &text, const auto &slot)
    {
        QAction *action = new QAction(text, menu); // sets parent = menu
        connect(action, &QAction::triggered, this, slot);
        menu->addAction(action);
    };
    switch (m_gview->mode())
    {
        case GraphicsView::Mode::TextSelection:
            addAction("Copy Text", &dodo::YankSelection);
            break;

        case GraphicsView::Mode::AnnotSelect:
            if (!m_annot_selection_present)
                return;
            addAction("Delete Annotations", &dodo::deleteKeyAction);
            addAction("Change color", &dodo::annotChangeColor);
            break;

        case GraphicsView::Mode::TextHighlight:
            addAction("Change color", &dodo::changeHighlighterColor);
            break;

        case GraphicsView::Mode::AnnotRect:
            addAction("Change color", &dodo::changeAnnotRectColor);
            break;
    }
}

// Change color for annots in the selection area
void
dodo::annotChangeColor() noexcept
{
    auto color =
        QColorDialog::getColor(Qt::red, this, "Highlight Color", QColorDialog::ColorDialogOption::ShowAlphaChannel);
    if (color.isValid())
    {
        m_model->annotChangeColorForIndexes(m_selected_annots, color);
    }
}

void
dodo::changeHighlighterColor() noexcept
{
    auto color = QColorDialog::getColor(m_colors["highlight"], this, "Highlighter Color",
                                        QColorDialog::ColorDialogOption::ShowAlphaChannel);
    if (color.isValid())
    {
        m_colors["highlight"] = color.rgba();
        m_model->setHighlightColor(color);
        m_panel->setHighlightColor(color);
    }
}

void
dodo::changeAnnotRectColor() noexcept
{
    auto color = QColorDialog::getColor(m_colors["annot_rect"], this, "Annot Rect Color",
                                        QColorDialog::ColorDialogOption::ShowAlphaChannel);
    if (color.isValid())
    {
        m_colors["annot_rect"] = color.rgba();
        m_model->setHighlightColor(color);
        m_panel->setHighlightColor(color);
    }
}

void
dodo::mouseWheelScrollRequested(int direction) noexcept
{
    if (direction < 0) // Up
    {
        if (m_vscrollbar->value() == m_vscrollbar->minimum())
        {
            PrevPage();
            m_vscrollbar->setValue(m_vscrollbar->maximum());
        }
    }
    else
    { // Down
        if (m_vscrollbar->value() == m_vscrollbar->maximum())
        {
            NextPage();
            m_vscrollbar->setValue(m_vscrollbar->minimum());
        }
    }
}
