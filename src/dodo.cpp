#include "dodo.hpp"

#include "AboutDialog.hpp"
#include "DocumentView.hpp"
#include "EditLastPagesWidget.hpp"
#include "GraphicsView.hpp"
#include "StartupWidget.hpp"
#include "toml.hpp"

#include <QColorDialog>
#include <QFile>
#include <QJsonArray>
#include <QSplitter>
#include <QStackedLayout>
#include <QStyleHints>
#include <qguiapplication.h>
#include <qnamespace.h>
#include <qstackedlayout.h>
#include <variant>

// Constructs the `dodo` class
dodo::dodo() noexcept
{
    setAttribute(Qt::WA_NativeWindow); // This is necessary for DPI updates
    setAcceptDrops(true);
    installEventFilter(this);
}

// Destructor for `dodo` class
dodo::~dodo() noexcept
{
    m_last_pages_db.close();
}

// On-demand construction of `dodo` (for use with argparse)
void
dodo::construct() noexcept
{
    m_tab_widget = new TabWidget();
    // m_config_watcher = new QFileSystemWatcher(this);

    initActionMap();
    initConfig();
    initGui();
    updateGUIFromConfig();
    if (m_load_default_keybinding)
        initKeybinds();
    initMenubar();
    initDB();
    trimRecentFilesDatabase();
    populateRecentFiles();
    setMinimumSize(600, 400);
    initConnections();
    this->show();
}

// Initialize the menubar related stuff
void
dodo::initMenubar() noexcept
{
    // --- File Menu ---
    QMenu *fileMenu = m_menuBar->addMenu("&File");
    fileMenu->addAction(
        QString("Open File\t%1").arg(m_config.shortcuts["open_file"]), this,
        [&]() { OpenFile(); });

    m_recentFilesMenu = fileMenu->addMenu("Recent Files");

    m_actionFileProperties
        = fileMenu->addAction(QString("File Properties\t%1")
                                  .arg(m_config.shortcuts["file_properties"]),
                              this, &dodo::FileProperties);

    m_actionOpenContainingFolder = fileMenu->addAction(
        QString("Open Containing Folder\t%1")
            .arg(m_config.shortcuts["open_containing_folder"]),
        this, &dodo::OpenContainingFolder);
    m_actionOpenContainingFolder->setEnabled(false);

    m_actionSaveFile = fileMenu->addAction(
        QString("Save File\t%1").arg(m_config.shortcuts["save"]), this,
        &dodo::SaveFile);

    m_actionSaveAsFile = fileMenu->addAction(
        QString("Save As File\t%1").arg(m_config.shortcuts["save_as"]), this,
        &dodo::SaveAsFile);

    QMenu *sessionMenu = fileMenu->addMenu("Session");

    sessionMenu->addAction("Save", this, [&]() { SaveSession(); });
    sessionMenu->addAction("Save As", this, [&]() { SaveAsSession(); });
    sessionMenu->addAction("Load", this, [&]() { LoadSession(); });

    m_actionCloseFile = fileMenu->addAction(
        QString("Close File\t%1").arg(m_config.shortcuts["close_file"]), this,
        &dodo::CloseFile);

    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, &QMainWindow::close);

    QMenu *editMenu = m_menuBar->addMenu("&Edit");
    m_actionUndo    = editMenu->addAction(
        QString("Undo\t%1").arg(m_config.shortcuts["undo"]), this, &dodo::Undo);
    m_actionRedo = editMenu->addAction(
        QString("Redo\t%1").arg(m_config.shortcuts["redo"]), this, &dodo::Redo);
    m_actionUndo->setEnabled(false);
    m_actionRedo->setEnabled(false);
    editMenu->addAction(
        QString("Last Pages\t%1").arg(m_config.shortcuts["edit_last_pages"]),
        this, &dodo::editLastPages);

    // --- View Menu ---
    QMenu *viewMenu    = m_menuBar->addMenu("&View");
    m_actionFullscreen = viewMenu->addAction(
        QString("Fullscreen\t%1").arg(m_config.shortcuts["fullscreen"]), this,
        &dodo::ToggleFullscreen);
    m_actionFullscreen->setCheckable(true);

    m_actionZoomIn = viewMenu->addAction(
        QString("Zoom In\t%1").arg(m_config.shortcuts["zoom_in"]), this,
        &dodo::ZoomIn);
    m_actionZoomOut = viewMenu->addAction(
        QString("Zoom Out\t%1").arg(m_config.shortcuts["zoom_out"]), this,
        &dodo::ZoomOut);

    viewMenu->addSeparator();

    m_fitMenu = viewMenu->addMenu("Fit");

    m_actionFitNone = m_fitMenu->addAction(
        QString("None\t%1").arg(m_config.shortcuts["fit_none"]), this,
        &dodo::FitNone);

    m_actionFitWidth = m_fitMenu->addAction(
        QString("Width\t%1").arg(m_config.shortcuts["fit_width"]), this,
        &dodo::FitWidth);

    m_actionFitHeight = m_fitMenu->addAction(
        QString("Height\t%1").arg(m_config.shortcuts["fit_height"]), this,
        &dodo::FitHeight);

    m_actionFitWindow = m_fitMenu->addAction(
        QString("Window\t%1").arg(m_config.shortcuts["fit_window"]), this,
        &dodo::FitWindow);

    m_fitMenu->addSeparator();

    // Auto Resize toggle (independent)
    m_actionAutoresize = m_fitMenu->addAction(
        QString("Auto Resize\t%1").arg(m_config.shortcuts["auto_resize"]), this,
        &dodo::ToggleAutoResize);
    m_actionAutoresize->setCheckable(true);
    m_actionAutoresize->setChecked(
        m_config.ui.auto_resize); // default on or off

    viewMenu->addSeparator();
    m_toggleMenu = viewMenu->addMenu("Show/Hide");

    m_actionToggleOutline = m_toggleMenu->addAction(
        QString("Outline\t%1").arg(m_config.shortcuts["outline"]), this,
        &dodo::ShowOutline);
    m_actionToggleOutline->setCheckable(true);
    m_actionToggleOutline->setChecked(!m_outline_widget->isHidden());

    m_actionToggleMenubar = m_toggleMenu->addAction(
        QString("Menubar\t%1").arg(m_config.shortcuts["toggle_menubar"]), this,
        &dodo::ToggleMenubar);
    m_actionToggleMenubar->setCheckable(true);
    m_actionToggleMenubar->setChecked(!m_menuBar->isHidden());

    m_actionToggleTabBar = m_toggleMenu->addAction(
        QString("Tabs\t%1").arg(m_config.shortcuts["toggle_tabs"]), this,
        &dodo::ToggleTabBar);
    m_actionToggleTabBar->setCheckable(true);
    m_actionToggleTabBar->setChecked(!m_tab_widget->tabBar()->isHidden());

    m_actionTogglePanel = m_toggleMenu->addAction(
        QString("Statusbar\t%1").arg(m_config.shortcuts["toggle_statusbar"]),
        this, &dodo::TogglePanel);
    m_actionTogglePanel->setCheckable(true);
    m_actionTogglePanel->setChecked(!m_panel->isHidden());

    m_actionInvertColor = viewMenu->addAction(
        QString("Invert Color\t%1").arg(m_config.shortcuts["invert_color"]),
        this, &dodo::InvertColor);
    m_actionInvertColor->setCheckable(true);
    m_actionInvertColor->setChecked(m_config.behavior.invert_mode);

    QMenu *toolsMenu = m_menuBar->addMenu("Tools");

    m_modeMenu = toolsMenu->addMenu("Mode");

    QActionGroup *modeActionGroup = new QActionGroup(this);
    modeActionGroup->setExclusive(true);

    m_actionRegionSelect = m_modeMenu->addAction(
        QString("Region Selection"), this, &dodo::RegionSelectionMode);
    m_actionRegionSelect->setCheckable(true);
    modeActionGroup->addAction(m_actionRegionSelect);

    m_actionTextSelect = m_modeMenu->addAction(QString("Text Selection"), this,
                                               &dodo::TextSelectionMode);
    m_actionTextSelect->setCheckable(true);
    modeActionGroup->addAction(m_actionTextSelect);

    m_actionTextHighlight = m_modeMenu->addAction(
        QString("Text Highlight\t%1").arg(m_config.shortcuts["text_highlight"]),
        this, &dodo::ToggleTextHighlight);
    m_actionTextHighlight->setCheckable(true);
    modeActionGroup->addAction(m_actionTextHighlight);

    m_actionAnnotRect = m_modeMenu->addAction(
        QString("Annotate Rectangle\t%1").arg(m_config.shortcuts["annot_rect"]),
        this, &dodo::ToggleRectAnnotation);
    m_actionAnnotRect->setCheckable(true);
    modeActionGroup->addAction(m_actionAnnotRect);

    m_actionAnnotEdit = m_modeMenu->addAction(
        QString("Edit Annotations\t%1").arg(m_config.shortcuts["annot_edit"]),
        this, &dodo::ToggleAnnotSelect);
    m_actionAnnotEdit->setCheckable(true);
    modeActionGroup->addAction(m_actionAnnotEdit);

    switch (m_config.behavior.initial_mode)
    {
        case GraphicsView::Mode::RegionSelection:
            m_actionRegionSelect->setChecked(true);
            break;
        case GraphicsView::Mode::TextSelection:
            m_actionTextSelect->setChecked(true);
            break;
        case GraphicsView::Mode::TextHighlight:
            m_actionTextHighlight->setChecked(true);
            break;
        case GraphicsView::Mode::AnnotSelect:
            m_actionAnnotEdit->setChecked(true);
            break;
        case GraphicsView::Mode::AnnotRect:
            m_actionAnnotRect->setChecked(true);
            break;
        default:
            break;
    }

    m_actionEncrypt = toolsMenu->addAction(
        QString("Encrypt Document\t%1").arg(m_config.shortcuts["encrypt"]),
        this, &dodo::EncryptDocument);
    m_actionEncrypt->setEnabled(false);

    m_actionDecrypt = toolsMenu->addAction(
        QString("Decrypt Document\t%1").arg(m_config.shortcuts["decrypt"]),
        this, &dodo::DecryptDocument);
    m_actionDecrypt->setEnabled(false);

    // --- Navigation Menu ---
    m_navMenu = m_menuBar->addMenu("&Navigation");

    m_navMenu->addAction(
        QString("StartPage\t%1").arg(m_config.shortcuts["startpage"]), this,
        &dodo::showStartupWidget);

    m_actionGotoPage = m_navMenu->addAction(
        QString("Goto Page\t%1").arg(m_config.shortcuts["goto_page"]), this,
        &dodo::GotoPage);

    m_actionFirstPage = m_navMenu->addAction(
        QString("First Page\t%1").arg(m_config.shortcuts["first_page"]), this,
        &dodo::FirstPage);

    m_actionPrevPage = m_navMenu->addAction(
        QString("Previous Page\t%1").arg(m_config.shortcuts["prev_page"]), this,
        &dodo::PrevPage);

    m_actionNextPage = m_navMenu->addAction(
        QString("Next Page\t%1").arg(m_config.shortcuts["next_page"]), this,
        &dodo::NextPage);
    m_actionLastPage = m_navMenu->addAction(
        QString("Last Page\t%1").arg(m_config.shortcuts["last_page"]), this,
        &dodo::LastPage);

    m_actionPrevLocation
        = m_navMenu->addAction(QString("Previous Location\t%1")
                                   .arg(m_config.shortcuts["prev_location"]),
                               this, &dodo::GoBackHistory);

    QMenu *helpMenu = m_menuBar->addMenu("&Help");
    m_actionAbout   = helpMenu->addAction(
        QString("About\t%1").arg(m_config.shortcuts["about"]), this,
        &dodo::ShowAbout);
    helpMenu->addAction(
        QString("Keybindings\t%1").arg(m_config.shortcuts["keybindings"]), this,
        &dodo::ShowKeybindings);

    updateUiEnabledState();
}

