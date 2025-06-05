#include "dodo.hpp"

#include "AboutDialog.hpp"
#include "DocumentView.hpp"
#include "EditLastPagesWidget.hpp"
#include "toml.hpp"

#include <QJsonArray>
#include <QStyleHints>
#include <qdesktopservices.h>
#include <qnamespace.h>

dodo::dodo() noexcept
{
    setAttribute(Qt::WA_NativeWindow); // This is necessary for DPI updates
}

dodo::~dodo() noexcept
{
    m_last_pages_db.close();
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
    populateRecentFiles();
    initConnections();
    setMinimumSize(600, 400);
    this->show();
}

void
dodo::initMenubar() noexcept
{

    // --- File Menu ---
    QMenu *fileMenu = m_menuBar->addMenu("&File");
    fileMenu->addAction(QString("Open File\t%1").arg(m_config.shortcuts_map["open_file"]), this, [&]() { OpenFile(); });
    m_actionFileProperties = fileMenu->addAction(
        QString("File Properties\t%1").arg(m_config.shortcuts_map["file_properties"]), this, &dodo::FileProperties);

    m_actionSaveFile =
        fileMenu->addAction(QString("Save File\t%1").arg(m_config.shortcuts_map["save"]), this, &dodo::SaveFile);

    m_actionSaveAsFile = fileMenu->addAction(QString("Save As File\t%1").arg(m_config.shortcuts_map["save_as"]), this,
                                             &dodo::SaveAsFile);

    QMenu *sessionMenu = fileMenu->addMenu("Session");

    sessionMenu->addAction("Save", this, [&]() { SaveSession(); });
    sessionMenu->addAction("Save As", this, [&]() { SaveAsSession(); });
    sessionMenu->addAction("Load", this, [&]() { LoadSession(); });

    m_actionCloseFile = fileMenu->addAction(QString("Close File\t%1").arg(m_config.shortcuts_map["close_file"]), this,
                                            &dodo::CloseFile);

    m_recentFilesMenu = fileMenu->addMenu("Recent Files");
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, &QMainWindow::close);

    QMenu *editMenu = m_menuBar->addMenu("&Edit");
    editMenu->addAction(QString("Last Pages\t%1").arg(m_config.shortcuts_map["edit_last_pages"]), this,
                        &dodo::editLastPages);

    // --- View Menu ---
    QMenu *viewMenu    = m_menuBar->addMenu("&View");
    m_actionFullscreen = viewMenu->addAction(QString("Fullscreen\t%1").arg(m_config.shortcuts_map["fullscreen"]), this,
                                             &dodo::ToggleFullscreen);
    m_actionFullscreen->setCheckable(true);

    m_actionZoomIn =
        viewMenu->addAction(QString("Zoom In\t%1").arg(m_config.shortcuts_map["zoom_in"]), this, &dodo::ZoomIn);
    m_actionZoomOut =
        viewMenu->addAction(QString("Zoom Out\t%1").arg(m_config.shortcuts_map["zoom_out"]), this, &dodo::ZoomOut);

    viewMenu->addSeparator();

    m_fitMenu = viewMenu->addMenu("Fit");

    m_actionFitNone =
        m_fitMenu->addAction(QString("None\t%1").arg(m_config.shortcuts_map["fit_none"]), this, &dodo::FitNone);

    m_actionFitWidth =
        m_fitMenu->addAction(QString("Width\t%1").arg(m_config.shortcuts_map["fit_width"]), this, &dodo::FitWidth);

    m_actionFitHeight =
        m_fitMenu->addAction(QString("Height\t%1").arg(m_config.shortcuts_map["fit_height"]), this, &dodo::FitHeight);

    m_actionFitWindow =
        m_fitMenu->addAction(QString("Window\t%1").arg(m_config.shortcuts_map["fit_window"]), this, &dodo::FitWindow);

    m_fitMenu->addSeparator();

    // Auto Resize toggle (independent)
    m_actionAutoresize = m_fitMenu->addAction(QString("Auto Resize\t%1").arg(m_config.shortcuts_map["auto_resize"]),
                                              this, &dodo::ToggleAutoResize);
    m_actionAutoresize->setCheckable(true);
    m_actionAutoresize->setChecked(m_config.auto_resize); // default on or off

    viewMenu->addSeparator();

    m_actionToggleOutline = viewMenu->addAction(QString("Outline\t%1").arg(m_config.shortcuts_map["outline"]), this,
                                                &dodo::TableOfContents);

    m_actionToggleMenubar = viewMenu->addAction(QString("Menubar\t%1").arg(m_config.shortcuts_map["toggle_menubar"]),
                                                this, &dodo::ToggleMenubar);
    m_actionToggleMenubar->setCheckable(true);
    m_actionToggleMenubar->setChecked(!m_menuBar->isHidden());

    m_actionToggleTabBar =
        viewMenu->addAction(QString("Tabs\t%1").arg(m_config.shortcuts_map["toggle_tabs"]), this, &dodo::ToggleTabBar);
    m_actionToggleTabBar->setCheckable(true);
    m_actionToggleTabBar->setChecked(!m_tab_widget->tabBar()->isHidden());

    m_actionTogglePanel =
        viewMenu->addAction(QString("Panel\t%1").arg(m_config.shortcuts_map["toggle_panel"]), this, &dodo::TogglePanel);
    m_actionTogglePanel->setCheckable(true);
    m_actionTogglePanel->setChecked(!m_panel->isHidden());

    m_actionInvertColor = viewMenu->addAction(QString("Invert Color\t%1").arg(m_config.shortcuts_map["invert_color"]),
                                              this, &dodo::InvertColor);
    m_actionInvertColor->setCheckable(true);
    m_actionInvertColor->setChecked(m_config.invert_mode);

    QMenu *toolsMenu = m_menuBar->addMenu("Tools");

    QActionGroup *selectionActionGroup = new QActionGroup(this);
    selectionActionGroup->setExclusive(true);

    m_actionTextSelect = toolsMenu->addAction(QString("Text Selection"), this, &dodo::TextSelectionMode);
    m_actionTextSelect->setCheckable(true);
    m_actionTextSelect->setChecked(true);
    selectionActionGroup->addAction(m_actionTextSelect);

    m_actionTextHighlight = toolsMenu->addAction(
        QString("Text Highlight\t%1").arg(m_config.shortcuts_map["text_highlight"]), this, &dodo::ToggleTextHighlight);
    m_actionTextHighlight->setCheckable(true);
    selectionActionGroup->addAction(m_actionTextHighlight);

    m_actionAnnotRect = toolsMenu->addAction(
        QString("Annotate Rectangle\t%1").arg(m_config.shortcuts_map["annot_rect"]), this, &dodo::ToggleRectAnnotation);
    m_actionAnnotRect->setCheckable(true);
    selectionActionGroup->addAction(m_actionAnnotRect);

    m_actionAnnotEdit = toolsMenu->addAction(QString("Edit Annotations\t%1").arg(m_config.shortcuts_map["annot_edit"]),
                                             this, &dodo::ToggleAnnotSelect);
    m_actionAnnotEdit->setCheckable(true);
    selectionActionGroup->addAction(m_actionAnnotEdit);

    // --- Navigation Menu ---
    QMenu *navMenu = m_menuBar->addMenu("&Navigation");
    m_actionFirstPage =
        navMenu->addAction(QString("First Page\t%1").arg(m_config.shortcuts_map["first_page"]), this, &dodo::FirstPage);

    m_actionPrevPage = navMenu->addAction(QString("Previous Page\t%1").arg(m_config.shortcuts_map["prev_page"]), this,
                                          &dodo::PrevPage);

    m_actionNextPage =
        navMenu->addAction(QString("Next Page\t%1").arg(m_config.shortcuts_map["next_page"]), this, &dodo::NextPage);
    m_actionLastPage =
        navMenu->addAction(QString("Last Page\t%1").arg(m_config.shortcuts_map["last_page"]), this, &dodo::LastPage);

    m_actionPrevLocation = navMenu->addAction(
        QString("Previous Location\t%1").arg(m_config.shortcuts_map["prev_location"]), this, &dodo::GoBackHistory);

    QMenu *helpMenu = m_menuBar->addMenu("&Help");
    m_actionAbout =
        helpMenu->addAction(QString("About\t%1").arg(m_config.shortcuts_map["about"]), this, &dodo::ShowAbout);
    helpMenu->addAction(QString("Keybindings\t%1").arg(m_config.shortcuts_map["keybindings"]), this,
                        &dodo::ShowKeybindings);

    updateUiEnabledState();
}

