#include "dodo.hpp"

#include "AboutDialog.hpp"
#include "DocumentView.hpp"
#include "EditLastPagesWidget.hpp"
#include "StartupWidget.hpp"
#include "toml.hpp"

#include <QColorDialog>
#include <QFile>
#include <QJsonArray>
#include <QSplitter>
#include <QStackedLayout>
#include <QStyleHints>
#include <qstackedlayout.h>

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
    initActionMap();
    initConfig();
    initGui();
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
    m_actionFileProperties
        = fileMenu->addAction(QString("File Properties\t%1")
                                  .arg(m_config.shortcuts["file_properties"]),
                              this, &dodo::FileProperties);

    m_actionOpenContainingFolder = fileMenu->addAction(
        QString("Open Containing Folder\t%1")
            .arg(m_config.shortcuts["open_containing_folder"]),
        this, &dodo::OpenContainingFolder);

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

    m_recentFilesMenu = fileMenu->addMenu("Recent Files");
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, &QMainWindow::close);

    QMenu *editMenu = m_menuBar->addMenu("&Edit");
    m_actionUndo    = editMenu->addAction(
        QString("Undo\t%1").arg(m_config.shortcuts["undo"]), this, &dodo::Undo);
    m_actionRedo = editMenu->addAction(
        QString("Redo\t%1").arg(m_config.shortcuts["redo"]), this, &dodo::Redo);
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

    m_actionToggleOutline = viewMenu->addAction(
        QString("Outline\t%1").arg(m_config.shortcuts["outline"]), this,
        &dodo::ShowOutline);
    m_actionToggleOutline->setCheckable(true);
    m_actionToggleOutline->setChecked(!m_outline_widget->isHidden());

    m_actionToggleMenubar = viewMenu->addAction(
        QString("Menubar\t%1").arg(m_config.shortcuts["toggle_menubar"]), this,
        &dodo::ToggleMenubar);
    m_actionToggleMenubar->setCheckable(true);
    m_actionToggleMenubar->setChecked(!m_menuBar->isHidden());

    m_actionToggleTabBar = viewMenu->addAction(
        QString("Tabs\t%1").arg(m_config.shortcuts["toggle_tabs"]), this,
        &dodo::ToggleTabBar);
    m_actionToggleTabBar->setCheckable(true);
    m_actionToggleTabBar->setChecked(!m_tab_widget->tabBar()->isHidden());

    m_actionTogglePanel = viewMenu->addAction(
        QString("Panel\t%1").arg(m_config.shortcuts["toggle_panel"]), this,
        &dodo::TogglePanel);
    m_actionTogglePanel->setCheckable(true);
    m_actionTogglePanel->setChecked(!m_panel->isHidden());

    m_actionInvertColor = viewMenu->addAction(
        QString("Invert Color\t%1").arg(m_config.shortcuts["invert_color"]),
        this, &dodo::InvertColor);
    m_actionInvertColor->setCheckable(true);
    m_actionInvertColor->setChecked(m_config.behavior.invert_mode);

    QMenu *toolsMenu = m_menuBar->addMenu("Tools");

    QActionGroup *selectionActionGroup = new QActionGroup(this);
    selectionActionGroup->setExclusive(true);

    m_actionRegionSelect = toolsMenu->addAction(
        QString("Region Selection"), this, &dodo::RegionSelectionMode);
    m_actionRegionSelect->setCheckable(true);
    m_actionRegionSelect->setChecked(true);
    selectionActionGroup->addAction(m_actionRegionSelect);

    m_actionTextSelect = toolsMenu->addAction(QString("Text Selection"), this,
                                              &dodo::TextSelectionMode);
    m_actionTextSelect->setCheckable(true);
    m_actionTextSelect->setChecked(true);
    selectionActionGroup->addAction(m_actionTextSelect);

    m_actionTextHighlight = toolsMenu->addAction(
        QString("Text Highlight\t%1").arg(m_config.shortcuts["text_highlight"]),
        this, &dodo::ToggleTextHighlight);
    m_actionTextHighlight->setCheckable(true);
    selectionActionGroup->addAction(m_actionTextHighlight);

    m_actionAnnotRect = toolsMenu->addAction(
        QString("Annotate Rectangle\t%1").arg(m_config.shortcuts["annot_rect"]),
        this, &dodo::ToggleRectAnnotation);
    m_actionAnnotRect->setCheckable(true);
    selectionActionGroup->addAction(m_actionAnnotRect);

    m_actionAnnotEdit = toolsMenu->addAction(
        QString("Edit Annotations\t%1").arg(m_config.shortcuts["annot_edit"]),
        this, &dodo::ToggleAnnotSelect);
    m_actionAnnotEdit->setCheckable(true);
    selectionActionGroup->addAction(m_actionAnnotEdit);

    // --- Navigation Menu ---
    m_navMenu = m_menuBar->addMenu("&Navigation");

    m_navMenu->addAction(
        QString("StartPage").arg(m_config.shortcuts["startpage"]), this,
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
    m_config.ui.window_title_format    = "%1 - dodo";
    m_config.ui.link_hint_size         = 16.0f;

    m_config.ui.colors["search_index"] = "#3daee944";
    m_config.ui.colors["search_match"] = "#55FF8844";
    m_config.ui.colors["accent"]       = "#FF500044";
    m_config.ui.colors["background"]   = "#00000000";
    m_config.ui.colors["link_hint_fg"] = "#000000";
    m_config.ui.colors["link_hint_bg"] = "#FFFF00";
    m_config.ui.colors["highlight"]    = "#55FFFF00";
    m_config.ui.colors["selection"]    = "#55000055";
    m_config.ui.colors["jump_marker"]  = "#FFFF0000";
    m_config.ui.colors["annot_rect"]   = "#55FF0000";

    m_config.rendering.dpi        = 300.0f;
    m_config.rendering.dpr        = 1.0f;
    m_config.behavior.cache_pages = 10;

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

    m_session_dir = QDir(m_config_dir.filePath("sessions"));

    qDebug() << "Using config file:" << m_config_file_path;
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

    m_config.ui.startup_tab    = ui["startup_tab"].value_or(true);
    m_config.ui.auto_hide_tabs = ui["auto_hide_tabs"].value_or(false);
    m_config.ui.panel_shown    = ui["panel"].value_or(true);
    m_config.ui.outline_shown  = ui["outline"].value_or(false);
    m_config.ui.menubar_shown  = ui["menubar"].value_or(true);
    m_config.ui.tabs_shown     = ui["tabs"].value_or(true);

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

    auto rendering         = toml["rendering"];
    m_config.rendering.dpi = rendering["dpi"].value_or(300.0f);
    m_config.rendering.dpr = rendering["dpr"].value_or(
        QApplication::primaryScreen()->devicePixelRatio());
    m_config.behavior.cache_pages = rendering["cache_pages"].value_or(10);

    auto behavior = toml["behavior"];
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
    m_config.ui.initial_fit = behavior["initial_fit"].value_or("width");
    // m_config.auto_reload       = behavior["auto_reload"].value_or(true);
    m_config.behavior.recent_files = behavior["recent_files"].value_or(true);
    m_config.behavior.num_recent_files
        = behavior["num_recent_files"].value_or(10);

    // if (m_config.auto_reload)
    // {
    //     // TODO:
    // }

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
    m_config.shortcuts["undo"]            = "u";
    m_config.shortcuts["redo"]            = "Ctrl+r";
    m_config.shortcuts["toggle_menubar"]  = "Ctrl+Shift+m";
    m_config.shortcuts["invert_color"]    = "b";
    m_config.shortcuts["link_hint_visit"] = "f";
    m_config.shortcuts["save"]            = "Ctrl+s";
    m_config.shortcuts["text_highlight"]  = "Alt+1";
    m_config.shortcuts["annot_rect"]      = "Alt+2";
    m_config.shortcuts["annot_edit"]      = "Alt+3";
    m_config.shortcuts["outline"]         = "t";
    m_config.shortcuts["search"]          = "/";
    m_config.shortcuts["search_next"]     = "n";
    m_config.shortcuts["search_prev"]     = "Shift+n";
    m_config.shortcuts["zoom_in"]         = "+";
    m_config.shortcuts["zoom_out"]        = "-";
    m_config.shortcuts["zoom_reset"]      = "0";
    m_config.shortcuts["prev_location"]   = "Ctrl+o";
    m_config.shortcuts["open"]            = "o";
    m_config.shortcuts["scroll_left"]     = "h";
    m_config.shortcuts["scroll_down"]     = "j";
    m_config.shortcuts["scroll_up"]       = "k";
    m_config.shortcuts["scroll_right"]    = "l";
    m_config.shortcuts["next_page"]       = "Shift+j";
    m_config.shortcuts["prev_page"]       = "Shift+k";
    m_config.shortcuts["first_page"]      = "g,g";
    m_config.shortcuts["last_page"]       = "Shift+g";

    std::vector<std::pair<QString, std::function<void()>>> shortcuts = {
        {":", [this]() { invokeCommand(); }},
        {"Ctrl+Shift+m", [this]() { ToggleMenubar(); }},
        {"i", [this]() { InvertColor(); }},
        {"b", [this]() { GotoPage(); }},
        {"f", [this]() { VisitLinkKB(); }},
        {"Ctrl+s", [this]() { SaveFile(); }},
        {"Alt+1", [this]() { ToggleTextHighlight(); }},
        {"Alt+2", [this]() { ToggleRectAnnotation(); }},
        {"Alt+3", [this]() { ToggleAnnotSelect(); }},
        {"t", [this]() { ShowOutline(); }},
        {"/", [this]() { invokeSearch(); }},
        {"n", [this]() { NextHit(); }},
        {"Shift+n", [this]() { PrevHit(); }},
        {"Ctrl+o", [this]() { GoBackHistory(); }},
        {"o", [this]() { OpenFile(); }},
        {"j", [this]() { ScrollDown(); }},
        {"k", [this]() { ScrollUp(); }},
        {"h", [this]() { ScrollLeft(); }},
        {"l", [this]() { ScrollRight(); }},
        {"Shift+j", [this]() { NextPage(); }},
        {"Shift+k", [this]() { PrevPage(); }},
        {"g,g", [this]() { FirstPage(); }},
        {"Shift+g", [this]() { LastPage(); }},
        {"0", [this]() { ZoomReset(); }},
        {"=", [this]() { ZoomIn(); }},
        {"-", [this]() { ZoomOut(); }},
        {"<", [this]() { RotateAnticlock(); }},
        {">", [this]() { RotateClock(); }},
    };

    for (const auto &[key, func] : shortcuts)
    {
        auto *sc = new QShortcut(QKeySequence(key), this);
        connect(sc, &QShortcut::activated, func);
    }
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

    m_command_bar = new CommandBar(this);
    m_command_bar->setVisible(false);

    m_message_bar = new MessageBar(this);
    m_message_bar->setVisible(false);

    m_outline_widget = new OutlineWidget(this);
    m_outline_widget->setVisible(m_config.ui.outline_shown);

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

    widget->setLayout(m_layout);

    m_tab_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

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

    m_layout->addWidget(m_command_bar);
    m_layout->addWidget(m_message_bar);
    m_layout->addWidget(m_panel);
    this->setCentralWidget(widget);

    m_menuBar = this->menuBar(); // initialize here so that the config
                                 // visibility works

    m_tab_widget->setTabBarAutoHide(m_config.ui.auto_hide_tabs);
    m_panel->setVisible(m_config.ui.panel_shown);
    m_menuBar->setVisible(m_config.ui.menubar_shown);
    m_tab_widget->tabBar()->setVisible(m_config.ui.tabs_shown);

    // Remove padding and shit
    // m_tab_widget->tabBar()->setContentsMargins(0, 0, 0, 0);
    // m_layout->setSpacing(0);
    // m_layout->setContentsMargins(0, 0, 0, 0);
    // m_command_bar->setContentsMargins(0, 0, 0, 0);
    // m_panel->layout()->setContentsMargins(5, 1, 5, 1);
    // m_panel->setContentsMargins(0, 0, 0, 0);
    // this->setContentsMargins(0, 0, 0, 0);
}