// Initialize the `last_pages_db` related stuff
void
dodo::initDB() noexcept
{
    m_last_pages_db = QSqlDatabase::addDatabase("QSQLITE");
    m_last_pages_db.setDatabaseName(m_config_dir.filePath("last_pages.db"));
    m_last_pages_db.open();

    QSqlQuery query;

    // FIXME: Maybe add some unique hashing so that this works even when you
    // move a file
    query.exec("CREATE TABLE IF NOT EXISTS last_visited ("
               "file_path TEXT PRIMARY KEY, "
               "page_number INTEGER, "
               "last_accessed TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");
}

// Initialize the default settings in case the config
// file is not found
void
dodo::initDefaults() noexcept
{
    m_config.ui.jump_marker_shown      = true;
    m_config.ui.full_filepath_in_panel = false;
    m_config.ui.zoom                   = 1.0f;
    m_config.ui.outline_shown          = false;
    m_config.ui.window_title_format    = QStringLiteral("%1 - dodo");
    m_config.ui.link_hint_size         = 16.0f;

    m_config.ui.colors[QStringLiteral("search_index")]
        = QStringLiteral("#3daee944");
    m_config.ui.colors[QStringLiteral("search_match")]
        = QStringLiteral("#55FF8844");
    m_config.ui.colors[QStringLiteral("accent")] = QStringLiteral("#FF500044");
    m_config.ui.colors[QStringLiteral("background")]
        = QStringLiteral("#00000000");
    m_config.ui.colors[QStringLiteral("link_hint_fg")]
        = QStringLiteral("#000000");
    m_config.ui.colors[QStringLiteral("link_hint_bg")]
        = QStringLiteral("#FFFF00");
    m_config.ui.colors[QStringLiteral("highlight")]
        = QStringLiteral("#55FFFF00");
    m_config.ui.colors[QStringLiteral("selection")]
        = QStringLiteral("#55000055");
    m_config.ui.colors[QStringLiteral("jump_marker")]
        = QStringLiteral("#FFFF0000");
    m_config.ui.colors[QStringLiteral("annot_rect")]
        = QStringLiteral("#55FF0000");

    m_config.rendering.dpi = 300.0f;
    m_config.rendering.dpr
        = m_screen_dpr_map.value(QApplication::primaryScreen()->name(), 1.0f);
    m_config.behavior.cache_pages = 10;

    m_config.behavior.undo_limit            = 25;
    m_config.behavior.remember_last_visited = true;
    m_config.behavior.page_history_limit    = 10;
    m_config.ui.auto_resize                 = false;
    m_config.ui.zoom_by                     = 1.25;
    m_config.ui.initial_fit                 = "width";
}

// Initialize the config related stuff
void
dodo::initConfig() noexcept
{
    m_config_dir = QDir(
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));

    // If config file path is not set, use the default one
    if (m_config_file_path.isEmpty())
        m_config_file_path = m_config_dir.filePath("config.toml");

    // if (!m_config_watcher->files().contains(m_config_file_path))
    //     m_config_watcher->addPath(m_config_file_path);

    m_session_dir = QDir(m_config_dir.filePath("sessions"));

    if (!QFile::exists(m_config_file_path))
    {
        initDefaults();
        return;
    }

    toml::table toml;

    try
    {
        toml = toml::parse_file(m_config_file_path.toStdString());
    }
    catch (std::exception &e)
    {
        QMessageBox::critical(
            this, "Error in configuration file",
            QString("There are one or more error(s) in your config "
                    "file:\n%1\n\nLoading default config.")
                .arg(e.what()));
        return;
    }

    auto ui = toml["ui"];

    m_config.ui.startup_tab      = ui["startup_tab"].value_or(true);
    m_config.ui.auto_hide_tabs   = ui["auto_hide_tabs"].value_or(false);
    m_config.ui.panel_shown      = ui["panel"].value_or(true);
    m_config.ui.outline_shown    = ui["outline"].value_or(false);
    m_config.ui.menubar_shown    = ui["menubar"].value_or(true);
    m_config.ui.tabs_shown       = ui["tabs"].value_or(true);
    m_config.ui.tabs_closable    = ui["tabs_closable"].value_or(true);
    m_config.ui.tabs_movable     = ui["tabs_movable"].value_or(true);
    m_config.ui.tab_elide_mode   = ui["tab_elide_mode"].value_or("right");
    m_config.ui.tab_bar_position = ui["tab_bar_position"].value_or("top");

    if (ui["fullscreen"].value_or(false))
        this->showFullScreen();

    m_config.ui.link_hint_size = ui["link_hint_size"].value_or(0.5f);

    m_config.ui.outline_as_side_panel
        = ui["outline_as_side_panel"].value_or(true);
    m_config.ui.outline_panel_width = ui["outline_panel_width"].value_or(300);
    m_config.ui.vscrollbar_shown    = ui["vscrollbar"].value_or(true);
    m_config.ui.hscrollbar_shown    = ui["hscrollbar"].value_or(true);
    m_config.ui.selection_drag_threshold
        = ui["selection_drag_threshold"].value_or(50);
    m_config.ui.jump_marker_shown = ui["jump_marker"].value_or(true);
    m_config.ui.full_filepath_in_panel
        = ui["full_file_path_in_panel"].value_or(false);
    m_config.ui.zoom          = ui["zoom_level"].value_or(1.0);
    m_config.ui.link_boundary = ui["link_boundary"].value_or(false);
    QString window_title
        = QString::fromStdString(ui["window_title"].value_or("{} - dodo"));
    window_title.replace("{}", "%1");
    m_config.ui.window_title_format = window_title;

    auto colors = toml["colors"];

    m_config.ui.colors["search_index"]
        = colors["search_index"].value_or("#3daee944");
    m_config.ui.colors["search_match"]
        = colors["search_match"].value_or("#FFFF8844");
    m_config.ui.colors["accent"]     = colors["accent"].value_or("#FF500044");
    m_config.ui.colors["background"] = colors["background"].value_or("#FFFFFF");
    m_config.ui.colors["link_hint_fg"]
        = colors["link_hint_fg"].value_or("#000000");
    m_config.ui.colors["link_hint_bg"]
        = colors["link_hint_bg"].value_or("#FFFF00");
    m_config.ui.colors["highlight"] = colors["highlight"].value_or("#55FFFF00");
    m_config.ui.colors["selection"] = colors["selection"].value_or("#550000FF");
    m_config.ui.colors["jump_marker"]
        = colors["jump_marker"].value_or("#FFFF0000");
    m_config.ui.colors["annot_rect"]
        = colors["annot_rect"].value_or("#55FF0000");

    auto rendering = toml["rendering"];

    // If DPR is specified in config, use that (can be scalar or map)
    if (rendering["dpr"])
    {
        if (rendering["dpr"].is_value())
        {
            m_config.rendering.dpr = rendering["dpr"].value_or(1.0f); // scalar
        }
        else if (rendering["dpr"].is_table())
        {
            auto dpr_table = rendering["dpr"];
            for (auto &[screen_name, value] : *dpr_table.as_table())
            {
                float dpr_value = value.value_or(1.0f);
                QString screen_str
                    = QString::fromStdString(std::string(screen_name.str()));
                QList<QScreen *> screens = QApplication::screens();
                for (QScreen *screen : screens)
                {
                    if (screen->name() == screen_str)
                    {
                        m_screen_dpr_map[screen->name()] = dpr_value;
                        break;
                    }
                }
            }
            m_config.rendering.dpr = m_screen_dpr_map;
        }
    }
    else
    {
        m_config.rendering.dpr = m_screen_dpr_map.value(
            QApplication::primaryScreen()->name(), 1.0f);
    }

    auto behavior = toml["behavior"];
    if (behavior["initial_mode"])
    {
        const std::string mode_str
            = behavior["initial_mode"].value_or("text_select");

        GraphicsView::Mode initial_mode;
        if (mode_str == "region_select")
            initial_mode = GraphicsView::Mode::RegionSelection;
        else if (mode_str == "text_select")
            initial_mode = GraphicsView::Mode::TextSelection;
        else if (mode_str == "text_highlight")
            initial_mode = GraphicsView::Mode::TextHighlight;
        else if (mode_str == "annot_rect")
            initial_mode = GraphicsView::Mode::AnnotRect;
        else if (mode_str == "annot_select")
            initial_mode = GraphicsView::Mode::AnnotSelect;
        else
            initial_mode = GraphicsView::Mode::TextSelection;
        m_config.behavior.initial_mode = initial_mode;
    }

    m_config.behavior.undo_limit  = behavior["undo_limit"].value_or(25);
    m_config.behavior.cache_pages = behavior["cache_pages"].value_or(10);
    m_config.behavior.remember_last_visited
        = behavior["remember_last_visited"].value_or(true);
    m_config.behavior.open_last_visited
        = behavior["open_last_visited"].value_or(false);
    m_config.behavior.page_history_limit
        = behavior["page_history"].value_or(100);
    m_config.rendering.antialiasing_bits
        = behavior["antialasing_bits"].value_or(8);
    m_config.ui.auto_resize = behavior["auto_resize"].value_or(false);
    m_config.ui.zoom_by     = behavior["zoom_factor"].value_or(1.25);
    m_config.ui.page_nav_with_mouse
        = behavior["page_nav_with_mouse"].value_or(false);
    m_config.behavior.synctex_editor_command = QString::fromStdString(
        behavior["synctex_editor_command"].value_or(""));
    m_config.behavior.invert_mode = behavior["invert_mode"].value_or(false);
    m_config.rendering.icc_color_profile
        = behavior["icc_color_profile"].value_or(true);
    m_config.ui.initial_fit        = behavior["initial_fit"].value_or("width");
    m_config.behavior.auto_reload  = behavior["auto_reload"].value_or(true);
    m_config.behavior.recent_files = behavior["recent_files"].value_or(true);
    m_config.behavior.num_recent_files
        = behavior["num_recent_files"].value_or(10);

    if (toml.contains("keybindings"))
    {
        m_load_default_keybinding = false;
        auto keys                 = toml["keybindings"];

        for (auto &[action, value] : *keys.as_table())
        {
            if (value.is_value())
                setupKeybinding(
                    QString::fromStdString(std::string(action.str())),
                    QString::fromStdString(value.value_or<std::string>("")));
        }
    }
}