void
dodo::initDB() noexcept
{
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
    m_config.jump_marker_shown      = true;
    m_config.full_filepath_in_panel = false;
    m_config.zoom                   = 1.0f;
    m_config.window_title_format    = "%1 - dodo";

    m_config.colors["search_index"] = QColor::fromString("#3daee944").rgba();
    m_config.colors["search_match"] = QColor::fromString("#55FF8844").rgba();
    m_config.colors["accent"]       = QColor::fromString("#FF500044").rgba();
    m_config.colors["background"]   = Qt::transparent;
    m_config.colors["link_hint_fg"] = QColor::fromString("#000000").rgba();
    m_config.colors["link_hint_bg"] = QColor::fromString("#FFFF00").rgba();
    m_config.colors["highlight"]    = QColor::fromString("#55FFFF00").rgba();
    m_config.colors["selection"]    = QColor::fromString("#550000FF").rgba();
    m_config.colors["jump_marker"]  = QColor::fromString("#FFFF0000").rgba();
    m_config.colors["annot_rect"]   = QColor::fromString("#55FF0000").rgba();

    m_config.dpi         = 300.0f;
    m_config.cache_pages = 10;

    m_config.remember_last_visited = true;
    m_config.page_history_limit    = 10;
    m_config.auto_resize           = false;
    m_config.zoom_by               = 1.25;
    m_config.initial_fit           = "width";

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
    m_session_dir         = QDir(m_config_dir.filePath("sessions"));

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

    m_tab_widget->tabBar()->setVisible(ui["tabs"].value_or(true));
    m_tab_widget->setTabBarAutoHide(ui["auto_hide_tabs"].value_or(false));
    m_panel->setVisible(ui["panel"].value_or(true));
    m_menuBar->setVisible(ui["menubar"].value_or(true));

    if (ui["fullscreen"].value_or(false))
        this->showFullScreen();

    m_config.vscrollbar_shown         = ui["vscrollbar"].value_or(true);
    m_config.hscrollbar_shown         = ui["hscrollbar"].value_or(true);
    m_config.selection_drag_threshold = ui["selection_drag_threshold"].value_or(50);
    m_config.jump_marker_shown        = ui["jump_marker"].value_or(true);
    m_config.full_filepath_in_panel   = ui["full_file_path_in_panel"].value_or(false);
    m_config.zoom                     = ui["zoom_level"].value_or(1.0);
    m_config.compact                  = ui["compact"].value_or(false);
    m_config.link_boundary            = ui["link_boundary"].value_or(false);
    auto window_title                 = QString::fromStdString(ui["window_title"].value_or("{} - dodo"));
    window_title.replace("{}", "%1");
    m_config.window_title_format = window_title;

    auto colors = toml["colors"];

    m_config.colors["search_index"] = QColor::fromString(colors["search_index"].value_or("#3daee944")).rgba();
    m_config.colors["search_match"] = QColor::fromString(colors["search_match"].value_or("#FFFF8844")).rgba();
    m_config.colors["accent"]       = QColor::fromString(colors["accent"].value_or("#FF500044")).rgba();
    m_config.colors["background"]   = QColor::fromString(colors["background"].value_or("#FFFFFF")).rgba();
    m_config.colors["link_hint_fg"] = QColor::fromString(colors["link_hint_fg"].value_or("#000000")).rgba();
    m_config.colors["link_hint_bg"] = QColor::fromString(colors["link_hint_bg"].value_or("#FFFF00")).rgba();
    m_config.colors["highlight"]    = QColor::fromString(colors["highlight"].value_or("#55FFFF00")).rgba();
    m_config.colors["selection"]    = QColor::fromString(colors["selection"].value_or("#550000FF")).rgba();
    m_config.colors["jump_marker"]  = QColor::fromString(colors["jump_marker"].value_or("#FFFF0000")).rgba();

    auto rendering       = toml["rendering"];
    m_config.dpi         = rendering["dpi"].value_or(300.0);
    m_config.cache_pages = rendering["cache_pages"].value_or(10);

    auto behavior                   = toml["behavior"];
    m_config.remember_last_visited  = behavior["remember_last_visited"].value_or(true);
    m_config.open_last_visited      = behavior["open_last_visited"].value_or(false);
    m_config.page_history_limit     = behavior["page_history"].value_or(100);
    m_config.antialiasing_bits      = behavior["antialasing_bits"].value_or(8);
    m_config.auto_resize            = behavior["auto_resize"].value_or(false);
    m_config.zoom_by                = behavior["zoom_factor"].value_or(1.25);
    m_config.page_nav_with_mouse    = behavior["page_nav_with_mouse"].value_or(false);
    m_config.synctex_editor_command = QString::fromStdString(behavior["synctex_editor_command"].value_or(""));
    m_config.invert_mode            = behavior["invert_mode"].value_or(false);
    m_config.icc_color_profile      = behavior["icc_color_profile"].value_or(true);
    m_config.initial_fit            = behavior["initial_fit"].value_or("width");

    if (toml.contains("keybindings"))
    {
        m_load_default_keybinding = false;
        auto keys                 = toml["keybindings"];
        m_actionMap               = {
            {"text_highlight_current_selection",
                           [this]()
                      {
            TextHighlightCurrentSelection();
        }},
            {"toggle_tabs",
                           [this]()
                      {
            ToggleTabBar();
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
            ClearTextSelection();
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
            RotateAnticlock();
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
            NextHit();
        }},
            {"search_prev",
                           [this]()
                      {
            PrevHit();
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
            {"annot_edit",
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

    if (m_config.compact)
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_panel->layout()->setContentsMargins(5, 1, 5, 1);
        m_panel->setContentsMargins(0, 0, 0, 0);
        m_tab_widget->setContentsMargins(0, 0, 0, 0);
        this->setContentsMargins(0, 0, 0, 0);
    }
}

void
dodo::initKeybinds() noexcept
{
    m_config.shortcuts_map["toggle_menubar"]  = "Ctrl+Shift+m";
    m_config.shortcuts_map["invert_color"]    = "b";
    m_config.shortcuts_map["link_hint_visit"] = "f";
    m_config.shortcuts_map["save"]            = "Ctrl+s";
    m_config.shortcuts_map["text_highlight"]  = "Alt+1";
    m_config.shortcuts_map["annot_rect"]      = "Alt+2";
    m_config.shortcuts_map["annot_edit"]      = "Alt+3";
    m_config.shortcuts_map["outline"]         = "t";
    m_config.shortcuts_map["search"]          = "/";
    m_config.shortcuts_map["search_next"]     = "n";
    m_config.shortcuts_map["search_prev"]     = "Shift+n";
    m_config.shortcuts_map["zoom_in"]         = "+";
    m_config.shortcuts_map["zoom_out"]        = "-";
    m_config.shortcuts_map["zoom_reset"]      = "0";
    m_config.shortcuts_map["prev_location"]   = "Ctrl+o";
    m_config.shortcuts_map["open"]            = "o";
    m_config.shortcuts_map["scroll_left"]     = "h";
    m_config.shortcuts_map["scroll_down"]     = "j";
    m_config.shortcuts_map["scroll_up"]       = "k";
    m_config.shortcuts_map["scroll_right"]    = "l";
    m_config.shortcuts_map["next_page"]       = "Shift+j";
    m_config.shortcuts_map["prev_page"]       = "Shift+k";
    m_config.shortcuts_map["first_page"]      = "g,g";
    m_config.shortcuts_map["last_page"]       = "Shift+g";

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
        NextHit();
    }},
        {"Shift+n",
         [this]()
    {
        PrevHit();
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
        RotateAnticlock();
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
    m_layout->addWidget(m_tab_widget);
    m_layout->addWidget(m_panel);
    this->setCentralWidget(widget);

    // m_tab_widget->tabBar()->installEventFilter(this);
    this->installEventFilter(this);

    m_menuBar = this->menuBar(); // initialize here so that the config visibility works
}

void
dodo::updateUiEnabledState() noexcept
{
    const bool hasOpenedFile = m_doc ? true : false;

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
dodo::setupKeybinding(const QString &action, const QString &key) noexcept
{
    auto it = m_actionMap.find(action);
    if (it == m_actionMap.end())
        return;

    QShortcut *shortcut = new QShortcut(QKeySequence(key), this);
    connect(shortcut, &QShortcut::activated, this, [=]() { it.value()(); });

    m_config.shortcuts_map[action] = key;
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
    abw->setAppInfo(__DODO_VERSION, "A fast, unintrusive pdf reader");
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
        m_config.startpage_override = argparser.get<int>("--page");

    if (argparser.is_used("--synctex-forward"))
    {
        m_config.startpage_override = -1; // do not override the page

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
            OpenFile(pdfPath);
            // synctexLocateInPdf(texPath, line, column); TODO:
        }
        else
        {
            qWarning() << "Invalid --synctex-forward format. Expected file.pdf#file.tex:line:column";
        }
    }

    try
    {
        auto files = argparser.get<std::vector<std::string>>("files");
        if (!files.empty())
        {
            OpenFiles(files);
            m_config.open_last_visited = false;
        }

        if (m_config.open_last_visited)
            openLastVisitedFile();
    }
    catch (...)
    {
    }
    m_config.startpage_override = -1;
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
            QAction *fileAction = new QAction(path, m_recentFilesMenu);
            connect(fileAction, &QAction::triggered, this, [&, path, page]()
            {
                OpenFile(path);
                GotoPage(page);
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
    if (!m_config.remember_last_visited || !m_last_pages_db.isOpen() && !m_last_pages_db.isValid())
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

void
dodo::ShowKeybindings() noexcept
{
    ShortcutsWidget *widget = new ShortcutsWidget(m_config.shortcuts_map, this);
    widget->show();
}

void
dodo::openLastVisitedFile() noexcept
{
    if (!m_last_pages_db.isOpen() || !m_last_pages_db.isValid())
        return;

    QSqlQuery q;
    q.prepare("SELECT file_path, page_number FROM last_visited ORDER BY last_accessed DESC");

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
            GotoPage(pageno);
        }
    }
}

void
dodo::Search() noexcept
{
    if (m_doc)
        m_doc->Search();
}

void
dodo::ZoomOut() noexcept
{
    if (m_doc)
        m_doc->ZoomOut();
}

void
dodo::ZoomIn() noexcept
{
    if (m_doc)
        m_doc->ZoomIn();
}

void
dodo::ZoomReset() noexcept
{
    if (m_doc)
        m_doc->ZoomReset();
}

void
dodo::GotoPage(int pageno) noexcept
{
    if (m_doc)
        m_doc->GotoPage(pageno);
}

void
dodo::NextHit() noexcept
{
    if (m_doc)
        m_doc->NextHit();
}

void
dodo::PrevHit() noexcept
{
    if (m_doc)
        m_doc->PrevHit();
}

void
dodo::ScrollLeft() noexcept
{
    if (m_doc)
        m_doc->ScrollLeft();
}

void
dodo::ScrollRight() noexcept
{
    if (m_doc)
        m_doc->ScrollRight();
}

void
dodo::ScrollUp() noexcept
{
    if (m_doc)
        m_doc->ScrollUp();
}

void
dodo::ScrollDown() noexcept
{
    if (m_doc)
        m_doc->ScrollDown();
}

void
dodo::RotateClock() noexcept
{
    // TODO:
}

void
dodo::RotateAnticlock() noexcept
{
    // TODO:
}

void
dodo::VisitLinkKB() noexcept
{
    if (m_doc)
    {
        m_link_hint_map = m_doc->LinkKB();
        if (!m_link_hint_map.isEmpty())
        {
            m_link_hint_current_mode = LinkHintMode::Visit;
            m_link_hint_mode = true;
        }
    }
}

void
dodo::CopyLinkKB() noexcept
{
    if (m_doc)
    {
        m_link_hint_map = m_doc->LinkKB();
        if (!m_link_hint_map.isEmpty())
        {
            m_link_hint_current_mode = LinkHintMode::Visit;
            m_link_hint_mode = true;
        }
    }
}

void
dodo::ClearTextSelection() noexcept
{
    if (m_doc)
        m_doc->ClearTextSelection();
}

void
dodo::YankSelection() noexcept
{
    if (m_doc)
        m_doc->YankSelection();
}

void
dodo::SelectAll() noexcept
{
    if (m_doc)
        m_doc->SelectAll();
}

void
dodo::FitNone() noexcept
{}

void
dodo::TextSelectionMode() noexcept
{}

void
dodo::OpenFiles(const std::vector<std::string> &files) noexcept
{
    for (const auto &s : files)
        OpenFile(QString::fromStdString(s));
}

void
dodo::OpenFile(DocumentView *view) noexcept
{
    initTabConnections(view);
    view->setDPR(m_dpr);
    m_tab_widget->addTab(view, QFileInfo(view->fileName()).fileName());
}

void
dodo::OpenFile(QString filePath) noexcept
{
    if (filePath.isEmpty())
    {
        filePath = QFileDialog::getOpenFileName(this, "Open File", "", "PDF Files (*.pdf);; All Files (*)");
        if (filePath.isEmpty())
            return;
    }
    DocumentView *docwidget = new DocumentView(filePath, m_config, m_tab_widget);
    docwidget->setDPR(m_dpr);
    initTabConnections(docwidget);
    int index = m_tab_widget->addTab(docwidget, QFileInfo(filePath).fileName());
    m_tab_widget->setCurrentIndex(index);
}

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
dodo::TableOfContents() noexcept
{
    if (m_doc)
        m_doc->TableOfContents();
}

void
dodo::InvertColor() noexcept
{
    if (m_doc)
        m_doc->InvertColor();
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
}

void
dodo::GoBackHistory() noexcept
{
    if (m_doc)
        m_doc->GoBackHistory();
}

void
dodo::TextHighlightCurrentSelection() noexcept
{
    if (m_doc)
        m_doc->TextHighlightCurrentSelection();
}

void
dodo::resizeEvent(QResizeEvent *e)
{
    QMainWindow::resizeEvent(e);
}

void
dodo::initConnections() noexcept
{
    connect(m_tab_widget, &QTabWidget::currentChanged, this, &dodo::handleCurrentTabChanged);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Unset);
    m_dpr = m_tab_widget->window()->devicePixelRatioF();
    qDebug() << m_dpr;

    auto win = window()->windowHandle();
    if (win)
    {
        connect(win, &QWindow::screenChanged, this, [&](QScreen *)
        {
            m_dpr = this->devicePixelRatioF();
            if (m_doc)
                m_doc->setDPR(m_dpr);
        });
    }

    connect(m_tab_widget, &QTabWidget::tabCloseRequested, this, [this](int index)
    {
        QWidget *widget   = m_tab_widget->widget(index);
        DocumentView *doc = qobject_cast<DocumentView *>(widget);

        doc->close();

        if (doc)
            doc->CloseFile();

        if (widget)
            widget->deleteLater();

        m_tab_widget->removeTab(index);
    });

    void invertColorActionUpdate(bool state);
    void autoResizeActionUpdate(bool state);
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
        return;
    }

    m_doc = qobject_cast<DocumentView *>(m_tab_widget->widget(index));

    updateMenuActions();
    updateUiEnabledState();
    updatePanel();

    this->setWindowTitle(m_doc->windowTitle());
}

void
dodo::closeEvent(QCloseEvent *e)
{
    for (int i = 0; i < m_tab_widget->count(); i++)
    {
        DocumentView *doc = qobject_cast<DocumentView *>(m_tab_widget->widget(i));
        if (doc)
        {

            // Unsaved Changes
            if (doc->model()->hasUnsavedChanges())
            {
                int ret = QMessageBox::warning(
                    this, "Unsaved Changes",
                    QString("File %1 has unsaved changes. Do you want to save them?").arg(m_tab_widget->tabText(i)),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);

                if (ret == QMessageBox::Cancel)
                {
                    e->ignore();
                    return;
                }
                else if (ret == QMessageBox::Save)
                {
                    if (!doc->model()->save())
                    {
                        QMessageBox::critical(this, "Save Failed", "Could not save the file.");
                        e->ignore();
                        return;
                    }
                }
            }

            doc->close();
        }
    }
    e->accept();
}

void
dodo::ToggleTabBar() noexcept
{
    auto bar = m_tab_widget->tabBar();

    if (bar->isVisible())
        bar->hide();
    else
        bar->show();
}

bool
dodo::eventFilter(QObject *object, QEvent *event)
{
    if (m_link_hint_mode)
    {
        if (event->type() == QEvent::KeyPress)
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

        if (event->type() == QEvent::ShortcutOverride)
        {
            event->accept();
            // QShortcutEvent *stEvent = static_cast<QShortcutEvent *>(event);
            // stEvent->accept();
            return true;
        }
    } else {

        if (object == m_tab_widget->tabBar() && event->type() == QEvent::ContextMenu)
        {
            QContextMenuEvent *contextEvent = static_cast<QContextMenuEvent *>(event);
            int index                       = m_tab_widget->tabBar()->tabAt(contextEvent->pos());

            if (index != -1)
            {
                QMenu menu;
                menu.addAction("Open Location", this, [this, index]() { openInExplorerForIndex(index); });
                menu.addAction("File Properties", this, [this, index]() { filePropertiesForIndex(index); });
                menu.addAction("Close Tab", this, [this, index]() { m_tab_widget->tabCloseRequested(index); });

                menu.exec(contextEvent->globalPos());
            }
            return true;
        }
    }

    // Let other events pass through
    return QObject::eventFilter(object, event);

}

void
dodo::openInExplorerForIndex(int index) noexcept
{
    auto doc = qobject_cast<DocumentView *>(m_tab_widget->widget(index));
    if (doc)
    {
        QString filepath = doc->fileName();
        QDesktopServices::openUrl(QUrl(QFileInfo(filepath).absolutePath()));
    }
}

void
dodo::filePropertiesForIndex(int index) noexcept
{
    auto doc = qobject_cast<DocumentView *>(m_tab_widget->widget(index));
    if (doc)
        doc->FileProperties();
}

void
dodo::initTabConnections(DocumentView *docwidget) noexcept
{
    connect(docwidget, &DocumentView::panelNameChanged, this,
            [this](const QString &name) { m_panel->setFileName(name); });

    connect(docwidget, &DocumentView::fileNameChanged, this, &dodo::handleFileNameChanged);

    connect(docwidget, &DocumentView::pageNumberChanged, m_panel, &Panel::setPageNo);
    connect(docwidget, &DocumentView::searchCountChanged, m_panel, &Panel::setSearchCount);
    connect(docwidget, &DocumentView::searchModeChanged, m_panel, &Panel::setSearchMode);
    connect(docwidget, &DocumentView::searchIndexChanged, m_panel, &Panel::setSearchIndex);
    connect(docwidget, &DocumentView::totalPageCountChanged, m_panel, &Panel::setTotalPageCount);
    connect(docwidget, &DocumentView::fitModeChanged, m_panel, &Panel::setFitMode);
    connect(docwidget, &DocumentView::selectionModeChanged, m_panel, &Panel::setMode);
    connect(docwidget, &DocumentView::highlightColorChanged, m_panel, &Panel::setHighlightColor);
    connect(docwidget, &DocumentView::selectionModeChanged, m_panel, &Panel::setMode);
    connect(docwidget, &DocumentView::clipboardContentChanged, this,
            [&](const QString &text) { m_clipboard->setText(text); });

    connect(docwidget, &DocumentView::invertColorActionUpdate, this,
            [&](bool state) { m_actionInvertColor->setChecked(state); });

    connect(docwidget, &DocumentView::autoResizeActionUpdate, this,
            [&](bool state) { m_actionAutoresize->setChecked(state); });

    connect(docwidget, &DocumentView::insertToDBRequested, this, &dodo::insertFileToDB);
}

void
dodo::insertFileToDB(const QString &fname, int pageno) noexcept
{
    if (m_last_pages_db.isValid() && m_last_pages_db.isOpen())
    {
        QSqlQuery q(m_last_pages_db);
        q.prepare("UPDATE last_visited SET page_number = ?, last_accessed = ? WHERE file_path = ?");
        q.bindValue(0, pageno);
        q.bindValue(1, QDateTime::currentDateTime());
        q.bindValue(2, fname);
        q.exec();

        if (!q.exec() || q.numRowsAffected() == 0)
        {
            q.prepare("INSERT INTO last_visited (file_path, page_number, last_accessed) VALUES (?, ?, ?)");
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
    auto valid = m_doc->model()->valid();
    m_panel->hidePageInfo(!valid);
    m_actionCloseFile->setEnabled(valid);
    m_actionInvertColor->setEnabled(m_doc->model()->invertColor());
    m_actionAutoresize->setCheckable(m_doc->autoResize());

    switch (m_doc->selectionMode())
    {
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
    }
}

void
dodo::updatePanel() noexcept
{
    auto model = m_doc->model();
    m_panel->setFileName(m_doc->windowTitle());
    m_panel->setHighlightColor(model->highlightColor());
    m_panel->setMode(m_doc->selectionMode());
    m_panel->setTotalPageCount(model->numPages());
    m_panel->setPageNo(m_doc->pageNo());
    m_panel->setFitMode(m_doc->fitMode());
}

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
        sessionName = QInputDialog::getItem(this, "Save Session",
                                            "Enter name for session (existing sessions are listed): ", existingSessions,
                                            0, true, &ok);
    }

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

            DocumentView *view = new DocumentView(filePath, m_config, m_tab_widget);
            view->GotoPage(page);
            view->Zoom(zoom);
            view->Fit(static_cast<DocumentView::FitMode>(fitMode));
            OpenFile(view);
        }
    }
}

QStringList
dodo::getSessionFiles() noexcept
{
    QStringList sessions;

    if (!m_session_dir.exists())
    {
        if (!m_session_dir.mkpath("."))
        {
            QMessageBox::warning(this, "Session Directory", "Unable to create sessions directory for some reason");
            return sessions;
        }
    }

    for (const QString &file : m_session_dir.entryList(QStringList() << "*.dodo", QDir::Files | QDir::NoSymLinks))
    {
        sessions << QFileInfo(file).completeBaseName();
    }
    {};
    return sessions;
}

void
dodo::SaveSession(QString sessionName) noexcept
{
    if (!m_doc)
    {
        QMessageBox::information(this, "Save Session", "No files in session to save the session");
        return;
    }

    if (m_session_name.isEmpty())
    {
        QStringList existingSessions = getSessionFiles();
        if (sessionName.isEmpty())
        {
            bool ok;
            sessionName = QInputDialog::getItem(
                this, "Save Session", "Enter name for session (existing sessions are listed): ", existingSessions, 0,
                true, &ok);
            if (sessionName.isEmpty())
                return;
        }
        else
        {
            if (existingSessions.contains(sessionName))
            {
                QMessageBox::warning(this, "Save Session", QString("Session name %1 already exists").arg(sessionName));
                return;
            }
        }
    }

    m_session_name = sessionName;

    QJsonArray sessionArray;

    for (int i = 0; i < m_tab_widget->count(); ++i)
    {
        DocumentView *doc = qobject_cast<DocumentView *>(m_tab_widget->widget(i));
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

    QJsonDocument doc(sessionArray);
    QFile file(m_session_dir.filePath(m_session_name + ".dodo"));
    file.open(QIODevice::WriteOnly);
    file.write(doc.toJson());
    file.write("\n\n// vim:ft=json");
    file.close();

    m_session_name = sessionName;
}

void
dodo::SaveAsSession(const QString &sessionPath) noexcept
{
    if (m_session_name.isEmpty())
    {
        QMessageBox::information(this, "Save As Session", "Cannot save session as you are not currently in a session");
        return;
    }

    QStringList existingSessions = getSessionFiles();

    QString selectedPath = QFileDialog::getSaveFileName(this, "Save As Session", m_session_dir.absolutePath(),
                                                        "dodo session files (*.dodo); All Files (*.*)");

    if (selectedPath.isEmpty())
        return;

    if (QFile::exists(selectedPath))
    {
        auto choice = QMessageBox::warning(this, "Overwrite Session",
                                           QString("Session named \"%1\" already exists. Do you want to overwrite it?")
                                               .arg(QFileInfo(selectedPath).fileName()),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (choice != QMessageBox::Yes)
            return;
    }

    // Save the session
    QString currentSessionPath = m_session_dir.filePath(m_session_name + ".dodo");
    if (!QFile::copy(currentSessionPath, selectedPath))
    {
        QMessageBox::critical(this, "Save As Session", "Failed to save session.");
    }
}