// Updates the UI elements checking if valid
// file is open or not
void
dodo::updateUiEnabledState() noexcept
{
    const bool hasOpenedFile = m_doc ? true : false;

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
    m_actionUndo->setEnabled(hasOpenedFile);
    m_actionRedo->setEnabled(hasOpenedFile);
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

    if (m_config.ui.startup_tab && m_tab_widget->count() == 0)
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
            "`remember_last_visited` option is turned off in the config file");
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
}

// Rotates the file in anticlockwise direction
void
dodo::RotateAnticlock() noexcept
{
    // TODO:
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

void
dodo::OpenFiles(const std::vector<std::string> &files) noexcept
{
    // QString working_dir = QDir::currentPath();
    for (const std::string &s : files)
        OpenFile(QString::fromStdString(s));
}

void
dodo::OpenFiles(const QStringList &files) noexcept
{
    for (const QString &file : files)
        OpenFile(file);
}

bool
dodo::OpenFile(DocumentView *view) noexcept
{
    initTabConnections(view);
    view->setDPR(m_dpr);
    QString fileName = view->fileName();
    QString path     = QFileInfo(fileName).fileName();
    m_tab_widget->addTab(view, path);

    // Switch to already opened filepath, if it's open.
    if (m_path_tab_map.contains(fileName))
    {
        int existingIndex = m_tab_widget->indexOf(m_path_tab_map[fileName]);
        if (existingIndex != -1)
        {
            m_tab_widget->setCurrentIndex(existingIndex);
            return true;
        }
    }

    return false;
}

bool
dodo::OpenFile(QString filePath) noexcept
{
    if (filePath.isEmpty())
    {
        filePath = QFileDialog::getOpenFileName(
            this, "Open File", "", "PDF Files (*.pdf);; All Files (*)");
        if (filePath.isEmpty())
            return false;
    }

    // Switch to already opened filepath, if it's open.
    if (m_path_tab_map.contains(filePath))
    {
        int existingIndex = m_tab_widget->indexOf(m_path_tab_map[filePath]);
        if (existingIndex != -1)
        {
            m_tab_widget->setCurrentIndex(existingIndex);
            return true;
        }
    }

    filePath = filePath.replace("~", getenv("HOME"));

    if (!QFile::exists(filePath))
    {
        QMessageBox::warning(this, "Open File",
                             QString("Unable to find %1").arg(filePath));
        return false;
    }

    DocumentView *docwidget
        = new DocumentView(filePath, m_config, m_tab_widget);

    docwidget->setDPR(m_dpr);
    initTabConnections(docwidget);
    int index = m_tab_widget->addTab(docwidget, filePath);
    m_tab_widget->setCurrentIndex(index);

    m_outline_widget->setOutline(m_doc ? m_doc->model()->getOutline()
                                       : nullptr);

    m_path_tab_map[filePath] = docwidget;

    // TODO: Auto file reload
    // if (m_config.auto_reload)
    // {
    //     m_fs_watcher->addPath(filePath);
    // }
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

void
dodo::SaveFile() noexcept
{
    if (m_doc)
        m_doc->SaveFile();
}

void
dodo::SaveAsFile() noexcept
{
    if (m_doc)
        m_doc->SaveAsFile();
}

void
dodo::CloseFile() noexcept
{
    m_tab_widget->tabCloseRequested(m_tab_widget->currentIndex());
}

void
dodo::FitWidth() noexcept
{
    if (m_doc)
        m_doc->FitWidth();
}

void
dodo::FitHeight() noexcept
{
    if (m_doc)
        m_doc->FitHeight();
}

void
dodo::FitWindow() noexcept
{
    if (m_doc)
        m_doc->FitWindow();
}

void
dodo::ToggleAutoResize() noexcept
{
    if (m_doc)
        m_doc->ToggleAutoResize();
}

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
    qDebug() << "Outline shown: " << m_outline_widget->isVisible();
}

void
dodo::InvertColor() noexcept
{
    if (m_doc)
    {
        m_doc->InvertColor();
        m_actionInvertColor->setChecked(!m_actionInvertColor->isChecked());
    }
}

void
dodo::ToggleTextHighlight() noexcept
{
    if (m_doc)
        m_doc->ToggleTextHighlight();
}

void
dodo::ToggleRectAnnotation() noexcept
{
    if (m_doc)
        m_doc->ToggleRectAnnotation();
}

void
dodo::ToggleAnnotSelect() noexcept
{
    if (m_doc)
        m_doc->ToggleAnnotSelect();
}

void
dodo::FirstPage() noexcept
{
    if (m_doc)
        m_doc->FirstPage();
}

void
dodo::PrevPage() noexcept
{
    if (m_doc)
        m_doc->PrevPage();
}

void
dodo::NextPage() noexcept
{
    if (m_doc)
        m_doc->NextPage();
}

void
dodo::LastPage() noexcept
{
    if (m_doc)
        m_doc->LastPage();

    updatePageNavigationActions();
}

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

    QList<QScreen *> outputs = QGuiApplication::screens();
    connect(m_tab_widget, &QTabWidget::currentChanged, this,
            &dodo::handleCurrentTabChanged);
    // m_dpr = m_tab_widget->window()->devicePixelRatioF();
    m_dpr = m_config.rendering.dpr;

    QWindow *win = window()->windowHandle();

    // TODO: Check for multiple screen support
    connect(win, &QWindow::screenChanged, this, [&](QScreen *)
    {
        m_dpr = this->devicePixelRatioF();
        if (m_doc)
            m_doc->setDPR(m_dpr);
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

                // Set the outline to nullptr if the closed tab was the current
                // one
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

    connect(m_command_bar, &CommandBar::processCommand, this,
            &dodo::processCommand);

    connect(m_actionInvertColor, &QAction::triggered, [&]() { InvertColor(); });

    connect(m_navMenu, &QMenu::aboutToShow, this,
            &dodo::updatePageNavigationActions);

    connect(m_outline_widget, &OutlineWidget::jumpToPageRequested, this,
            &dodo::gotoPage);
}

void
dodo::handleFileNameChanged(const QString &name) noexcept
{
    m_panel->setFileName(name);
    this->setWindowTitle(name);
}

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

    m_doc = qobject_cast<DocumentView *>(widget);

    updateMenuActions();
    updateUiEnabledState();
    updatePanel();

    m_outline_widget->setOutline(m_doc ? m_doc->model()->getOutline()
                                       : nullptr);

    this->setWindowTitle(m_doc->windowTitle());
}

void
dodo::closeEvent(QCloseEvent *e)
{
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

            doc->CloseFile();
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

bool
dodo::eventFilter(QObject *object, QEvent *event)
{
    QEvent::Type type = event->type();
    if (m_link_hint_mode)
    {
        if (type == QEvent::KeyPress)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape)
            {
                m_currentHintInput.clear();
                m_doc->clearKBHintsOverlay();
                m_link_hint_map.clear();
                m_link_hint_mode = false;
                // this->removeEventFilter(this);
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
            int num = m_currentHintInput.toInt();
            if (m_link_hint_map.contains(num))
            {
                Model::LinkInfo info = m_link_hint_map[num];

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

        if (type == QEvent::ShortcutOverride)
        {
            event->accept();
            return true;
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

    connect(docwidget, &DocumentView::fitModeChanged, m_panel,
            &Panel::setFitMode);

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
    if (m_last_pages_db.isValid() && m_last_pages_db.isOpen())
    {
        QSqlQuery q(m_last_pages_db);
        q.prepare("UPDATE last_visited SET page_number = ?, last_accessed = ? "
                  "WHERE file_path = ?");
        q.bindValue(0, pageno);
        q.bindValue(1, QDateTime::currentDateTime());
        q.bindValue(2, fname);
        q.exec();

        if (!q.exec() || q.numRowsAffected() == 0)
        {
            q.prepare("INSERT INTO last_visited (file_path, page_number, "
                      "last_accessed) VALUES (?, ?, ?)");
            q.bindValue(0, fname);
            q.bindValue(1, pageno);
            q.bindValue(2, QDateTime::currentDateTime());
            q.exec();
        }
    }
}

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
        m_panel->setFitMode(m_doc->fitMode());
    }
    else
    {
        m_panel->hidePageInfo(true);
        m_panel->setFileName("");
        m_panel->setHighlightColor("");
        // m_panel->setMode();
        // m_panel->setFitMode(m_doc->fitMode());
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
        qDebug() << "Could not open session file!";
        QMessageBox::critical(this, "Open Session",
                              "Could not open session: " + sessionName);
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
        QMessageBox::critical(this, "Save Session",
                              "Could not save session: " + m_session_name);
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

void
dodo::updateActionsAndStuffForSystemTabs() noexcept
{
    m_panel->hidePageInfo(true);
    updateUiEnabledState();
    m_panel->setFileName("Startup");
}

// Invokes the `CommandBar` with command mode
void
dodo::invokeCommand() noexcept
{
    m_command_bar->setVisible(true);
    m_command_bar->clear();
    m_command_bar->insert(":");
    m_command_bar->setFocus();
}

// Invokes the `CommandBar` with search mode
void
dodo::invokeSearch() noexcept
{
    m_command_bar->setVisible(true);
    m_command_bar->clear();
    m_command_bar->insert("/");
    m_command_bar->setFocus();
}

// Process the command sent by `CommandBar`
void
dodo::processCommand(const QString &cmd) noexcept
{
    if (cmd.isEmpty())
        return;

    QString trimmed = cmd.mid(1).trimmed();

    if (cmd.startsWith(":"))
    {
        // If number, goto page
        bool ok;
        int pageno = trimmed.toInt(&ok);

        if (ok)
        {
            gotoPage(pageno);
            return;
        }

        QStringList parts
            = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.isEmpty())
            return;

        QString command = parts[0];

        if (m_actionMap.contains(command))
        {
            QStringList args = parts.mid(1);
            m_actionMap[command](args);
        }
        else
        {
            m_message_bar->showMessage(
                QString("%1 command doesn't exist").arg(command));
        }
    }

    else if (cmd.startsWith("/"))
    {
        Search(trimmed);
    }
}

// Undo operation
void
dodo::Undo() noexcept
{
    if (m_doc && m_doc->model())
    {
        if (m_doc->model()->undoStack()->canUndo())
        {
            m_doc->model()->undoStack()->undo();
            // TODO: Fix this function!!
        }
    }
}

// Redo operation
void
dodo::Redo() noexcept
{
    if (m_doc && m_doc->model())
        m_doc->model()->undoStack()->redo();
}

// Initialize the actions with corresponding functions
// to call
void
dodo::initActionMap() noexcept
{
    m_actionMap = {
        {"setdpr",
         [this](const QStringList &args)
    {
        if (args.isEmpty())
            return;
        bool ok;
        float dpr = args.at(0).toFloat(&ok);
        if (ok)
            setDPR(dpr);
        else
            m_message_bar->showMessage("Invalid DPR");
    }},
        {"tabgoto",
         [this](const QStringList &args)
    {
        if (args.isEmpty())
            return;
        bool ok;
        int index = args.at(0).toInt(&ok);
        if (ok)
            GotoTab(index);
        else
            m_message_bar->showMessage("Invalid tab index");
    }},
        {"open_containing_folder",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        OpenContainingFolder();
    }},
        {"tabnext",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        NextTab();
    }},
        {"tabprev",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        PrevTab();
    }},
        {"tabclose",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        CloseTab();
    }},
        {"reload",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        reloadDocument();
    }},
        {"command",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        invokeCommand();
    }},
        {"undo",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        Undo();
    }},
        {"redo",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        Redo();
    }},
        {"text_highlight_current_selection",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        TextHighlightCurrentSelection();
    }},
        {"toggle_tabs",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ToggleTabBar();
    }},
        {"keybindings",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ShowKeybindings();
    }},
        {"yank_all",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        YankAll();
    }},
        {"save",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        SaveFile();
    }},
        {"save_as",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        SaveAsFile();
    }},
        {"yank",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        YankSelection();
    }},
        {"cancel_selection",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ClearTextSelection();
    }},
        {"about",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ShowAbout();
    }},
        {"link_hint_visit",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        VisitLinkKB();
    }},
        {"link_hint_copy",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        CopyLinkKB();
    }},
        {"outline",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ShowOutline();
    }},
        {"rotate_clock",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        RotateClock();
    }},
        {"rotate_anticlock",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        RotateAnticlock();
    }},
        {"prev_location",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        GoBackHistory();
    }},
        {"scroll_down",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ScrollDown();
    }},
        {"scroll_up",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ScrollUp();
    }},
        {"scroll_left",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ScrollLeft();
    }},
        {"scroll_right",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ScrollRight();
    }},
        {"invert_color",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        InvertColor();
    }},
        {"search",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        invokeSearch();
    }},
        {"search_next",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        NextHit();
    }},
        {"search_prev",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        PrevHit();
    }},
        {"next_page",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        NextPage();
    }},
        {"prev_page",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        PrevPage();
    }},
        {"goto_page",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        GotoPage();
    }},
        {"first_page",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        FirstPage();
    }},
        {"last_page",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        LastPage();
    }},
        {"zoom_in",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ZoomIn();
    }},
        {"zoom_out",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ZoomOut();
    }},
        {"zoom_reset",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ZoomReset();
    }},
        {"annot_edit",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ToggleAnnotSelect();
    }},
        {"text_highlight",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ToggleTextHighlight();
    }},
        {"annot_rect",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ToggleRectAnnotation();
    }},
        {"fullscreen",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ToggleFullscreen();
    }},
        {"file_properties",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        FileProperties();
    }},
        {"open_file",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        OpenFile();
    }},
        {"close_file",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        CloseFile();
    }},
        {"fit_width",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        FitWidth();
    }},
        {"fit_height",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        FitHeight();
    }},
        {"fit_window",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        FitWindow();
    }},
        {"auto_resize",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ToggleAutoResize();
    }},
        {"toggle_menubar",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        ToggleMenubar();
    }},
        {"toggle_panel",
         [this](const QStringList &args)
    {
        Q_UNUSED(args);
        TogglePanel();
    }},

    };
}

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
    {
        m_doc->reloadDocument();
    }
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
    if (tabno > 0 || tabno < m_tab_widget->count())
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

void
dodo::OpenContainingFolder() noexcept
{
    if (m_doc)
    {
        QString filepath = m_doc->fileName();
        QDesktopServices::openUrl(QUrl(QFileInfo(filepath).absolutePath()));
    }
}