// Initialize the keybindings related stuff
void
dodo::initKeybinds() noexcept
{
    m_config.shortcuts[QStringLiteral("undo")] = QStringLiteral("u");
    m_config.shortcuts[QStringLiteral("redo")] = QStringLiteral("Ctrl+r");
    m_config.shortcuts[QStringLiteral("toggle_menubar")]
        = QStringLiteral("Ctrl+Shift+m");
    m_config.shortcuts[QStringLiteral("invert_color")]    = QStringLiteral("b");
    m_config.shortcuts[QStringLiteral("link_hint_visit")] = QStringLiteral("f");
    m_config.shortcuts[QStringLiteral("save")] = QStringLiteral("Ctrl+s");
    m_config.shortcuts[QStringLiteral("text_highlight")]
        = QStringLiteral("Alt+1");
    m_config.shortcuts[QStringLiteral("annot_rect")]  = QStringLiteral("Alt+2");
    m_config.shortcuts[QStringLiteral("annot_edit")]  = QStringLiteral("Alt+3");
    m_config.shortcuts[QStringLiteral("outline")]     = QStringLiteral("t");
    m_config.shortcuts[QStringLiteral("search")]      = QStringLiteral("/");
    m_config.shortcuts[QStringLiteral("search_next")] = QStringLiteral("n");
    m_config.shortcuts[QStringLiteral("search_prev")]
        = QStringLiteral("Shift+n");
    m_config.shortcuts[QStringLiteral("zoom_in")]    = QStringLiteral("+");
    m_config.shortcuts[QStringLiteral("zoom_out")]   = QStringLiteral("-");
    m_config.shortcuts[QStringLiteral("zoom_reset")] = QStringLiteral("0");
    m_config.shortcuts[QStringLiteral("prev_location")]
        = QStringLiteral("Ctrl+o");
    m_config.shortcuts[QStringLiteral("open")]         = QStringLiteral("o");
    m_config.shortcuts[QStringLiteral("scroll_left")]  = QStringLiteral("h");
    m_config.shortcuts[QStringLiteral("scroll_down")]  = QStringLiteral("j");
    m_config.shortcuts[QStringLiteral("scroll_up")]    = QStringLiteral("k");
    m_config.shortcuts[QStringLiteral("scroll_right")] = QStringLiteral("l");
    m_config.shortcuts[QStringLiteral("next_page")] = QStringLiteral("Shift+j");
    m_config.shortcuts[QStringLiteral("prev_page")] = QStringLiteral("Shift+k");
    m_config.shortcuts[QStringLiteral("first_page")] = QStringLiteral("g,g");
    m_config.shortcuts[QStringLiteral("last_page")] = QStringLiteral("Shift+g");

    // Helper lambda to create and connect shortcuts
    auto addShortcut = [this](const char *key, auto &&func)
    {
        auto *sc = new QShortcut(QKeySequence(QLatin1String(key)), this);
        connect(sc, &QShortcut::activated, std::forward<decltype(func)>(func));
    };

    addShortcut("Ctrl+Shift+m", [this]() { ToggleMenubar(); });
    addShortcut("i", [this]() { InvertColor(); });
    addShortcut("b", [this]() { GotoPage(); });
    addShortcut("f", [this]() { VisitLinkKB(); });
    addShortcut("Ctrl+s", [this]() { SaveFile(); });
    addShortcut("Alt+1", [this]() { ToggleTextHighlight(); });
    addShortcut("Alt+2", [this]() { ToggleRectAnnotation(); });
    addShortcut("Alt+3", [this]() { ToggleAnnotSelect(); });
    addShortcut("t", [this]() { ShowOutline(); });
    addShortcut("n", [this]() { NextHit(); });
    addShortcut("Shift+n", [this]() { PrevHit(); });
    addShortcut("Ctrl+o", [this]() { GoBackHistory(); });
    addShortcut("o", [this]() { OpenFile(); });
    addShortcut("j", [this]() { ScrollDown(); });
    addShortcut("k", [this]() { ScrollUp(); });
    addShortcut("h", [this]() { ScrollLeft(); });
    addShortcut("l", [this]() { ScrollRight(); });
    addShortcut("Shift+j", [this]() { NextPage(); });
    addShortcut("Shift+k", [this]() { PrevPage(); });
    addShortcut("g,g", [this]() { FirstPage(); });
    addShortcut("Shift+g", [this]() { LastPage(); });
    addShortcut("0", [this]() { ZoomReset(); });
    addShortcut("=", [this]() { ZoomIn(); });
    addShortcut("-", [this]() { ZoomOut(); });
    addShortcut("<", [this]() { RotateAnticlock(); });
    addShortcut(">", [this]() { RotateClock(); });
}

// Initialize the GUI related Stuff
void
dodo::initGui() noexcept
{
    QWidget *widget = new QWidget();

    // Panel
    m_panel = new Panel(this);
    m_panel->hidePageInfo(true);
    m_panel->setMode(GraphicsView::Mode::TextSelection);

    m_message_bar = new MessageBar(this);
    m_message_bar->setVisible(false);

    m_outline_widget = new OutlineWidget(this);

    connect(m_panel, &Panel::modeColorChangeRequested, this,
            [&](GraphicsView::Mode mode)
    {
        QColorDialog colorDialog(this);
        colorDialog.setOption(QColorDialog::ShowAlphaChannel, true);
        colorDialog.setWindowTitle("Select Color");
        if (colorDialog.exec() == QDialog::Accepted)
        {
            QColor color = colorDialog.selectedColor();
            auto model   = m_doc->model();
            if (mode == GraphicsView::Mode::AnnotRect)
                model->setAnnotRectColor(color);
            else if (mode == GraphicsView::Mode::TextHighlight)
                model->setHighlightColor(color);
            else if (mode == GraphicsView::Mode::TextSelection)
                model->setSelectionColor(color);
            m_panel->setHighlightColor(color);
        }
    });

    connect(m_panel, &Panel::pageChangeRequested, this, &dodo::gotoPage);

    widget->setLayout(m_layout);

    m_tab_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    this->setCentralWidget(widget);

    m_menuBar = this->menuBar(); // initialize here so that the config
                                 // visibility works

    if (m_config.ui.outline_as_side_panel)
    {
        QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
        splitter->addWidget(m_outline_widget);
        splitter->addWidget(m_tab_widget);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setFrameShape(QFrame::NoFrame);
        splitter->setFrameShadow(QFrame::Plain);
        splitter->setHandleWidth(1);
        splitter->setContentsMargins(0, 0, 0, 0);
        splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        splitter->setSizes({m_config.ui.outline_panel_width,
                            this->width() - m_config.ui.outline_panel_width});
        m_layout->addWidget(splitter, 1);
    }
    else
    {
        m_layout->addWidget(m_tab_widget, 1);
        // Make the outline a popup panel
        m_outline_widget->setWindowFlags(Qt::Dialog);
        m_outline_widget->setWindowModality(Qt::NonModal);
    }
}

// Updates the UI elements checking if valid
// file is open or not
void
dodo::updateUiEnabledState() noexcept
{
    const bool hasOpenedFile = m_doc ? true : false;

    m_actionOpenContainingFolder->setEnabled(hasOpenedFile);
    m_actionZoomIn->setEnabled(hasOpenedFile);
    m_actionZoomOut->setEnabled(hasOpenedFile);
    m_actionGotoPage->setEnabled(hasOpenedFile);
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
    m_actionEncrypt->setEnabled(hasOpenedFile);
    m_actionDecrypt->setEnabled(hasOpenedFile);
    updateSelectionModeActions();
}

// Helper function to construct `QShortcut` Qt shortcut
// from the config file
void
dodo::setupKeybinding(const QString &action, const QString &key) noexcept
{
    auto it = m_actionMap.find(action);
    if (it == m_actionMap.end())
        return;

    QShortcut *shortcut = new QShortcut(QKeySequence(key), this);
    connect(shortcut, &QShortcut::activated, this, [=]() { it.value()({}); });

    m_config.shortcuts[action] = key;
}

// Toggles the fullscreen mode
void
dodo::ToggleFullscreen() noexcept
{
    bool isFullscreen = this->isFullScreen();
    if (isFullscreen)
        this->showNormal();
    else
        this->showFullScreen();
    m_actionFullscreen->setChecked(!isFullscreen);
}

// Toggles the panel
void
dodo::TogglePanel() noexcept
{
    bool shown = !m_panel->isHidden();
    m_panel->setHidden(shown);
    m_actionTogglePanel->setChecked(!shown);
}

// Toggles the menubar
void
dodo::ToggleMenubar() noexcept
{
    bool shown = !m_menuBar->isHidden();
    m_menuBar->setHidden(shown);
    m_actionToggleMenubar->setChecked(!shown);
}

// Shows the about page
void
dodo::ShowAbout() noexcept
{
    AboutDialog *abw = new AboutDialog(this);
    abw->setAppInfo(__DODO_VERSION, "A fast, unintrusive pdf reader");
    abw->exec();
}

// Reads the arguments passed with `dodo` from the
// commandline
void
dodo::readArgsParser(argparse::ArgumentParser &argparser) noexcept
{
    if (argparser["--version"] == true)
    {
        qInfo() << "dodo version: " << __DODO_VERSION;
        exit(0);
    }

    if (argparser.is_used("--config"))
    {
        m_config_file_path
            = QString::fromStdString(argparser.get<std::string>("--config"));
    }

    this->construct();

    if (argparser.is_used("--session"))
    {
        QString sessionName
            = QString::fromStdString(argparser.get<std::string>("--session"));
        LoadSession(sessionName);
    }

    if (argparser.is_used("--page"))
        m_config.behavior.startpage_override = argparser.get<int>("--page");

    if (argparser.is_used("--synctex-forward"))
    {
        m_config.behavior.startpage_override = -1; // do not override the page

        // Format: --synctex-forward={pdf}#{src}:{line}:{column}
        // Example: --synctex-forward=test.pdf#main.tex:14
        QString arg = QString::fromStdString(
            argparser.get<std::string>("--synctex-forward"));

        // Format: file.pdf#file.tex:line
        static const QRegularExpression re(
            QStringLiteral(R"(^(.*)#(.*):(\d+):(\d+)$)"));
        QRegularExpressionMatch match = re.match(arg);

        static const QString homeDir = QString::fromLocal8Bit(qgetenv("HOME"));
        if (match.hasMatch())
        {
            QString pdfPath = match.captured(1);
            pdfPath.replace(QLatin1Char('~'), homeDir);
            QString texPath = match.captured(2);
            texPath.replace(QLatin1Char('~'), homeDir);
            int line   = match.captured(3).toInt();
            int column = match.captured(4).toInt();
            Q_UNUSED(line);
            Q_UNUSED(column);
            OpenFile(pdfPath);
            // synctexLocateInPdf(texPath, line, column); TODO:
        }
        else
        {
            qWarning() << "Invalid --synctex-forward format. Expected "
                          "file.pdf#file.tex:line:column";
        }
    }

    try
    {
        auto files = argparser.get<std::vector<std::string>>("files");
        if (!files.empty())
        {
            OpenFiles(files);
            m_config.behavior.open_last_visited = false;
        }

        if (m_config.behavior.open_last_visited)
            openLastVisitedFile();
    }
    catch (...)
    {
    }

    if (m_tab_widget->count() == 0 && m_config.ui.startup_tab)
        showStartupWidget();
    m_config.behavior.startpage_override = -1;
}

// Populates the `QMenu` for recent files with
// recent files entries from the database
void
dodo::populateRecentFiles() noexcept
{
    if (!m_config.behavior.recent_files)
    {
        m_recentFilesMenu->setEnabled(false);
        return;
    }

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
            QAction *fileAction = new QAction(path, m_recentFilesMenu);
            connect(fileAction, &QAction::triggered, this, [&, path, page]()
            {
                OpenFile(path);
                gotoPage(page);
            });

            m_recentFilesMenu->addAction(fileAction);
        }
    }
    else
    {
        qWarning() << "Failed to query recent files:"
                   << query.lastError().text();
    }

    if (m_recentFilesMenu->isEmpty())
        m_recentFilesMenu->setDisabled(true);
    else
        m_recentFilesMenu->setEnabled(true);
}

// Opens a widget that allows to edit the `last_pages_db`
// entries
void
dodo::editLastPages() noexcept
{
    if (!m_config.behavior.remember_last_visited
        || (!m_last_pages_db.isOpen() && !m_last_pages_db.isValid()))
    {
        QMessageBox::information(
            this, "Edit Last Pages",
            "Couldn't find the database of last pages. Maybe "
            "`remember_last_visited` option is turned off in the config "
            "file");
        return;
    }

    EditLastPagesWidget *elpw = new EditLastPagesWidget(m_last_pages_db, this);
    elpw->show();
    connect(elpw, &EditLastPagesWidget::finished, this,
            &dodo::populateRecentFiles);
}

// Opens a widget which shows the currently set keybindings
void
dodo::ShowKeybindings() noexcept
{
    ShortcutsWidget *widget = new ShortcutsWidget(m_config.shortcuts, this);
    widget->show();
}

// Helper function to open last visited file
void
dodo::openLastVisitedFile() noexcept
{
    if (!m_last_pages_db.isOpen() || !m_last_pages_db.isValid())
        return;

    QSqlQuery q;
    q.prepare("SELECT file_path, page_number FROM last_visited ORDER BY "
              "last_accessed DESC");

    if (!q.exec())
    {
        qWarning() << "DB Error: " << q.lastError().text();
        return;
    }

    if (q.next())
    {
        QString last_visited_file = q.value(0).toString();
        int pageno                = q.value(1).toInt();

        if (QFile::exists(last_visited_file))
        {
            OpenFile(last_visited_file);
            gotoPage(pageno);
        }
    }
}

// Search for term
void
dodo::Search(const QString &term) noexcept
{
    if (m_doc)
        m_doc->Search(term);
}

// Zoom out the file
void
dodo::ZoomOut() noexcept
{
    if (m_doc)
        m_doc->ZoomOut();
}

// Zoom in the file
void
dodo::ZoomIn() noexcept
{
    if (m_doc)
        m_doc->ZoomIn();
}

// Resets zoom
void
dodo::ZoomReset() noexcept
{
    if (m_doc)
        m_doc->ZoomReset();
}

// Go to a particular page (asks user with a dialog)
void
dodo::GotoPage() noexcept
{
    if (!m_doc || !m_doc->model())
        return;

    int total = m_doc->model()->numPages();
    if (total == 0)
    {
        QMessageBox::information(this, "Goto Page",
                                 "This document has no pages");
        return;
    }

    int pageno = QInputDialog::getInt(
        this, "Goto Page", QString("Enter page number (1 to %1)").arg(total),
        1);

    if (pageno <= 0 || pageno > total)
    {
        QMessageBox::critical(this, "Goto Page",
                              QString("Page %1 is out of range").arg(pageno));
        return;
    }

    gotoPage(pageno);
}

// Go to a particular page (no dialog)
void
dodo::gotoPage(int pageno) noexcept
{
    if (m_doc)
        m_doc->GotoPage(pageno);
}

// Goes to the next search hit
void
dodo::NextHit() noexcept
{
    if (m_doc)
        m_doc->NextHit();
}

// Goes to the previous search hit
void
dodo::PrevHit() noexcept
{
    if (m_doc)
        m_doc->PrevHit();
}

// Scrolls left in the file
void
dodo::ScrollLeft() noexcept
{
    if (m_doc)
        m_doc->ScrollLeft();
}

// Scrolls right in the file
void
dodo::ScrollRight() noexcept
{
    if (m_doc)
        m_doc->ScrollRight();
}

// Scrolls up in the file
void
dodo::ScrollUp() noexcept
{
    if (m_doc)
        m_doc->ScrollUp();
}

// Scrolls down in the file
void
dodo::ScrollDown() noexcept
{
    if (m_doc)
        m_doc->ScrollDown();
}

// Rotates the file in clockwise direction
void
dodo::RotateClock() noexcept
{
    // TODO:
    if (m_doc)
        m_doc->RotateClock();
}

// Rotates the file in anticlockwise direction
void
dodo::RotateAnticlock() noexcept
{
    // TODO:
    if (m_doc)
        m_doc->RotateAntiClock();
}

// Shows link hints for each visible link to visit link
// using the keyboard
void
dodo::VisitLinkKB() noexcept
{
    if (m_doc)
    {
        m_link_hint_map = m_doc->LinkKB();
        if (!m_link_hint_map.isEmpty())
        {
            m_link_hint_current_mode = LinkHintMode::Visit;
            m_link_hint_mode         = true;
        }
    }
}

// Shows link hints for each visible link to copy link
// using the keyboard
void
dodo::CopyLinkKB() noexcept
{
    if (m_doc)
    {
        m_link_hint_map = m_doc->LinkKB();
        if (!m_link_hint_map.isEmpty())
        {
            m_link_hint_current_mode = LinkHintMode::Visit;
            m_link_hint_mode         = true;
        }
    }
}

// Clears the currently selected text in the file
void
dodo::ClearTextSelection() noexcept
{
    if (m_doc)
        m_doc->ClearTextSelection();
}

// Copies the text selection (if any) to the clipboard
void
dodo::YankSelection() noexcept
{
    if (m_doc)
        m_doc->YankSelection();
}

// Selects all the text in the file
void
dodo::YankAll() noexcept
{
    if (m_doc)
        m_doc->YankAll();
}

void
dodo::FitNone() noexcept
{
    // TODO: Implement `FitNone`
}

void
dodo::TextSelectionMode() noexcept
{
    // TODO: Implement `TextSelectionMode`
}

void
dodo::RegionSelectionMode() noexcept
{
    // TODO: Implement `RegionSelectionMode`
}

// Opens multiple files given a list of file paths
void
dodo::OpenFiles(const std::vector<std::string> &files) noexcept
{
    // QString working_dir = QDir::currentPath();
    for (const std::string &s : files)
        OpenFile(QString::fromStdString(s));
}

// Opens multiple files given a list of file paths
void
dodo::OpenFiles(const QStringList &files) noexcept
{
    for (const QString &file : files)
        OpenFile(file);
}

// Opens a file given the DocumentView pointer
bool
dodo::OpenFile(DocumentView *view) noexcept
{
    initTabConnections(view);
    view->setDPR(m_dpr);
    QString fileName = view->fileName();
    QString path     = QFileInfo(fileName).fileName();
    m_tab_widget->addTab(view, path);

    // Switch to already opened filepath, if it's open.
    auto it = m_path_tab_map.find(fileName);
    if (it != m_path_tab_map.end())
    {
        int existingIndex = m_tab_widget->indexOf(it.value());
        if (existingIndex != -1)
        {
            m_tab_widget->setCurrentIndex(existingIndex);
            return true;
        }
    }

    return false;
}

// Opens a file given the file path
bool
dodo::OpenFile(const QString &filePath) noexcept
{
    if (filePath.isEmpty())
    {
        QStringList files;
        files = QFileDialog::getOpenFileNames(
            this, "Open File", "", "PDF Files (*.pdf);; All Files (*)");
        if (files.empty())
            return false;
        else if (files.size() > 1)
        {
            OpenFiles(files);
            return true;
        }
    }

    // Switch to already opened filepath, if it's open.
    auto it = m_path_tab_map.find(filePath);
    if (it != m_path_tab_map.end())
    {
        int existingIndex = m_tab_widget->indexOf(it.value());
        if (existingIndex != -1)
        {
            m_tab_widget->setCurrentIndex(existingIndex);
            return true;
        }
    }

    static const QString &homeDir = QString::fromLocal8Bit(qgetenv("HOME"));
    QString fp                    = filePath;
    if (filePath.startsWith("~"))
        fp = fp.replace(QLatin1Char('~'), homeDir);

    if (!QFile::exists(fp))
    {
        QMessageBox::warning(this, "Open File",
                             QString("Unable to find %1").arg(fp));
        return false;
    }

    DocumentView *docwidget = new DocumentView(fp, m_config, m_tab_widget);

    if (!docwidget->fileOpenedSuccessfully())
    {
        docwidget->deleteLater();
        delete docwidget;
        QMessageBox::warning(this, "Open File",
                             QString("Failed to open %1").arg(fp));
        return false;
    }

    docwidget->setDPR(m_dpr);
    initTabConnections(docwidget);
    int index = m_tab_widget->addTab(docwidget, fp);
    m_tab_widget->setCurrentIndex(index);

    m_outline_widget->setOutline(m_doc ? m_doc->model()->getOutline()
                                       : nullptr);

    m_path_tab_map[filePath] = docwidget;

    if (m_config.ui.outline_shown)
        m_outline_widget->setVisible(m_config.ui.outline_shown);
    return true;
}

// Opens the properties widget with properties for the
// current file
void
dodo::FileProperties() noexcept
{
    if (m_doc)
        m_doc->FileProperties();
}

// Saves the current file
void
dodo::SaveFile() noexcept
{
    if (m_doc)
        m_doc->SaveFile();
}

// Saves the current file as a new file
void
dodo::SaveAsFile() noexcept
{
    if (m_doc)
        m_doc->SaveAsFile();
}

// Closes the current file
void
dodo::CloseFile() noexcept
{
    m_tab_widget->tabCloseRequested(m_tab_widget->currentIndex());
}

// Fit the document to the width of the window
void
dodo::FitWidth() noexcept
{
    if (m_doc)
        m_doc->FitWidth();
}

// Fit the document to the height of the window
void
dodo::FitHeight() noexcept
{
    if (m_doc)
        m_doc->FitHeight();
}

// Fit the document to the window
void
dodo::FitWindow() noexcept
{
    if (m_doc)
        m_doc->FitWindow();
}

// Toggle auto-resize mode
void
dodo::ToggleAutoResize() noexcept
{
    if (m_doc)
        m_doc->ToggleAutoResize();
}

// Show or hide the outline panel
void
dodo::ShowOutline() noexcept
{
    if (m_outline_widget->isVisible())
    {
        m_outline_widget->hide();
        m_actionToggleOutline->setChecked(false);
    }
    else
    {
        m_outline_widget->show();
        m_actionToggleOutline->setChecked(true);
    }
}

// Invert colors of the document
void
dodo::InvertColor() noexcept
{
    if (m_doc)
    {
        m_doc->InvertColor();
        m_actionInvertColor->setChecked(!m_actionInvertColor->isChecked());
    }
}

// Toggle text highlight mode
void
dodo::ToggleTextHighlight() noexcept
{
    if (m_doc)
        m_doc->ToggleTextHighlight();
}

// Toggle rectangle annotation mode
void
dodo::ToggleRectAnnotation() noexcept
{
    if (m_doc)
        m_doc->ToggleRectAnnotation();
}

// Toggle annotation select mode
void
dodo::ToggleAnnotSelect() noexcept
{
    if (m_doc)
        m_doc->ToggleAnnotSelect();
}

// Toggle region select mode
void
dodo::ToggleRegionSelect() noexcept
{
    if (m_doc)
        m_doc->ToggleRegionSelect();
}

// Go to the first page
void
dodo::FirstPage() noexcept
{
    if (m_doc)
        m_doc->FirstPage();
}

// Go to the previous page
void
dodo::PrevPage() noexcept
{
    if (m_doc)
        m_doc->PrevPage();
}

// Go to the next page
void
dodo::NextPage() noexcept
{
    if (m_doc)
        m_doc->NextPage();
}

// Go to the last page
void
dodo::LastPage() noexcept
{
    if (m_doc)
        m_doc->LastPage();

    updatePageNavigationActions();
}

// Go back in the page history
void
dodo::GoBackHistory() noexcept
{
    if (m_doc)
        m_doc->GoBackHistory();
}

// Highlight text annotation for the current selection
void
dodo::TextHighlightCurrentSelection() noexcept
{
    if (m_doc)
        m_doc->TextHighlightCurrentSelection();
}

// Initialize all the connections for the `dodo` class
void
dodo::initConnections() noexcept
{
    connect(m_config_watcher, &QFileSystemWatcher::fileChanged, this,
            &dodo::updateGUIFromConfig);
    QList<QScreen *> outputs = QGuiApplication::screens();
    connect(m_tab_widget, &QTabWidget::currentChanged, this,
            &dodo::handleCurrentTabChanged);

    QWindow *win = window()->windowHandle();

    m_dpr = m_screen_dpr_map.value(QGuiApplication::primaryScreen()->name(),
                                   1.0f);

    connect(win, &QWindow::screenChanged, this, [&](QScreen *screen)
    {
        if (std::holds_alternative<QMap<QString, float>>(
                m_config.rendering.dpr))
        {
            m_dpr = m_screen_dpr_map.value(screen->name(), 1.0f);
            if (m_doc)
                m_doc->setDPR(m_dpr);
        }
        else if (std::holds_alternative<float>(m_config.rendering.dpr))
        {
            m_dpr = std::get<float>(m_config.rendering.dpr);
        }
    });

    connect(m_tab_widget, &QTabWidget::tabCloseRequested, this,
            [this](const int index)
    {
        QWidget *widget = m_tab_widget->widget(index);
        if (!widget)
            return;
        const QString tabRole = widget->property("tabRole").toString();
        if (tabRole == "doc")
        {
            DocumentView *doc = qobject_cast<DocumentView *>(widget);

            if (doc)
            {
                m_path_tab_map.remove(doc->fileName());

                // Set the outline to nullptr if the closed tab was the
                // current one
                if (m_doc == doc)
                    m_outline_widget->setOutline(nullptr);
                doc->CloseFile();
            }
        }
        else if (tabRole == "startup")
        {
            if (m_startup_widget)
            {
                m_startup_widget->deleteLater();
                m_startup_widget = nullptr;
            }
        }

        m_tab_widget->removeTab(index);
        if (m_tab_widget->count() == 0)
        {
            m_doc = nullptr;
            updatePanel();
        }
    });

    connect(m_actionInvertColor, &QAction::triggered, [&]() { InvertColor(); });

    connect(m_navMenu, &QMenu::aboutToShow, this,
            &dodo::updatePageNavigationActions);

    connect(m_outline_widget, &OutlineWidget::jumpToPageRequested, this,
            &dodo::gotoPage);
}

// Handle when the file name is changed
void
dodo::handleFileNameChanged(const QString &name) noexcept
{
    m_panel->setFileName(name);
    this->setWindowTitle(name);
}

// Handle when the current tab is changed
void
dodo::handleCurrentTabChanged(int index) noexcept
{
    if (index == -1)
    {
        m_panel->hidePageInfo(true);
        updateMenuActions();
        updateUiEnabledState();
        updatePanel();
        this->setWindowTitle("");
        return;
    }

    QWidget *widget = m_tab_widget->widget(index);

    if (widget->property("tabRole") == "startup")
    {
        m_doc = nullptr;
        updateActionsAndStuffForSystemTabs();
        this->setWindowTitle("Startup");
        return;
    }

    if (widget->property("tabRole") == "empty")
    {
        m_doc = nullptr;
        updateActionsAndStuffForSystemTabs();
        this->setWindowTitle("Welcome");
        return;
    }

    m_doc = qobject_cast<DocumentView *>(widget);

    updateMenuActions();
    updateUiEnabledState();
    updatePanel();

    m_outline_widget->setOutline(m_doc ? m_doc->model()->getOutline()
                                       : nullptr);

    this->setWindowTitle(m_doc->windowTitle());
}

// Handle the close event to check for unsaved changes
void
dodo::closeEvent(QCloseEvent *e)
{
    // First pass: handle all unsaved changes dialogs and mark documents as
    // handled
    for (int i = 0; i < m_tab_widget->count(); i++)
    {
        DocumentView *doc
            = qobject_cast<DocumentView *>(m_tab_widget->widget(i));
        if (doc)
        {
            // Unsaved Changes
            if (doc->model()->hasUnsavedChanges())
            {
                int ret = QMessageBox::warning(
                    this, "Unsaved Changes",
                    QString("File %1 has unsaved changes. Do you want to save "
                            "them?")
                        .arg(m_tab_widget->tabText(i)),
                    QMessageBox::Save | QMessageBox::Discard
                        | QMessageBox::Cancel,
                    QMessageBox::Save);

                if (ret == QMessageBox::Cancel)
                {
                    e->ignore();
                    return;
                }
                else if (ret == QMessageBox::Save)
                {
                    if (!doc->model()->save())
                    {
                        QMessageBox::critical(this, "Save Failed",
                                              "Could not save the file.");
                        e->ignore();
                        return;
                    }
                }
            }
        }
    }
    e->accept();
}

void
dodo::ToggleTabBar() noexcept
{
    QTabBar *bar = m_tab_widget->tabBar();

    if (bar->isVisible())
        bar->hide();
    else
        bar->show();
}

// Event filter to capture key events for link hints mode and
// other events
bool
dodo::eventFilter(QObject *object, QEvent *event)
{
    const QEvent::Type type = event->type();
    if (m_link_hint_mode)
    {
        switch (type)
        {
            case QEvent::KeyPress:
            {
                QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
                switch (keyEvent->key())
                {
                    case Qt::Key_Escape:
                        m_currentHintInput.clear();
                        m_doc->clearKBHintsOverlay();
                        m_link_hint_map.clear();
                        m_link_hint_mode = false;
                        return true;
                    case Qt::Key_Backspace:
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
                        m_currentHintInput.removeLast();
#else
                        if (!m_currentHintInput.isEmpty())
                            m_currentHintInput.chop(1);
#endif
                        return true;
                    default:
                        break;
                }

                m_currentHintInput += keyEvent->text();
                int num = m_currentHintInput.toInt();
                auto it = m_link_hint_map.find(num);
                if (it != m_link_hint_map.end())
                {
                    const Model::LinkInfo &info = it.value();

                    switch (m_link_hint_current_mode)
                    {
                        case LinkHintMode::None:
                            break;

                        case LinkHintMode::Visit:
                            m_doc->model()->followLink(info);
                            break;

                        case LinkHintMode::Copy:
                            m_clipboard->setText(info.uri);
                            break;
                    }

                    m_currentHintInput.clear();
                    m_link_hint_map.clear();
                    m_doc->clearKBHintsOverlay();
                    m_link_hint_mode = false;
                    return true;
                }
                keyEvent->accept();
                return true;
            }
            case QEvent::ShortcutOverride:
                event->accept();
                return true;
            default:
                break;
        }
    }
    else
    {

        if (object == m_tab_widget->tabBar() && type == QEvent::ContextMenu)
        {
            QContextMenuEvent *contextEvent
                = static_cast<QContextMenuEvent *>(event);
            int index = m_tab_widget->tabBar()->tabAt(contextEvent->pos());

            if (index != -1)
            {
                QMenu menu;
                menu.addAction("Open Location", this, [this, index]()
                { openInExplorerForIndex(index); });
                menu.addAction("File Properties", this, [this, index]()
                { filePropertiesForIndex(index); });
                menu.addAction("Close Tab", this, [this, index]()
                { m_tab_widget->tabCloseRequested(index); });

                menu.exec(contextEvent->globalPos());
            }
            return true;
        }
    }

    if (event->type() == QEvent::Drop)
    {
        QDropEvent *e          = static_cast<QDropEvent *>(event);
        const QList<QUrl> urls = e->mimeData()->urls();

        for (const QUrl &url : urls)
        {
            if (url.isLocalFile())
            {
                QString filepath = url.toLocalFile();
                OpenFile(filepath);
            }
        }
        return true;
    }
    else if (event->type() == QEvent::DragEnter)
    {
        QDragEnterEvent *e = static_cast<QDragEnterEvent *>(event);
        if (e->mimeData()->hasUrls())
            e->acceptProposedAction();
        return true;
    }

    // Let other events pass through
    return QObject::eventFilter(object, event);
}

// Opens the file of tab with index `index`
// in file manager program
void
dodo::openInExplorerForIndex(int index) noexcept
{
    DocumentView *doc
        = qobject_cast<DocumentView *>(m_tab_widget->widget(index));
    if (doc)
    {
        QString filepath = doc->fileName();
        QDesktopServices::openUrl(QUrl(QFileInfo(filepath).absolutePath()));
    }
}

// Shows the properties of file of tab with index `index`
void
dodo::filePropertiesForIndex(int index) noexcept
{
    DocumentView *doc
        = qobject_cast<DocumentView *>(m_tab_widget->widget(index));
    doc->InvertColor();
    if (doc)
        doc->FileProperties();
}

// Initialize connections on each tab addition
void
dodo::initTabConnections(DocumentView *docwidget) noexcept
{
    connect(docwidget, &DocumentView::panelNameChanged, this,
            [this](const QString &name) { m_panel->setFileName(name); });

    // Connect undo stack signals to update undo/redo menu actions
    QUndoStack *undoStack = docwidget->model()->undoStack();
    connect(undoStack, &QUndoStack::canUndoChanged, this,
            [this, docwidget](bool canUndo)
    {
        if (m_doc == docwidget)
            m_actionUndo->setEnabled(canUndo);
    });
    connect(undoStack, &QUndoStack::canRedoChanged, this,
            [this, docwidget](bool canRedo)
    {
        if (m_doc == docwidget)
            m_actionRedo->setEnabled(canRedo);
    });

    connect(m_panel, &Panel::modeChangeRequested, docwidget,
            &DocumentView::nextSelectionMode);

    connect(m_panel, &Panel::fitModeChangeRequested, docwidget,
            &DocumentView::nextFitMode);

    connect(docwidget, &DocumentView::fileNameChanged, this,
            &dodo::handleFileNameChanged);

    connect(docwidget, &DocumentView::pageNumberChanged, m_panel,
            &Panel::setPageNo);

    connect(docwidget, &DocumentView::searchCountChanged, m_panel,
            &Panel::setSearchCount);

    connect(docwidget, &DocumentView::searchModeChanged, m_panel,
            &Panel::setSearchMode);

    connect(docwidget, &DocumentView::searchIndexChanged, m_panel,
            &Panel::setSearchIndex);

    connect(docwidget, &DocumentView::totalPageCountChanged, m_panel,
            &Panel::setTotalPageCount);

    connect(docwidget, &DocumentView::selectionModeChanged, m_panel,
            &Panel::setMode);

    connect(docwidget, &DocumentView::highlightColorChanged, m_panel,
            &Panel::setHighlightColor);

    connect(docwidget, &DocumentView::selectionModeChanged, m_panel,
            &Panel::setMode);

    connect(docwidget, &DocumentView::clipboardContentChanged, this,
            [&](const QString &text) { m_clipboard->setText(text); });

    connect(docwidget, &DocumentView::autoResizeActionUpdate, this,
            [&](bool state) { m_actionAutoresize->setChecked(state); });

    connect(docwidget, &DocumentView::insertToDBRequested, this,
            &dodo::insertFileToDB);
}

// Insert file to DB when tab is closed to track
// recent files
void
dodo::insertFileToDB(const QString &fname, int pageno) noexcept
{
    if (!m_last_pages_db.isValid() || !m_last_pages_db.isOpen())
        return;

    const QDateTime now = QDateTime::currentDateTime();
    QSqlQuery q(m_last_pages_db);
    q.prepare("UPDATE last_visited SET page_number = ?, last_accessed = ? "
              "WHERE file_path = ?");
    q.bindValue(0, pageno);
    q.bindValue(1, now);
    q.bindValue(2, fname);

    if (!q.exec() || q.numRowsAffected() == 0)
    {
        q.prepare("INSERT INTO last_visited (file_path, page_number, "
                  "last_accessed) VALUES (?, ?, ?)");
        q.bindValue(0, fname);
        q.bindValue(1, pageno);
        q.bindValue(2, now);
        q.exec();
    }
}

// Update the menu actions based on the current document state
void
dodo::updateMenuActions() noexcept
{
    const bool valid = m_doc != nullptr;

    m_panel->hidePageInfo(!valid);
    m_actionCloseFile->setEnabled(valid);

    if (valid)
    {
        Model *model = m_doc->model();
        if (model)
        {
            m_actionInvertColor->setEnabled(model->invertColor());
            QUndoStack *undoStack = model->undoStack();
            m_actionUndo->setEnabled(undoStack->canUndo());
            m_actionRedo->setEnabled(undoStack->canRedo());
        }
        else
            m_actionInvertColor->setEnabled(false);

        m_actionAutoresize->setCheckable(true);
        m_actionAutoresize->setChecked(m_doc->autoResize());
        m_actionTextSelect->setChecked(false);
        m_actionTextHighlight->setChecked(false);
        m_actionAnnotEdit->setChecked(false);
        m_actionAnnotRect->setChecked(false);
        updateSelectionModeActions();
    }
    else
    {
        m_actionInvertColor->setEnabled(false);
        m_actionAutoresize->setCheckable(false);

        m_actionTextSelect->setChecked(false);
        m_actionTextHighlight->setChecked(false);
        m_actionAnnotEdit->setChecked(false);
        m_actionAnnotRect->setChecked(false);
        m_actionUndo->setEnabled(false);
        m_actionRedo->setEnabled(false);
        m_modeMenu->setEnabled(false);
    }
}

// Update the panel info
void
dodo::updatePanel() noexcept
{
    if (m_doc)
    {
        Model *model = m_doc->model();
        if (!model)
            return;
        m_panel->setFileName(m_doc->windowTitle());
        m_panel->setHighlightColor(model->highlightColor());
        m_panel->setMode(m_doc->selectionMode());
        m_panel->setTotalPageCount(model->numPages());
        m_panel->setPageNo(m_doc->pageNo() + 1);
    }
    else
    {
        m_panel->hidePageInfo(true);
        m_panel->setFileName("");
        m_panel->setHighlightColor("");
        // m_panel->setMode();
    }
}

// Loads the given session (if it exists)
void
dodo::LoadSession(QString sessionName) noexcept
{
    QStringList existingSessions = getSessionFiles();
    if (existingSessions.empty())
    {
        QMessageBox::information(this, "Load Session", "No sessions found");
        return;
    }

    if (sessionName.isEmpty())
    {
        bool ok;
        sessionName = QInputDialog::getItem(
            this, "Save Session",
            "Enter name for session (existing sessions are listed): ",
            existingSessions, 0, true, &ok);
    }

    sessionName = sessionName + ".dodo";

    QFile file(m_session_dir.filePath(sessionName));
    if (file.open(QIODevice::ReadOnly))
    {
        QJsonDocument doc       = QJsonDocument::fromJson(file.readAll());
        QJsonArray sessionArray = doc.array();

        for (const QJsonValue &value : sessionArray)
        {
            QJsonObject entry = value.toObject();
            QString filePath  = entry["file_path"].toString();
            int page          = entry["current_page"].toInt();
            double zoom       = entry["zoom"].toDouble();
            int fitMode       = entry["fit_mode"].toInt();
            bool invert       = entry["invert_color"].toBool();

            DocumentView *view
                = new DocumentView(filePath, m_config, m_tab_widget);
            view->GotoPage(page);
            view->Zoom(zoom);
            view->Fit(static_cast<DocumentView::FitMode>(fitMode));
            if (invert)
                view->InvertColor();
            OpenFile(view);
        }
    }
    else
    {

        QMessageBox::critical(
            this, "Open Session",
            QStringLiteral("Could not open session: %1").arg(sessionName));
    }
}

// Returns the session files
QStringList
dodo::getSessionFiles() noexcept
{
    QStringList sessions;

    if (!m_session_dir.exists())
    {
        if (!m_session_dir.mkpath("."))
        {
            QMessageBox::warning(
                this, "Session Directory",
                "Unable to create sessions directory for some reason");
            return sessions;
        }
    }

    for (const QString &file : m_session_dir.entryList(
             QStringList() << "*.dodo", QDir::Files | QDir::NoSymLinks))
        sessions << QFileInfo(file).completeBaseName();

    return sessions;
}

// Saves the current session
void
dodo::SaveSession(QString sessionName) noexcept
{
    if (!m_doc)
    {
        QMessageBox::information(this, "Save Session",
                                 "No files in session to save the session");
        return;
    }

    if (m_session_name.isEmpty())
    {
        QStringList existingSessions = getSessionFiles();
        if (sessionName.isEmpty())
        {
            bool ok;
            sessionName = QInputDialog::getItem(
                this, "Save Session",
                "Enter name for session (existing sessions are listed): ",
                existingSessions, 0, true, &ok);
            if (sessionName.isEmpty())
                return;
        }
        else
        {
            if (existingSessions.contains(sessionName))
            {
                QMessageBox::warning(
                    this, "Save Session",
                    QString("Session name %1 already exists").arg(sessionName));
                return;
            }
        }
    }

    m_session_name = sessionName;

    QJsonArray sessionArray;

    for (int i = 0; i < m_tab_widget->count(); ++i)
    {
        DocumentView *doc
            = qobject_cast<DocumentView *>(m_tab_widget->widget(i));
        if (!doc || !doc->model()->valid())
            continue;

        QJsonObject entry;
        entry["file_path"]    = doc->fileName();
        entry["current_page"] = doc->pageNo();
        entry["zoom"]         = doc->zoom();
        entry["invert_color"] = doc->model()->invertColor();
        entry["rotation"]     = doc->rotation();
        entry["fit_mode"]     = static_cast<int>(doc->fitMode());

        sessionArray.append(entry);
    }

    QFile file(m_session_dir.filePath(m_session_name + ".dodo"));
    bool result = file.open(QIODevice::WriteOnly);
    if (!result)
    {
        QMessageBox::critical(
            this, "Save Session",
            QStringLiteral("Could not save session: %1").arg(m_session_name));
        return;
    }
    QJsonDocument doc(sessionArray);
    file.write(doc.toJson());
    file.write("\n\n// vim:ft=json");
    file.close();

    m_session_name = sessionName;
}

// Saves the current session under new name
void
dodo::SaveAsSession(const QString &sessionPath) noexcept
{
    Q_UNUSED(sessionPath);
    if (m_session_name.isEmpty())
    {
        QMessageBox::information(
            this, "Save As Session",
            "Cannot save session as you are not currently in a session");
        return;
    }

    QStringList existingSessions = getSessionFiles();

    QString selectedPath = QFileDialog::getSaveFileName(
        this, "Save As Session", m_session_dir.absolutePath(),
        "dodo session files (*.dodo); All Files (*.*)");

    if (selectedPath.isEmpty())
        return;

    if (QFile::exists(selectedPath))
    {
        auto choice = QMessageBox::warning(
            this, "Overwrite Session",
            QString("Session named \"%1\" already exists. Do you want to "
                    "overwrite it?")
                .arg(QFileInfo(selectedPath).fileName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (choice != QMessageBox::Yes)
            return;
    }

    // Save the session
    QString currentSessionPath
        = m_session_dir.filePath(m_session_name + ".dodo");
    if (!QFile::copy(currentSessionPath, selectedPath))
    {
        QMessageBox::critical(this, "Save As Session",
                              "Failed to save session.");
    }
}

// Shows the startup widget
void
dodo::showStartupWidget() noexcept
{
    if (m_startup_widget)
    {
        int index = m_tab_widget->indexOf(m_startup_widget);
        if (index != -1)
            m_tab_widget->setCurrentIndex(index);
        return;
    }

    m_startup_widget = new StartupWidget(m_last_pages_db, m_tab_widget);
    connect(m_startup_widget, &StartupWidget::openFileRequested, this,
            [this](const QString &filepath, int pageno)
    {
        bool opened = OpenFile(filepath);
        if (!opened)
            return;
        gotoPage(pageno);

        int index = m_tab_widget->indexOf(m_startup_widget);
        if (index != -1)
            m_tab_widget->tabCloseRequested(index);
    });
    int index = m_tab_widget->addTab(m_startup_widget, "Startup");
    m_tab_widget->setCurrentIndex(index);
    m_panel->setFileName("Startup");
}

// Update actions and stuff for system tabs
void
dodo::updateActionsAndStuffForSystemTabs() noexcept
{
    m_panel->hidePageInfo(true);
    updateUiEnabledState();
    m_panel->setFileName("Startup");
}

// Undo operation
void
dodo::Undo() noexcept
{
    if (m_doc && m_doc->model())
    {
        auto undoStack = m_doc->model()->undoStack();
        if (undoStack->canUndo())
            undoStack->undo();
    }
}

// Redo operation
void
dodo::Redo() noexcept
{
    if (m_doc && m_doc->model())
    {
        auto redoStack = m_doc->model()->undoStack();
        if (redoStack->canRedo())
            redoStack->redo();
    }
}

// Initialize the actions with corresponding functions
// to call
// Helper macro for actions that don't use arguments
#define ACTION_NO_ARGS(name, func)                                             \
    {QStringLiteral(name), [this](const QStringList &) { func(); }}

// Initialize the action map
void
dodo::initActionMap() noexcept
{
    m_actionMap = {
        // Actions with arguments
        {QStringLiteral("setdpr"),
         [this](const QStringList &args)
    {
        if (args.isEmpty())
            return;
        bool ok;
        float dpr = args.at(0).toFloat(&ok);
        if (ok)
            setDPR(dpr);
        else
            m_message_bar->showMessage(QStringLiteral("Invalid DPR"));
    }},
        {QStringLiteral("tabgoto"),
         [this](const QStringList &args)
    {
        if (args.isEmpty())
            return;
        bool ok;
        int index = args.at(0).toInt(&ok);
        if (ok)
            GotoTab(index);
        else
            m_message_bar->showMessage(QStringLiteral("Invalid tab index"));
    }},
        {QStringLiteral("region_select_mode"),
         [this](const QStringList &)
    {
        qDebug() << "DD";
        ToggleRegionSelect();
    }},

        // Actions without arguments
        ACTION_NO_ARGS("open_containing_folder", OpenContainingFolder),
        ACTION_NO_ARGS("tab_next", NextTab),
        ACTION_NO_ARGS("encrypt", EncryptDocument),
        ACTION_NO_ARGS("tab_prev", PrevTab),
        ACTION_NO_ARGS("tab_close", CloseTab),
        ACTION_NO_ARGS("reload", reloadDocument),
        ACTION_NO_ARGS("undo", Undo),
        ACTION_NO_ARGS("redo", Redo),
        ACTION_NO_ARGS("text_highlight_current_selection",
                       TextHighlightCurrentSelection),
        ACTION_NO_ARGS("toggle_tabs", ToggleTabBar),
        ACTION_NO_ARGS("keybindings", ShowKeybindings),
        ACTION_NO_ARGS("yank_all", YankAll),
        ACTION_NO_ARGS("save", SaveFile),
        ACTION_NO_ARGS("save_as", SaveAsFile),
        ACTION_NO_ARGS("yank", YankSelection),
        ACTION_NO_ARGS("cancel_selection", ClearTextSelection),
        ACTION_NO_ARGS("about", ShowAbout),
        ACTION_NO_ARGS("link_hint_visit", VisitLinkKB),
        ACTION_NO_ARGS("link_hint_copy", CopyLinkKB),
        ACTION_NO_ARGS("outline", ShowOutline),
        ACTION_NO_ARGS("rotate_clock", RotateClock),
        ACTION_NO_ARGS("rotate_anticlock", RotateAnticlock),
        ACTION_NO_ARGS("prev_location", GoBackHistory),
        ACTION_NO_ARGS("scroll_down", ScrollDown),
        ACTION_NO_ARGS("scroll_up", ScrollUp),
        ACTION_NO_ARGS("scroll_left", ScrollLeft),
        ACTION_NO_ARGS("scroll_right", ScrollRight),
        ACTION_NO_ARGS("invert_color", InvertColor),
        ACTION_NO_ARGS("search_next", NextHit),
        ACTION_NO_ARGS("search_prev", PrevHit),
        ACTION_NO_ARGS("next_page", NextPage),
        ACTION_NO_ARGS("prev_page", PrevPage),
        ACTION_NO_ARGS("goto_page", GotoPage),
        ACTION_NO_ARGS("first_page", FirstPage),
        ACTION_NO_ARGS("last_page", LastPage),
        ACTION_NO_ARGS("zoom_in", ZoomIn),
        ACTION_NO_ARGS("zoom_out", ZoomOut),
        ACTION_NO_ARGS("zoom_reset", ZoomReset),
        ACTION_NO_ARGS("annot_edit_mode", ToggleAnnotSelect),
        ACTION_NO_ARGS("text_highlight_mode", ToggleTextHighlight),
        ACTION_NO_ARGS("annot_rect_mode", ToggleRectAnnotation),
        ACTION_NO_ARGS("fullscreen", ToggleFullscreen),
        ACTION_NO_ARGS("file_properties", FileProperties),
        ACTION_NO_ARGS("open_file", OpenFile),
        ACTION_NO_ARGS("close_file", CloseFile),
        ACTION_NO_ARGS("fit_width", FitWidth),
        ACTION_NO_ARGS("fit_height", FitHeight),
        ACTION_NO_ARGS("fit_window", FitWindow),
        ACTION_NO_ARGS("auto_resize", ToggleAutoResize),
        ACTION_NO_ARGS("toggle_menubar", ToggleMenubar),
        ACTION_NO_ARGS("toggle_statusbar", TogglePanel),
        ACTION_NO_ARGS("toggle_focus_mode", ToggleFocusMode),

        // Tab navigation shortcuts
        {QStringLiteral("tab1"), [this](const QStringList &) { GotoTab(1); }},
        {QStringLiteral("tab2"), [this](const QStringList &) { GotoTab(2); }},
        {QStringLiteral("tab3"), [this](const QStringList &) { GotoTab(3); }},
        {QStringLiteral("tab4"), [this](const QStringList &) { GotoTab(4); }},
        {QStringLiteral("tab5"), [this](const QStringList &) { GotoTab(5); }},
        {QStringLiteral("tab6"), [this](const QStringList &) { GotoTab(6); }},
        {QStringLiteral("tab7"), [this](const QStringList &) { GotoTab(7); }},
        {QStringLiteral("tab8"), [this](const QStringList &) { GotoTab(8); }},
        {QStringLiteral("tab9"), [this](const QStringList &) { GotoTab(9); }},
    };
}

#undef ACTION_NO_ARGS

// Trims the recent files DB (if it exists) to `num_recent_files` number of
// files
void
dodo::trimRecentFilesDatabase() noexcept
{
    // If no DB is loaded, return from this function
    // If num_recent_files config entry has negative value,
    // retain all the recent files
    if (!m_last_pages_db.isValid() || m_config.behavior.num_recent_files < 0)
        return;

    QSqlQuery query;
    const QString trimQuery = QString("DELETE FROM last_visited "
                                      "WHERE file_path NOT IN ("
                                      "    SELECT file_path "
                                      "    FROM last_visited "
                                      "    ORDER BY last_accessed DESC "
                                      "    LIMIT %1"
                                      ")")
                                  .arg(m_config.behavior.num_recent_files);

    if (!query.exec(trimQuery))
    {
        qWarning() << "Failed to trim recent files:"
                   << query.lastError().text();
    }
}

// Sets the DPR of the current document
void
dodo::setDPR(float dpr) noexcept
{
    if (m_doc)
    {
        m_doc->FirstPage();
        m_doc->setDPR(dpr);
    }
}

// Reload the document in place
void
dodo::reloadDocument() noexcept
{
    // TODO: Fix this implementation
    if (m_doc)
        m_doc->model()->reloadDocument();
}

// Go to the first tab
void
dodo::FirstTab() noexcept
{
    if (m_tab_widget->count() != 0)
    {
        m_tab_widget->setCurrentIndex(0);
    }
}

// Go to the last tab
void
dodo::LastTab() noexcept
{
    int count = m_tab_widget->count();
    if (count != 0)
    {
        m_tab_widget->setCurrentIndex(count - 1);
    }
}

// Go to the next tab
void
dodo::NextTab() noexcept
{
    int count        = m_tab_widget->count();
    int currentIndex = m_tab_widget->currentIndex();
    if (count != 0 && currentIndex < count)
    {
        m_tab_widget->setCurrentIndex(currentIndex + 1);
    }
}

// Go to the previous tab
void
dodo::PrevTab() noexcept
{
    int count        = m_tab_widget->count();
    int currentIndex = m_tab_widget->currentIndex();
    if (count != 0 && currentIndex > 0)
    {
        m_tab_widget->setCurrentIndex(currentIndex - 1);
    }
}

// Go to the tab at nth position specified by `tabno`
void
dodo::GotoTab(int tabno) noexcept
{
    if (tabno > 0 && tabno <= m_tab_widget->count())
    {
        m_tab_widget->setCurrentIndex(tabno - 1);
    }
    else
    {
        m_message_bar->showMessage("Invalid Tab Number");
    }
}

// Close the current tab
void
dodo::CloseTab(int tabno) noexcept
{
    if (m_tab_widget->count() == 0)
        return;

    // Current tab
    if (tabno == -1)
    {
        m_tab_widget->tabCloseRequested(m_tab_widget->currentIndex());
    }
    else // someother tab that's not current
    {
        if (validTabIndex(tabno - 1))
            m_tab_widget->tabCloseRequested(tabno - 1);
        else
            m_message_bar->showMessage("Invalid tab index");
    }
}

// Useful for updating the Navigation QMenu
void
dodo::updatePageNavigationActions() noexcept
{
    const int page  = m_doc ? m_doc->pageNo() : -1;
    const int count = m_doc ? m_doc->count() : 0;

    m_actionFirstPage->setEnabled(page > 0);
    m_actionPrevPage->setEnabled(page > 0);
    m_actionNextPage->setEnabled(page >= 0 && page < count - 1);
    m_actionLastPage->setEnabled(page >= 0 && page < count - 1);
}

// Open the containing folder of the current document
void
dodo::OpenContainingFolder() noexcept
{
    if (m_doc)
    {
        QString filepath = m_doc->fileName();
        QDesktopServices::openUrl(QUrl(QFileInfo(filepath).absolutePath()));
    }
}

// Encrypt the current document
void
dodo::EncryptDocument() noexcept
{
    if (m_doc)
    {
        Model::EncryptInfo encryptInfo;
        bool ok;
        QString password = QInputDialog::getText(
            this, "Encrypt Document", "Enter password:", QLineEdit::Password,
            QString(), &ok);
        if (!ok || password.isEmpty())
            return;
        encryptInfo.user_password = password;
        if (m_doc->model()->encrypt(encryptInfo))
            m_message_bar->showMessage("Document encrypted successfully");
        else
            m_message_bar->showMessage("Failed to encrypt document");
    }
}

void
dodo::DecryptDocument() noexcept
{
    if (m_doc)
    {
        if (m_doc->model()->passwordRequired())
        {
            bool ok;
            QString pwd = QInputDialog::getText(
                this, "Decrypt Document",
                "Enter password:", QLineEdit::Password, QString(), &ok);
            if (!ok || pwd.isEmpty())
                return;
            if (m_doc->model()->authenticate(pwd))
            {
                // Password correct, proceed to decryption
                if (m_doc->model()->decrypt())
                    m_message_bar->showMessage(
                        "Document decrypted successfully");
                else
                    m_message_bar->showMessage("Failed to decrypt document");
            }
            else
            {
                QMessageBox::critical(this, "Password",
                                      "Password is incorrect");
                return;
            }
        }
        else
        {
            m_message_bar->showMessage("Document is not encrypted");
        }
    }
}

// Update selection mode actions (QAction) in QMenu based on current selection
// mode
void
dodo::updateSelectionModeActions() noexcept
{
    if (!m_doc)
        return;

    switch (m_doc->selectionMode())
    {
        case GraphicsView::Mode::RegionSelection:
            m_actionRegionSelect->setChecked(true);
            break;
        case GraphicsView::Mode::TextSelection:
            m_actionTextSelect->setChecked(true);
            break;
        case GraphicsView::Mode::TextHighlight:
            m_actionTextHighlight->setChecked(true);
            break;
        case GraphicsView::Mode::AnnotSelect:
            m_actionAnnotEdit->setChecked(true);
            break;
        case GraphicsView::Mode::AnnotRect:
            m_actionAnnotRect->setChecked(true);
            break;
        default:
            break;
    }
}

void
dodo::updateGUIFromConfig() noexcept
{
    m_tab_widget->setTabsClosable(m_config.ui.tabs_closable);
    m_tab_widget->setMovable(m_config.ui.tabs_movable);

    if (m_config.ui.tab_bar_position == "top")
        m_tab_widget->setTabPosition(QTabWidget::North);
    else if (m_config.ui.tab_bar_position == "bottom")
        m_tab_widget->setTabPosition(QTabWidget::South);
    else if (m_config.ui.tab_bar_position == "left")
        m_tab_widget->setTabPosition(QTabWidget::West);
    else if (m_config.ui.tab_bar_position == "right")
        m_tab_widget->setTabPosition(QTabWidget::East);

    m_outline_widget->setVisible(m_config.ui.outline_shown);

    m_layout->addWidget(m_message_bar);
    m_layout->addWidget(m_panel);

    m_tab_widget->setTabBarAutoHide(m_config.ui.auto_hide_tabs);
    m_panel->setVisible(m_config.ui.panel_shown);
    m_menuBar->setVisible(m_config.ui.menubar_shown);
    m_tab_widget->tabBar()->setVisible(m_config.ui.tabs_shown);
}

void
dodo::ToggleFocusMode() noexcept
{
    if (!m_doc)
        return;

    setFocusMode(!m_focus_mode);
}

void
dodo::setFocusMode(bool enable) noexcept
{
    m_focus_mode = enable;

    if (m_focus_mode)
    {
        m_menuBar->setVisible(false);
        m_panel->setVisible(false);
        m_tab_widget->tabBar()->setVisible(false);
    }
    else
    {
        m_menuBar->setVisible(m_config.ui.menubar_shown);
        m_panel->setVisible(m_config.ui.panel_shown);
        updateTabbarVisibility();
    }
}

void
dodo::updateTabbarVisibility() noexcept
{
    // Let tab widget manage visibility itself based on auto-hide property
    m_tab_widget->tabBar()->setVisible(true); // initially show
    if (m_tab_widget->tabBarAutoHide() && m_tab_widget->count() < 2)
        m_tab_widget->tabBar()->setVisible(false);
}
