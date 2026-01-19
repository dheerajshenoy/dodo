#include "dodo.hpp"

#include "AboutDialog.hpp"
#include "CommandPaletteWidget.hpp"
#include "DocumentView.hpp"
#include "EditLastPagesWidget.hpp"
#include "FloatingOverlayWidget.hpp"
#include "GraphicsView.hpp"
#include "HighlightSearchWidget.hpp"
#include "SaveSessionDialog.hpp"
#include "SearchBar.hpp"
#include "StartupWidget.hpp"
#include "toml.hpp"
#include "utils.hpp"

#include <QColorDialog>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMimeData>
#include <QProcess>
#include <QSplitter>
#include <QStyleHints>
#include <QWindow>
#include <qobject.h>
#include <variant>

namespace
{
} // namespace

// Constructs the `dodo` class
dodo::dodo() noexcept
{
    setAttribute(Qt::WA_NativeWindow); // This is necessary for DPI updates
    setAcceptDrops(true);
    installEventFilter(this);
}

dodo::dodo(const QString &sessionName, const QJsonArray &sessionArray) noexcept
{
    setAttribute(Qt::WA_NativeWindow); // This is necessary for DPI updates
    setAcceptDrops(true);
    installEventFilter(this);
    construct();
    openSessionFromArray(sessionArray);
    setSessionName(sessionName);
    m_statusbar->setSessionName(sessionName);
}

// Destructor for `dodo` class
dodo::~dodo() noexcept {}

// On-demand construction of `dodo` (for use with argparse)
void
dodo::construct() noexcept
{
    m_tab_widget     = new TabWidget();
    m_config_watcher = new QFileSystemWatcher(this);

    initActionMap();
    initConfig();
    initGui();
    updateGUIFromConfig();
    if (m_load_default_keybinding)
        initDefaultKeybinds();
    initMenubar();
    warnShortcutConflicts();
    initDB();
    trimRecentFilesDatabase();
    populateRecentFiles();
    setMinimumSize(600, 400);
    initConnections();
    updateUiEnabledState();
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

    m_actionSessionSave
        = sessionMenu->addAction("Save", this, [&]() { SaveSession(); });
    m_actionSessionSaveAs
        = sessionMenu->addAction("Save As", this, [&]() { SaveAsSession(); });
    m_actionSessionLoad
        = sessionMenu->addAction("Load", this, [&]() { LoadSession(); });

    m_actionSessionSaveAs->setEnabled(false);

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
    m_viewMenu         = m_menuBar->addMenu("&View");
    m_actionFullscreen = m_viewMenu->addAction(
        QString("Fullscreen\t%1").arg(m_config.shortcuts["fullscreen"]), this,
        &dodo::ToggleFullscreen);
    m_actionFullscreen->setCheckable(true);
    m_actionFullscreen->setChecked(m_config.ui.window.fullscreen);

    m_actionZoomIn = m_viewMenu->addAction(
        QString("Zoom In\t%1").arg(m_config.shortcuts["zoom_in"]), this,
        &dodo::ZoomIn);
    m_actionZoomOut = m_viewMenu->addAction(
        QString("Zoom Out\t%1").arg(m_config.shortcuts["zoom_out"]), this,
        &dodo::ZoomOut);

    m_actionHighlightSearch = m_viewMenu->addAction("Search Highlights", this,
                                                    &dodo::ShowHighlightSearch);

    m_viewMenu->addSeparator();

    m_fitMenu = m_viewMenu->addMenu("Fit");

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
        m_config.ui.layout.auto_resize); // default on or off

    // --- Layout Menu ---

    m_viewMenu->addSeparator();
    m_layoutMenu                    = m_viewMenu->addMenu("Layout");
    QActionGroup *layoutActionGroup = new QActionGroup(this);
    layoutActionGroup->setExclusive(true);

    m_actionLayoutSingle = m_layoutMenu->addAction(
        QString("Single Page\t%1").arg(m_config.shortcuts["layout_single"]),
        this, [&]() { SetLayoutMode(DocumentView::LayoutMode::SINGLE); });

    m_actionLayoutLeftToRight = m_layoutMenu->addAction(
        QString("Left to Right Page\t%1")
            .arg(m_config.shortcuts["layout_left_to_right"]),
        this,
        [&]() { SetLayoutMode(DocumentView::LayoutMode::LEFT_TO_RIGHT); });

    m_actionLayoutTopToBottom = m_layoutMenu->addAction(
        QString("Top to Bottom Page\t%1")
            .arg(m_config.shortcuts["layout_top_to_bottom"]),
        this,
        [&]() { SetLayoutMode(DocumentView::LayoutMode::TOP_TO_BOTTOM); });

    layoutActionGroup->addAction(m_actionLayoutSingle);
    layoutActionGroup->addAction(m_actionLayoutLeftToRight);
    layoutActionGroup->addAction(m_actionLayoutTopToBottom);

    m_actionLayoutSingle->setCheckable(true);
    m_actionLayoutLeftToRight->setCheckable(true);
    m_actionLayoutTopToBottom->setCheckable(true);
    m_actionLayoutSingle->setChecked(
        m_config.ui.layout.mode == "single" ? true : false);
    m_actionLayoutLeftToRight->setChecked(
        m_config.ui.layout.mode == "left_to_right" ? true : false);
    m_actionLayoutTopToBottom->setChecked(
        m_config.ui.layout.mode == "top_to_bottom" ? true : false);

    // --- Toggle Menu ---

    m_viewMenu->addSeparator();
    m_toggleMenu = m_viewMenu->addMenu("Show/Hide");

#ifdef ENABLE_LLM_SUPPORT
    m_actionToggleLLMWidget = m_toggleMenu->addAction(
        QString("LLM Widget\t%1").arg(m_config.shortcuts["toggle_llm_widget"]),
        this, &dodo::ToggleLLMWidget);
    m_actionToggleLLMWidget->setCheckable(true);
    m_actionToggleLLMWidget->setChecked(m_config.ui.llm_widget.visible);
#endif

    m_actionToggleOutline = m_toggleMenu->addAction(
        QString("Outline\t%1").arg(m_config.shortcuts["outline"]), this,
        &dodo::ShowOutline);
    m_actionToggleOutline->setCheckable(true);
    m_actionToggleOutline->setChecked(!m_outline_widget->isHidden());

    m_actionToggleHighlightAnnotSearch = m_toggleMenu->addAction(
        QString("Highlight Annotation Search\t%1")
            .arg(m_config.shortcuts["highlight_annot_search"]),
        this, &dodo::ShowHighlightSearch);
    m_actionToggleHighlightAnnotSearch->setCheckable(true);
    m_actionToggleHighlightAnnotSearch->setChecked(
        !m_outline_widget->isHidden());

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
    m_actionTogglePanel->setChecked(!m_statusbar->isHidden());

    m_actionInvertColor = m_viewMenu->addAction(
        QString("Invert Color\t%1").arg(m_config.shortcuts["invert_color"]),
        this, &dodo::InvertColor);
    m_actionInvertColor->setCheckable(true);
    m_actionInvertColor->setChecked(m_config.behavior.invert_mode);

    // --- Tools Menu ---

    QMenu *toolsMenu = m_menuBar->addMenu("Tools");

    m_modeMenu = toolsMenu->addMenu("Mode");

    QActionGroup *modeActionGroup = new QActionGroup(this);
    modeActionGroup->setExclusive(true);

    m_actionRegionSelect = m_modeMenu->addAction(
        QString("Region Selection"), this, &dodo::ToggleRegionSelect);
    m_actionRegionSelect->setCheckable(true);
    modeActionGroup->addAction(m_actionRegionSelect);

    m_actionTextSelect = m_modeMenu->addAction(QString("Text Selection"), this,
                                               &dodo::ToggleTextSelection);
    m_actionTextSelect->setCheckable(true);
    modeActionGroup->addAction(m_actionTextSelect);

    m_actionTextHighlight = m_modeMenu->addAction(
        QString("Text Highlight\t%1")
            .arg(m_config.shortcuts["text_highlight_mode"]),
        this, &dodo::ToggleTextHighlight);
    m_actionTextHighlight->setCheckable(true);
    modeActionGroup->addAction(m_actionTextHighlight);

    m_actionAnnotRect
        = m_modeMenu->addAction(QString("Annotate Rectangle\t%1")
                                    .arg(m_config.shortcuts["annot_rect_mode"]),
                                this, &dodo::ToggleAnnotRect);
    m_actionAnnotRect->setCheckable(true);
    modeActionGroup->addAction(m_actionAnnotRect);

    m_actionAnnotEdit
        = m_modeMenu->addAction(QString("Edit Annotations\t%1")
                                    .arg(m_config.shortcuts["annot_edit_mode"]),
                                this, &dodo::ToggleAnnotSelect);
    m_actionAnnotEdit->setCheckable(true);
    modeActionGroup->addAction(m_actionAnnotEdit);

    m_actionAnnotPopup = m_modeMenu->addAction(
        QString("Annotate Popup\t%1")
            .arg(m_config.shortcuts["annot_popup_mode"]),
        this, &dodo::ToggleAnnotPopup);
    m_actionAnnotPopup->setCheckable(true);
    modeActionGroup->addAction(m_actionAnnotPopup);

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
        case GraphicsView::Mode::AnnotPopup:
            m_actionAnnotPopup->setChecked(true);
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

    // QMenu *markMenu = m_navMenu->addMenu("Marks");

    // m_actionSetMark = markMenu->addAction(
    //     QString("Set Mar\t%1").arg(m_config.shortcuts["set_mark"]), this,
    //     &dodo::SetMark);

    /* Help Menu */
    QMenu *helpMenu = m_menuBar->addMenu("&Help");
    m_actionAbout   = helpMenu->addAction(
        QString("About\t%1").arg(m_config.shortcuts["about"]), this,
        &dodo::ShowAbout);

    m_actionShowTutorialFile
        = helpMenu->addAction(QString("Open Tutorial File\t%1")
                                  .arg(m_config.shortcuts["tutorial_file"]),
                              this, &dodo::showTutorialFile);
}

// Initialize the recent files store
void
dodo::initDB() noexcept
{
    m_recent_files_path = m_config_dir.filePath("last_pages.json");
    m_recent_files_store.setFilePath(m_recent_files_path);
    if (!m_recent_files_store.load())
        qWarning() << "Failed to load recent files store";
}

// Initialize the default settings in case the config
// file is not found
void
dodo::initDefaults() noexcept
{
    m_config.ui.markers.jump_marker      = true;
    m_config.ui.statusbar.file_name_only = false;
    m_config.ui.zoom.level               = 1.0f;
    m_config.ui.outline.visible          = false;
    m_config.ui.window.title_format      = QStringLiteral("%1 - dodo");
    m_config.ui.link_hints.size          = 0.5f;
    m_config.ui.links.detect_urls        = false;
    m_config.ui.links.url_regex          = R"((https?://|www\.)[^\s<>()\"']+)";
    m_config.ui.highlight_search.visible = false;
    m_config.ui.highlight_search.type    = "overlay";
    m_config.ui.highlight_search.panel_position = "right";
    m_config.ui.highlight_search.panel_width    = 300;
    m_config.ui.outline.type                    = "overlay";

    m_config.ui.colors.search_index = 0x3DAEE944;
    m_config.ui.colors.search_match = 0x55FF8844;
    m_config.ui.colors.accent       = 0xFF500044;
    m_config.ui.colors.background   = 0x00000000;
    m_config.ui.colors.link_hint_fg = 0x000000FF;
    m_config.ui.colors.link_hint_bg = 0xFFFF00FF;
    m_config.ui.colors.highlight    = 0x55FFFF00;
    m_config.ui.colors.selection    = 0x55000055;
    m_config.ui.colors.jump_marker  = 0xFFFF0000;
    m_config.ui.colors.annot_rect   = 0x55FF0000;
    m_config.ui.colors.annot_popup  = 0xFFFFFFAA;

    m_config.rendering.dpi = 300.0f;
    m_config.rendering.dpr
        = m_screen_dpr_map.value(QApplication::primaryScreen()->name(), 1.0f);
    m_config.behavior.cache_pages          = 20;
    m_config.behavior.clear_inactive_cache = false;

    m_config.behavior.undo_limit            = 25;
    m_config.behavior.remember_last_visited = true;
    m_config.behavior.page_history_limit    = 10;
    m_config.ui.layout.auto_resize          = false;
    m_config.ui.zoom.factor                 = 1.25;
    m_config.ui.layout.initial_fit          = "width";
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

    auto ui_window                 = ui["window"];
    m_config.ui.window.startup_tab = ui_window["startup_tab"].value_or(true);
    m_config.ui.window.menubar     = ui_window["menubar"].value_or(true);
    m_config.ui.window.fullscreen  = ui_window["fullscreen"].value_or(false);
    if (m_config.ui.window.fullscreen)
        this->showFullScreen();
    QString window_title = QString::fromStdString(
        ui_window["window_title"].value_or("{} - dodo"));
    window_title.replace("{}", "%1");
    m_config.ui.window.title_format = window_title;

    auto ui_statusbar             = ui["statusbar"];
    m_config.ui.statusbar.visible = ui_statusbar["visible"].value_or(true);
    auto padding_array            = ui_statusbar["padding"].as_array();

    if (padding_array && padding_array->size() > 0)
    {
        m_config.ui.statusbar.padding = {padding_array->get(0)->value_or(5),
                                         padding_array->get(1)->value_or(5),
                                         padding_array->get(2)->value_or(5),
                                         padding_array->get(3)->value_or(5)};
    }

    m_config.ui.statusbar.show_progress
        = ui_statusbar["show_progress"].value_or(true);
    m_config.ui.statusbar.file_name_only
        = ui_statusbar["file_name_only"].value_or(false);
    m_config.ui.statusbar.show_file_info
        = ui_statusbar["show_file_info"].value_or(true);
    m_config.ui.statusbar.show_page_number
        = ui_statusbar["show_page_number"].value_or(true);
    m_config.ui.statusbar.show_mode = ui_statusbar["show_mode"].value_or(true);
    m_config.ui.statusbar.show_session_name
        = ui_statusbar["show_session_name"].value_or(false);

    auto ui_layout          = ui["layout"];
    m_config.ui.layout.mode = ui_layout["mode"].value_or("top_to_bottom");
    m_config.ui.layout.initial_fit = ui_layout["initial_fit"].value_or("width");
    m_config.ui.layout.auto_resize = ui_layout["auto_resize"].value_or(false);
    m_config.ui.layout.spacing     = ui_layout["spacing"].value_or(10);

    auto ui_zoom            = ui["zoom"];
    m_config.ui.zoom.level  = ui_zoom["level"].value_or(1.0);
    m_config.ui.zoom.factor = ui_zoom["factor"].value_or(1.25);

    auto ui_selection = ui["selection"];
    m_config.ui.selection.drag_threshold
        = ui_selection["drag_threshold"].value_or(50);

    auto ui_scrollbars              = ui["scrollbars"];
    m_config.ui.scrollbars.vertical = ui_scrollbars["vertical"].value_or(true);
    m_config.ui.scrollbars.horizontal
        = ui_scrollbars["horizontal"].value_or(true);
    m_config.ui.scrollbars.search_hits
        = ui_scrollbars["search_hits"].value_or(true);
    m_config.ui.scrollbars.auto_hide
        = ui_scrollbars["auto_hide"].value_or(true);
    m_config.ui.scrollbars.size = ui_scrollbars["size"].value_or(12);
    m_config.ui.scrollbars.hide_timeout
        = ui_scrollbars["hide_timeout"].value_or(1500);

    auto command_palette = ui["command_palette"];
    m_config.ui.command_palette.height
        = command_palette["height"].value_or(300);
    m_config.ui.command_palette.width = command_palette["width"].value_or(500);
    m_config.ui.command_palette.placeholder_text
        = QString::fromStdString(command_palette["placeholder_text"].value_or(
            std::string("Type a command...")));
    m_config.ui.command_palette.vscrollbar
        = command_palette["vscrollbar"].value_or(true);
    m_config.ui.command_palette.show_grid
        = command_palette["show_grid"].value_or(false);
    m_config.ui.command_palette.show_shortcuts
        = command_palette["show_shortcuts"].value_or(true);

    auto ui_markers                 = ui["markers"];
    m_config.ui.markers.jump_marker = ui_markers["jump_marker"].value_or(true);

    auto ui_links                 = ui["links"];
    auto ui_link_hints            = ui["link_hints"];
    m_config.ui.links.boundary    = ui_links["boundary"].value_or(false);
    m_config.ui.links.detect_urls = ui_links["detect_urls"].value_or(false);
    m_config.ui.links.url_regex
        = QString::fromStdString(ui_links["url_regex"].value_or(
            std::string(R"((https?://|www\.)[^\s<>()\"']+)")));
    m_config.ui.link_hints.size = ui_link_hints["size"].value_or(0.5f);

    auto ui_tabs                  = ui["tabs"];
    m_config.ui.tabs.visible      = ui_tabs["visible"].value_or(true);
    m_config.ui.tabs.auto_hide    = ui_tabs["auto_hide"].value_or(false);
    m_config.ui.tabs.closable     = ui_tabs["closable"].value_or(true);
    m_config.ui.tabs.movable      = ui_tabs["movable"].value_or(true);
    m_config.ui.tabs.elide_mode   = ui_tabs["elide_mode"].value_or("right");
    m_config.ui.tabs.bar_position = ui_tabs["bar_position"].value_or("top");

    auto ui_outline             = ui["outline"];
    m_config.ui.outline.visible = ui_outline["visible"].value_or(false);
    m_config.ui.outline.type    = "overlay";
    m_config.ui.outline.panel_position
        = ui_outline["panel_position"].value_or("left");
    m_config.ui.outline.panel_width = ui_outline["panel_width"].value_or(300);

    auto ui_highlight_search = ui["highlight_search"];
    m_config.ui.highlight_search.visible
        = ui_highlight_search["visible"].value_or(false);
    m_config.ui.highlight_search.type
        = ui_highlight_search["type"].value_or("overlay");
    m_config.ui.highlight_search.panel_position
        = ui_highlight_search["panel_position"].value_or("right");
    m_config.ui.highlight_search.panel_width
        = ui_highlight_search["panel_width"].value_or(300);

#ifdef ENABLE_LLM_SUPPORT
    m_config.ui.llm_widget.panel_position
        = ui["llm_widget"]["panel_position"].value_or("right");
    m_config.ui.llm_widget.panel_width
        = ui["llm_widget"]["panel_width"].value_or(400);
    m_config.ui.llm_widget.visible
        = ui["llm_widget"]["visible"].value_or(false);

    auto llm = toml["llm"];

    m_config.llm.provider   = llm["provider"].value_or("");
    m_config.llm.model      = llm["model"].value_or("");
    m_config.llm.max_tokens = llm["max_tokens"].value_or(512);

#endif

    auto colors = toml["colors"];

    if (!parseHexColor(colors["accent"].value_or("#3DAEE9FF"),
                       m_config.ui.colors.accent))
        m_config.ui.colors.accent = 0x3DAEE9FF;

    if (!parseHexColor(colors["background"].value_or("#00000000"),
                       m_config.ui.colors.background))
        m_config.ui.colors.background = 0x00000000;

    if (!parseHexColor(colors["search_match"].value_or("#55500033"),
                       m_config.ui.colors.search_match))
        m_config.ui.colors.search_match = 0x55500033;

    if (!parseHexColor(colors["search_index"].value_or("#55FF0055"),
                       m_config.ui.colors.search_index))
        m_config.ui.colors.search_index = 0x55FF0055;

    if (!parseHexColor(colors["link_hint_bg"].value_or("#000000FF"),
                       m_config.ui.colors.link_hint_bg))
        m_config.ui.colors.link_hint_bg = 0x000000FF;

    if (!parseHexColor(colors["link_hint_fg"].value_or("#EA3EE9FF"),
                       m_config.ui.colors.link_hint_fg))
        m_config.ui.colors.link_hint_fg = 0xEA3EE9FF;

    if (!parseHexColor(colors["selection"].value_or("#33000055"),
                       m_config.ui.colors.selection))
        m_config.ui.colors.selection = 0x33000055;

    if (!parseHexColor(colors["highlight"].value_or("#55FF0055"),
                       m_config.ui.colors.highlight))
        m_config.ui.colors.highlight = 0x55FF0055;

    if (!parseHexColor(colors["jump_marker"].value_or("#FF0000FF"),
                       m_config.ui.colors.jump_marker))
        m_config.ui.colors.jump_marker = 0xFF0000FF;

    if (!parseHexColor(colors["annot_rect"].value_or("#55FF5588"),
                       m_config.ui.colors.annot_rect))
        m_config.ui.colors.annot_rect = 0x55FF5588;

    if (!parseHexColor(colors["annot_popup"].value_or("#FFFFFFAA"),
                       m_config.ui.colors.annot_popup))
        m_config.ui.colors.annot_popup = 0xFFFFFFAA;

    if (!parseHexColor(colors["page_background"].value_or("#FFFFFF"),
                       m_config.ui.colors.page_background))
        m_config.ui.colors.page_background = 0xFFFFFF;

    if (!parseHexColor(colors["page_foreground"].value_or("#000000"),
                       m_config.ui.colors.page_foreground))
        m_config.ui.colors.page_foreground = 0x000000;

    auto rendering = toml["rendering"];

    m_config.rendering.dpi        = rendering["dpi"].value_or(72.0f);
    m_config.behavior.cache_pages = rendering["cache_pages"].value_or(10);
    m_config.rendering.antialiasing_bits
        = rendering["antialiasing_bits"].value_or(8);
    m_config.rendering.icc_color_profile
        = rendering["icc_color_profile"].value_or(true);

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
        if (mode_str == "region_select_mode")
            initial_mode = GraphicsView::Mode::RegionSelection;
        else if (mode_str == "text_select_mode")
            initial_mode = GraphicsView::Mode::TextSelection;
        else if (mode_str == "text_highlight_mode")
            initial_mode = GraphicsView::Mode::TextHighlight;
        else if (mode_str == "annot_rect_mode")
            initial_mode = GraphicsView::Mode::AnnotRect;
        else if (mode_str == "annot_select_mode")
            initial_mode = GraphicsView::Mode::AnnotSelect;
        else if (mode_str == "annot_popup_mode")
            initial_mode = GraphicsView::Mode::AnnotPopup;
        else
            initial_mode = GraphicsView::Mode::TextSelection;
        m_config.behavior.initial_mode = initial_mode;
    }

    m_config.behavior.confirm_on_quit
        = behavior["confirm_on_quit"].value_or(true);
    m_config.behavior.undo_limit = behavior["undo_limit"].value_or(25);
    m_config.behavior.remember_last_visited
        = behavior["remember_last_visited"].value_or(true);
    m_config.behavior.always_open_in_new_window
        = behavior["always_open_in_new_window"].value_or(false);
    m_config.behavior.page_history_limit
        = behavior["page_history"].value_or(100);
#ifdef HAS_SYNCTEX
    m_config.behavior.synctex_editor_command = QString::fromStdString(
        behavior["synctex_editor_command"].value_or(""));
#endif
    m_config.behavior.invert_mode = behavior["invert_mode"].value_or(false);
    m_config.behavior.auto_reload = behavior["auto_reload"].value_or(true);
    m_config.behavior.config_auto_reload
        = behavior["config_auto_reload"].value_or(true);
    m_config.behavior.recent_files = behavior["recent_files"].value_or(true);
    m_config.behavior.num_recent_files
        = behavior["num_recent_files"].value_or(10);
    m_config.behavior.cache_pages = behavior["cache_pages"].value_or(20);
    m_config.behavior.clear_inactive_cache
        = behavior["clear_inactive_cache"].value_or(false);

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
dodo::initDefaultKeybinds() noexcept
{
    // Helper lambda to create and connect shortcuts
    auto addShortcut = [this](const char *key, auto &&func)
    {
        auto *sc = new QShortcut(QKeySequence(QLatin1String(key)), this);
        connect(sc, &QShortcut::activated, std::forward<decltype(func)>(func));
    };

    // Navigation

    m_config.shortcuts["scroll_left"]  = "h";
    m_config.shortcuts["scroll_down"]  = "j";
    m_config.shortcuts["scroll_up"]    = "k";
    m_config.shortcuts["scroll_right"] = "l";

    addShortcut("h", [this]() { ScrollLeft(); });
    addShortcut("j", [this]() { ScrollDown(); });
    addShortcut("k", [this]() { ScrollUp(); });
    addShortcut("l", [this]() { ScrollRight(); });

    m_config.shortcuts["next_page"]  = "Shift+j";
    m_config.shortcuts["prev_page"]  = "Shift+k";
    m_config.shortcuts["first_page"] = "g,g";
    m_config.shortcuts["last_page"]  = "Shift+g";

    addShortcut("Shift+j", [this]() { NextPage(); });
    addShortcut("Shift+k", [this]() { PrevPage(); });
    addShortcut("g,g", [this]() { FirstPage(); });
    addShortcut("Shift+g", [this]() { LastPage(); });
    addShortcut("Ctrl+g", [this]() { GotoPage(); });

    // Search
    m_config.shortcuts["search"]      = "/";
    m_config.shortcuts["search_next"] = "n";
    m_config.shortcuts["search_prev"] = "Shift+n";

    addShortcut("/", [this]() { Search(); });
    addShortcut("n", [this]() { NextHit(); });
    addShortcut("Shift+n", [this]() { PrevHit(); });

    // Zoom
    m_config.shortcuts["zoom_in"]    = "+";
    m_config.shortcuts["zoom_out"]   = "-";
    m_config.shortcuts["zoom_reset"] = "0";

    addShortcut("=", [this]() { ZoomIn(); });
    addShortcut("-", [this]() { ZoomOut(); });
    addShortcut("0", [this]() { ZoomReset(); });

    // Fit modes
    m_config.shortcuts["fit_width"]   = "Ctrl+Shift+W";
    m_config.shortcuts["fit_height"]  = "Ctrl+Shift+H";
    m_config.shortcuts["fit_window"]  = "Ctrl+Shift+=";
    m_config.shortcuts["auto_resize"] = "Ctrl+Shift+R";

    addShortcut("Ctrl+Shift+W", [this]() { FitWidth(); });
    addShortcut("Ctrl+Shift+H", [this]() { FitHeight(); });
    addShortcut("Ctrl+Shift+=", [this]() { FitWindow(); });
    addShortcut("Ctrl+Shift+R", [this]() { ToggleAutoResize(); });

    // Outline and History
    m_config.shortcuts["outline"]                = "t";
    m_config.shortcuts["highlight_annot_search"] = "Alt+Shift+H";
    m_config.shortcuts["prev_location"]          = "Ctrl+o";

    addShortcut("t", [this]() { ShowOutline(); });
    addShortcut("Alt+Shift+H", [this]() { ShowOutline(); });
    addShortcut("Ctrl+o", [this]() { GoBackHistory(); });

    // Annotations and Interaction modes
    m_config.shortcuts["text_selection_mode"] = "1";
    m_config.shortcuts["text_highlight_mode"] = "2";
    m_config.shortcuts["annot_rect_mode"]     = "3";
    m_config.shortcuts["region_select_mode"]  = "4";
    m_config.shortcuts["annot_popup_mode"]    = "5";

    addShortcut("1", [this]() { ToggleTextSelection(); });
    addShortcut("2", [this]() { ToggleTextHighlight(); });
    addShortcut("3", [this]() { ToggleAnnotRect(); });
    addShortcut("4", [this]() { ToggleRegionSelect(); });
    addShortcut("5", [this]() { ToggleAnnotPopup(); });

    // Links and Actions
    m_config.shortcuts["link_hint_visit"] = "f";
    m_config.shortcuts["open_file"]       = "o";
    m_config.shortcuts["save"]            = "Ctrl+s";
    addShortcut("f", [this]() { VisitLinkKB(); });
    addShortcut("o", [this]() { OpenFile(); });
    addShortcut("Ctrl+s", [this]() { SaveFile(); });

    // Editing and UI
    m_config.shortcuts["undo"]            = "u";
    m_config.shortcuts["redo"]            = "Ctrl+r";
    m_config.shortcuts["invert_color"]    = "i";
    m_config.shortcuts["toggle_menubar"]  = "Ctrl+Shift+m";
    m_config.shortcuts["command_palette"] = ":";

    addShortcut("u", [this]() { Undo(); });
    addShortcut("Ctrl+r", [this]() { Redo(); });
    addShortcut("i", [this]() { InvertColor(); });
    addShortcut("Ctrl+Shift+m", [this]() { ToggleMenubar(); });
    addShortcut("Ctrl+Shift+P", [this]() { ToggleCommandPalette(); });

    // Rotation
    m_config.shortcuts["rotate_clock"]     = ">";
    m_config.shortcuts["rotate_anticlock"] = ">";
    addShortcut("<", [this]() { RotateAnticlock(); });
    addShortcut(">", [this]() { RotateClock(); });
}

void
dodo::warnShortcutConflicts() noexcept
{
    QHash<QString, QStringList> shortcutsByKey;
    for (auto it = m_config.shortcuts.constBegin();
         it != m_config.shortcuts.constEnd(); ++it)
    {
        const QString key = it.value().trimmed();
        if (key.isEmpty())
            continue;

        const QKeySequence seq(key);
        if (seq.isEmpty())
            continue;

        const QString normalized = seq.toString(QKeySequence::PortableText);
        if (normalized.isEmpty())
            continue;

        shortcutsByKey[normalized].append(it.key());
    }

    QStringList conflicts;
    for (auto it = shortcutsByKey.constBegin(); it != shortcutsByKey.constEnd();
         ++it)
    {
        if (it.value().size() < 2)
            continue;

        QString keyDisplay
            = QKeySequence(it.key()).toString(QKeySequence::NativeText);
        if (keyDisplay.isEmpty())
            keyDisplay = it.key();

        const QString actions = it.value().join(", ");
        conflicts.append(QString("%1 -> %2").arg(keyDisplay, actions));
    }

    if (conflicts.isEmpty())
        return;

    const int maxItems = 3;
    QString message;
    if (conflicts.size() <= maxItems)
    {
        message = QString("Shortcut conflict(s): %1").arg(conflicts.join("; "));
    }
    else
    {
        message = QString("Shortcut conflict(s): %1; and %2 more")
                      .arg(conflicts.mid(0, maxItems).join("; "))
                      .arg(conflicts.size() - maxItems);
    }

    qWarning() << message;
    if (m_message_bar)
        m_message_bar->showMessage(message, 6.0f);
}

// Initialize the GUI related Stuff
void
dodo::initGui() noexcept
{
    QWidget *widget = new QWidget();
    m_layout        = new QVBoxLayout();
    m_layout->setContentsMargins(0, 0, 0, 0);

    // Panel
    m_statusbar = new Statusbar(m_config, this);
    m_statusbar->hidePageInfo(true);
    m_statusbar->setMode(GraphicsView::Mode::TextSelection);
    m_statusbar->setSessionName("");

    m_search_bar = new SearchBar(this);
    m_search_bar->setVisible(false);

    m_message_bar = new MessageBar(this);
    m_message_bar->setVisible(false);

    m_outline_widget          = new OutlineWidget(this);
    m_highlight_search_widget = new HighlightSearchWidget(this);

    widget->setLayout(m_layout);
    m_tab_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_tab_widget->installEventFilter(this);

    this->setCentralWidget(widget);

    m_menuBar = this->menuBar(); // initialize here so that the config
                                 // visibility works

    const bool outlineSide = (m_config.ui.outline.type == "side_panel");
    const bool highlightSide
        = (m_config.ui.highlight_search.type == "side_panel");
    QWidget *mainContent = nullptr;
    if (outlineSide || highlightSide)
    {
        QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
        QWidget *sidePanel  = nullptr;
        bool panelLeft      = true;
        int panelWidth      = 300;

        if (outlineSide && highlightSide)
        {
            m_side_panel_tabs = new QTabWidget(this);
            m_side_panel_tabs->addTab(m_outline_widget, "Outline");
            m_side_panel_tabs->addTab(m_highlight_search_widget, "Highlights");
            sidePanel  = m_side_panel_tabs;
            panelLeft  = m_config.ui.outline.panel_position != "right";
            panelWidth = m_config.ui.outline.panel_width;
        }
        else if (outlineSide)
        {
            sidePanel  = m_outline_widget;
            panelLeft  = m_config.ui.outline.panel_position != "right";
            panelWidth = m_config.ui.outline.panel_width;
        }
        else
        {
            sidePanel  = m_highlight_search_widget;
            panelLeft  = m_config.ui.highlight_search.panel_position != "right";
            panelWidth = m_config.ui.highlight_search.panel_width;
        }

        if (panelLeft)
        {
            splitter->addWidget(sidePanel);
            splitter->addWidget(m_tab_widget);
            splitter->setStretchFactor(0, 0);
            splitter->setStretchFactor(1, 1);
            splitter->setSizes({panelWidth, this->width() - panelWidth});
        }
        else
        {
            splitter->addWidget(m_tab_widget);
            splitter->addWidget(sidePanel);
            splitter->setStretchFactor(0, 1);
            splitter->setStretchFactor(1, 0);
            splitter->setSizes({this->width() - panelWidth, panelWidth});
        }
        splitter->setFrameShape(QFrame::NoFrame);
        splitter->setFrameShadow(QFrame::Plain);
        splitter->setHandleWidth(1);
        splitter->setContentsMargins(0, 0, 0, 0);
        splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        mainContent = splitter;

        if (highlightSide)
            m_highlight_search_widget->setWindowFlags(Qt::Widget);
    }
    else
    {
        mainContent = m_tab_widget;
    }

#ifdef ENABLE_LLM_SUPPORT
    m_llm_widget = new LLMWidget(m_config, this);
    m_llm_widget->setVisible(m_config.ui.llm_widget.visible);
    connect(m_llm_widget, &LLMWidget::actionRequested, this,
            [this](const QString &action, const QStringList &args)
    {
        if (action == QStringLiteral("noop"))
            return;
        const auto it = m_actionMap.find(action);
        if (it == m_actionMap.end())
        {
            m_message_bar->showMessage(QStringLiteral("LLM: Unknown action"));
            return;
        }
        it.value()(args);
    });

    QSplitter *llm_splitter = new QSplitter(Qt::Horizontal, this);
    llm_splitter->addWidget(mainContent);
    llm_splitter->addWidget(m_llm_widget);
    llm_splitter->setStretchFactor(0, 1);
    llm_splitter->setStretchFactor(1, 0);
    const int llmWidth = m_config.ui.llm_widget.panel_width;
    llm_splitter->setSizes({this->width() - llmWidth, llmWidth});
    llm_splitter->setFrameShape(QFrame::NoFrame);
    llm_splitter->setFrameShadow(QFrame::Plain);
    llm_splitter->setHandleWidth(1);
    llm_splitter->setContentsMargins(0, 0, 0, 0);
    llm_splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_layout->addWidget(llm_splitter, 1);
#else
    m_layout->addWidget(mainContent, 1);
#endif

    if (!outlineSide && m_config.ui.outline.type == "overlay")
    {
        m_outline_overlay = new FloatingOverlayWidget(m_tab_widget);
        m_outline_overlay->setContentWidget(m_outline_widget);
        connect(m_outline_overlay, &FloatingOverlayWidget::overlayHidden, this,
                [this]()
        {
            if (m_actionToggleOutline)
                m_actionToggleOutline->setChecked(false);
            this->setFocus();
        });
    }
    else if (!outlineSide)
    {
        m_outline_widget->setWindowFlags(Qt::Dialog);
        m_outline_widget->setWindowModality(Qt::NonModal);
    }

    if (!highlightSide && m_config.ui.highlight_search.type == "overlay")
    {
        m_highlight_overlay = new FloatingOverlayWidget(m_tab_widget);
        m_highlight_overlay->setContentWidget(m_highlight_search_widget);
        connect(m_highlight_overlay, &FloatingOverlayWidget::overlayHidden,
                this, [this]()
        {
            if (m_actionToggleOutline)
                m_actionToggleOutline->setChecked(false);
            this->setFocus();
        });
    }
    else if (!highlightSide)
    {
        m_highlight_search_widget->setWindowFlags(Qt::Dialog);
        m_highlight_search_widget->setWindowModality(Qt::NonModal);
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
    m_actionHighlightSearch->setEnabled(hasOpenedFile);
    m_actionGotoPage->setEnabled(hasOpenedFile);
    m_actionFirstPage->setEnabled(hasOpenedFile);
    m_actionPrevPage->setEnabled(hasOpenedFile);
    m_actionNextPage->setEnabled(hasOpenedFile);
    m_actionLastPage->setEnabled(hasOpenedFile);
    m_actionFileProperties->setEnabled(hasOpenedFile);
    m_actionCloseFile->setEnabled(hasOpenedFile);
    m_fitMenu->setEnabled(hasOpenedFile);
    // m_actionToggleOutline->setEnabled(hasOpenedFile);
    m_actionInvertColor->setEnabled(hasOpenedFile);
    m_actionSaveFile->setEnabled(hasOpenedFile);
    m_actionSaveAsFile->setEnabled(hasOpenedFile);
    m_actionPrevLocation->setEnabled(hasOpenedFile);
    m_actionEncrypt->setEnabled(hasOpenedFile);
    m_actionDecrypt->setEnabled(hasOpenedFile);
    m_actionSessionSave->setEnabled(hasOpenedFile);
    m_actionSessionSaveAs->setEnabled(!m_session_name.isEmpty());
    updateSelectionModeActions();
}

// Helper function to construct `QShortcut` Qt shortcut
// from the config file
void
dodo::setupKeybinding(const QString &action, const QString &key) noexcept
{
    auto it = m_actionMap.find(action);
    if (it != m_actionMap.end())
    {
        QShortcut *shortcut = new QShortcut(QKeySequence(key), this);
        connect(shortcut, &QShortcut::activated, [it]() { it.value()({}); });
    }

#ifndef NDEBUG
    qDebug() << "Keybinding set:" << action << "->" << key;
#endif

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
    bool shown = !m_statusbar->isHidden();
    m_statusbar->setHidden(shown);
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
    abw->setAppInfo(APP_VERSION,
                    "A fast, configurable PDF reader built using MuPDF and Qt");
    abw->show();
}

// Reads the arguments passed with `dodo` from the
// commandline
void
dodo::ReadArgsParser(argparse::ArgumentParser &argparser) noexcept
{
    if (argparser.is_used("version"))
    {
        qInfo() << "dodo version: " << APP_VERSION;
        exit(0);
    }

    if (argparser.is_used("config"))
    {
        m_config_file_path
            = QString::fromStdString(argparser.get<std::string>("--config"));
    }

    this->construct();

    if (argparser.is_used("session"))
    {
        const QString &sessionName
            = QString::fromStdString(argparser.get<std::string>("--session"));
        LoadSession(sessionName);
    }

    if (argparser.is_used("page"))
        m_config.behavior.startpage_override = argparser.get<int>("--page");

#ifdef HAS_SYNCTEX
    if (argparser.is_used("synctex-forward"))
    {
        m_config.behavior.startpage_override = -1; // do not override the page

        // Format: --synctex-forward={pdf}#{src}:{line}:{column}
        // Example: --synctex-forward=test.pdf#main.tex:14
        const QString &arg = QString::fromStdString(
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
#endif

    if (argparser.is_used("files"))
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

    if (m_tab_widget->count() == 0 && m_config.ui.window.startup_tab)
        showStartupWidget();
    m_config.behavior.startpage_override = -1;
}

// Populates the `QMenu` for recent files with
// recent files entries from the store
void
dodo::populateRecentFiles() noexcept
{
    if (!m_config.behavior.recent_files)
    {
        m_recentFilesMenu->setEnabled(false);
        return;
    }

    m_recentFilesMenu->clear();
    for (const RecentFileEntry &entry : m_recent_files_store.entries())
    {
        if (entry.file_path.isEmpty())
            continue;
        const QString path  = entry.file_path;
        const int page      = entry.page_number;
        QAction *fileAction = new QAction(path, m_recentFilesMenu);
        connect(fileAction, &QAction::triggered, this, [&, path, page]()
        {
            OpenFile(path);
            gotoPage(page);
        });

        m_recentFilesMenu->addAction(fileAction);
    }

    if (m_recentFilesMenu->isEmpty())
        m_recentFilesMenu->setDisabled(true);
    else
        m_recentFilesMenu->setEnabled(true);
}

// Opens a widget that allows to edit the recent files
// entries
void
dodo::editLastPages() noexcept
{
    if (!m_config.behavior.remember_last_visited)
    {
        QMessageBox::information(
            this, "Edit Last Pages",
            "Couldn't find the recent files data. Maybe "
            "`remember_last_visited` option is turned off in the config "
            "file");
        return;
    }

    EditLastPagesWidget *elpw
        = new EditLastPagesWidget(&m_recent_files_store, this);
    elpw->show();
    connect(elpw, &EditLastPagesWidget::finished, this,
            &dodo::populateRecentFiles);
}

// Helper function to open last visited file
void
dodo::openLastVisitedFile() noexcept
{
    const QVector<RecentFileEntry> &entries = m_recent_files_store.entries();
    if (entries.isEmpty())
        return;

    const RecentFileEntry &entry = entries.first();
    if (QFile::exists(entry.file_path))
    {
        OpenFile(entry.file_path);
        gotoPage(entry.page_number);
    }
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
        m_doc->GotoPage(pageno - 1);
}

void
dodo::GotoLocation(int pageno, float x, float y) noexcept
{
    if (m_doc)
        m_doc->GotoLocation({pageno - 1, x, y});
}

void
dodo::GotoLocation(const DocumentView::PageLocation &loc) noexcept
{
    if (m_doc)
        m_doc->GotoLocation(loc);
}

// Goes to the next search hit
void
dodo::NextHit() noexcept
{
    if (m_doc)
        m_doc->NextHit();
}

void
dodo::GotoHit(int index) noexcept
{
    if (m_doc)
        m_doc->GotoHit(index);
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
    if (m_doc)
        m_doc->RotateClock();
}

// Rotates the file in anticlockwise direction
void
dodo::RotateAnticlock() noexcept
{
    if (m_doc)
        m_doc->RotateAnticlock();
}

// Shows link hints for each visible link to visit link
// using the keyboard
void
dodo::VisitLinkKB() noexcept
{
    if (m_doc)
    {
        m_lockedInputBuffer.clear();
        m_link_hint_map = m_doc->LinkKB();
        if (!m_link_hint_map.isEmpty())
        {
            m_link_hint_current_mode = LinkHintMode::Visit;
            m_link_hint_mode         = true;
            m_doc->UpdateKBHintsOverlay(m_lockedInputBuffer);
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
        m_lockedInputBuffer.clear();
        m_link_hint_map = m_doc->LinkKB();
        if (!m_link_hint_map.isEmpty())
        {
            m_link_hint_current_mode = LinkHintMode::Copy;
            m_link_hint_mode         = true;
            m_doc->UpdateKBHintsOverlay(m_lockedInputBuffer);
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

void
dodo::FitNone() noexcept
{
    if (m_doc)
        m_doc->setFitMode(DocumentView::FitMode::None);
}

// Opens multiple files given a list of file paths
void
dodo::OpenFiles(const std::vector<std::string> &files) noexcept
{
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
dodo::OpenFile(const QString &filePath,
               const std::function<void()> &callback) noexcept
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
        else
        {
            return OpenFile(files.first(), callback);
        }
    }

    QString fp = filePath;

    // expand ~
    if (fp == "~")
        fp = QDir::homePath();
    else if (fp.startsWith("~/"))
        fp = QDir(QDir::homePath()).filePath(fp.mid(2));

    // make absolute + clean
    fp = QDir::cleanPath(QFileInfo(fp).absoluteFilePath());

    // make absolute
    if (QDir::isRelativePath(fp))
        fp = QDir::current().absoluteFilePath(fp);

    // Switch to already opened filepath, if it's open.
    auto it = m_path_tab_map.find(fp);
    if (it != m_path_tab_map.end())
    {
        int existingIndex = m_tab_widget->indexOf(it.value());
        if (existingIndex != -1)
        {
            m_tab_widget->setCurrentIndex(existingIndex);
            const int page = it.value()->pageNo() + 1;
            insertFileToDB(fp, page > 0 ? page : 1);
            return true;
        }
    }

    if (!QFile::exists(fp))
    {
        QMessageBox::warning(this, "Open File",
                             QString("Unable to find %1").arg(fp));
        return false;
    }

    if (m_config.behavior.always_open_in_new_window)
    {
        bool has_document_tab = false;
        for (int i = 0; i < m_tab_widget->count(); ++i)
        {
            if (qobject_cast<DocumentView *>(m_tab_widget->widget(i)))
            {
                has_document_tab = true;
                break;
            }
        }

        if (has_document_tab)
        {
            QStringList args;
            args << fp;
            bool started = QProcess::startDetached(
                QCoreApplication::applicationFilePath(), args);
            if (!started)
                m_message_bar->showMessage("Failed to open file in new window");
            return started;
        }
    }

    DocumentView *docwidget = new DocumentView(m_config, m_tab_widget);
    int index               = m_tab_widget->addTab(docwidget, fp);

    connect(docwidget, &DocumentView::openFileFinished, this,
            [this, callback, index](DocumentView *doc)
    {
        const QString filePath = doc->filePath();
        doc->setDPR(m_dpr);
        initTabConnections(doc);
        auto outline = doc->model()->getOutline();
        if (outline)
        {
            m_outline_widget->setOutline(outline);
            if (m_config.ui.outline.visible)
            {
                m_outline_widget->show();
            }
        }

        m_path_tab_map[filePath] = doc;

        // Record in history
        const int page = doc->pageNo() + 1;
        insertFileToDB(filePath, page > 0 ? page : 1);

        m_tab_widget->setCurrentIndex(index);

        updatePanel();
        if (callback)
            callback();
    });

    connect(docwidget, &DocumentView::openFileFailed, this,
            [this](DocumentView *doc)
    {
        // Only use deleteLater() for async cleanup
        doc->deleteLater();
        QMessageBox::warning(this, "Open File",
                             QString("Failed to open %1").arg(doc->filePath()));
    });

    docwidget->openAsync(fp);

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
        m_doc->setFitMode(DocumentView::FitMode::Width);
}

// Fit the document to the height of the window
void
dodo::FitHeight() noexcept
{
    if (m_doc)
        m_doc->setFitMode(DocumentView::FitMode::Height);
}

// Fit the document to the window
void
dodo::FitWindow() noexcept
{
    if (m_doc)
        m_doc->setFitMode(DocumentView::FitMode::Window);
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
    if (!m_outline_widget->hasOutline())
    {
        QMessageBox::information(this, "Outline",
                                 "This document has no outline.");
        return;
    }
    QWidget *target = m_outline_widget;

    if (m_config.ui.outline.type == "overlay" && m_outline_overlay)
        target = m_outline_overlay;

    if (target->isVisible())
    {
        target->hide();
        m_actionToggleOutline->setChecked(false);
    }
    else
    {
        target->show();
        target->raise();
        target->activateWindow();
        m_actionToggleOutline->setChecked(true);
    }
}

// Show the highlight search panel
void
dodo::ShowHighlightSearch() noexcept
{
    if (!m_doc)
        return;

    m_highlight_search_widget->setModel(m_doc->model());
    if (m_config.ui.highlight_search.type == "side_panel" && m_side_panel_tabs)
        m_side_panel_tabs->setCurrentWidget(m_highlight_search_widget);

    QWidget *target = m_highlight_search_widget;
    if (m_config.ui.highlight_search.type == "overlay" && m_highlight_overlay)
        target = m_highlight_overlay;

    if (target->isVisible())
    {
        target->hide();
        m_actionToggleHighlightAnnotSearch->setChecked(false);
    }
    else
    {
        target->show();
        target->raise();
        target->activateWindow();
        m_actionToggleHighlightAnnotSearch->setChecked(true);
        m_highlight_search_widget->refresh();
    }
}

// Invert colors of the document
void
dodo::InvertColor() noexcept
{
    if (m_doc)
    {
        m_doc->setInvertColor(!m_doc->invertColor());
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

// Toggle text selection mode
void
dodo::ToggleTextSelection() noexcept
{
    if (m_doc)
        m_doc->ToggleTextSelection();
}

// Toggle rectangle annotation mode
void
dodo::ToggleAnnotRect() noexcept
{
    if (m_doc)
        m_doc->ToggleAnnotRect();
}

// Toggle annotation select mode
void
dodo::ToggleAnnotSelect() noexcept
{
    if (m_doc)
        m_doc->ToggleAnnotSelect();
}

// Toggle popup annotation mode
void
dodo::ToggleAnnotPopup() noexcept
{
    if (m_doc)
        m_doc->ToggleAnnotPopup();
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
        m_doc->GotoFirstPage();
    updatePageNavigationActions();
}

// Go to the previous page
void
dodo::PrevPage() noexcept
{
    if (m_doc)
        m_doc->GotoPrevPage();
    updatePageNavigationActions();
}

// Go to the next page
void
dodo::NextPage() noexcept
{
    if (m_doc)
        m_doc->GotoNextPage();
    updatePageNavigationActions();
}

// Go to the last page
void
dodo::LastPage() noexcept
{
    if (m_doc)
        m_doc->GotoLastPage();

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
    connect(m_statusbar, &Statusbar::modeColorChangeRequested, this,
            [&](GraphicsView::Mode mode) { modeColorChangeRequested(mode); });

    connect(m_statusbar, &Statusbar::pageChangeRequested, this,
            &dodo::gotoPage);

    connect(m_config_watcher, &QFileSystemWatcher::fileChanged, this,
            &dodo::updateGUIFromConfig);
    QList<QScreen *> outputs = QGuiApplication::screens();
    connect(m_tab_widget, &QTabWidget::currentChanged, this,
            &dodo::handleCurrentTabChanged);

    // Tab drag and drop connections for cross-window tab transfer
    connect(m_tab_widget, &TabWidget::tabDataRequested, this,
            &dodo::handleTabDataRequested);
    connect(m_tab_widget, &TabWidget::tabDropReceived, this,
            &dodo::handleTabDropReceived);
    connect(m_tab_widget, &TabWidget::tabDetached, this,
            &dodo::handleTabDetached);
    connect(m_tab_widget, &TabWidget::tabDetachedToNewWindow, this,
            &dodo::handleTabDetachedToNewWindow);

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

    connect(m_search_bar, &SearchBar::searchRequested, this,
            [this](const QString &term)
    {
        if (m_doc)
            m_doc->Search(term);
    });

    connect(m_search_bar, &SearchBar::searchIndexChangeRequested, this,
            &dodo::GotoHit);
    connect(m_search_bar, &SearchBar::nextHitRequested, this, &dodo::NextHit);
    connect(m_search_bar, &SearchBar::prevHitRequested, this, &dodo::PrevHit);

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
                    m_outline_widget->clearOutline();
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

    connect(m_outline_widget, &OutlineWidget::jumpToLocationRequested, this,
            [this](int page, const QPointF &pos) // page returned is 1-based
    {
        m_doc->addToHistory(m_doc->CurrentLocation());
        m_doc->GotoLocation({page - 1, (float)pos.x(), (float)pos.y()});
        m_outline_overlay->hide();
    });

    connect(m_highlight_search_widget,
            &HighlightSearchWidget::gotoLocationRequested, this,
            [this](int page, const QPointF &pos) // page returned is 0-based
    {
        m_doc->addToHistory(m_doc->CurrentLocation());
        GotoLocation({page, (float)pos.x(), (float)pos.y()});
    });
}

// Handle when the file name is changed
void
dodo::handleFileNameChanged(const QString &name) noexcept
{
    m_statusbar->setFileName(name);
    this->setWindowTitle(name);
}

// Handle when the current tab is changed
void
dodo::handleCurrentTabChanged(int index) noexcept
{
    // Clear page cache for the previously active tab if configured
    if (m_config.behavior.clear_inactive_cache && m_doc)
    {
        m_doc->model()->clearPageCache();
    }

    if (index == -1)
    {
        m_statusbar->hidePageInfo(true);
        updateMenuActions();
        updateUiEnabledState();
        updatePanel();
        this->setWindowTitle("");
        if (m_highlight_overlay)
            m_highlight_overlay->hide();
        if (m_highlight_search_widget)
            m_highlight_search_widget->hide();
        return;
    }

    QWidget *widget = m_tab_widget->widget(index);

    if (widget->property("tabRole") == "startup")
    {
        m_doc = nullptr;
        updateActionsAndStuffForSystemTabs();
        this->setWindowTitle("Startup");
        if (m_highlight_overlay)
            m_highlight_overlay->hide();
        if (m_highlight_search_widget)
            m_highlight_search_widget->hide();
        return;
    }

    if (widget->property("tabRole") == "empty")
    {
        m_doc = nullptr;
        updateActionsAndStuffForSystemTabs();
        this->setWindowTitle("Welcome");
        if (m_highlight_overlay)
            m_highlight_overlay->hide();
        if (m_highlight_search_widget)
            m_highlight_search_widget->hide();
        return;
    }

    m_doc = qobject_cast<DocumentView *>(widget);

    updateMenuActions();
    updateUiEnabledState();
    updatePanel();

    m_outline_widget->setOutline(m_doc ? m_doc->model()->getOutline()
                                       : nullptr);

    if (m_highlight_search_widget)
    {
        if (m_doc)
        {
            m_highlight_search_widget->setModel(m_doc->model());
            if (m_config.ui.highlight_search.type == "overlay"
                && m_highlight_overlay)
            {
                m_highlight_overlay->setVisible(
                    m_config.ui.highlight_search.visible);
                if (m_highlight_overlay->isVisible())
                    m_highlight_search_widget->refresh();
            }
            else
            {
                m_highlight_search_widget->setVisible(
                    m_config.ui.highlight_search.visible);
                if (m_highlight_search_widget->isVisible())
                    m_highlight_search_widget->refresh();
            }
        }
        else
        {
            if (m_highlight_overlay)
                m_highlight_overlay->hide();
            m_highlight_search_widget->hide();
            m_highlight_search_widget->setModel(nullptr);
        }
    }

    this->setWindowTitle(m_doc->windowTitle());
}

void
dodo::handleTabDataRequested(int index,
                             DraggableTabBar::TabData *outData) noexcept
{
    if (!validTabIndex(index))
        return;

    QWidget *widget = m_tab_widget->widget(index);
    if (!widget)
        return;

    DocumentView *doc = qobject_cast<DocumentView *>(widget);
    if (!doc)
        return;

    outData->filePath    = doc->filePath();
    outData->currentPage = doc->pageNo() + 1;
    outData->zoom        = doc->zoom();
    outData->invertColor = doc->invertColor();
    outData->rotation    = doc->model()->rotation();
    outData->fitMode     = static_cast<int>(doc->fitMode());
}

void
dodo::handleTabDropReceived(const DraggableTabBar::TabData &data) noexcept
{
    if (data.filePath.isEmpty())
        return;

    // Open the file and restore its state
    OpenFile(data.filePath, [this, data]()
    {
        if (!m_doc)
            return;

        // Restore document state
        m_doc->GotoPage(data.currentPage - 1);
        m_doc->setZoom(data.zoom);
        m_doc->setInvertColor(data.invertColor);

        // Restore rotation
        int currentRotation = m_doc->model()->rotation();
        int targetRotation  = data.rotation;
        while (currentRotation != targetRotation)
        {
            m_doc->RotateClock();
            currentRotation = (currentRotation + 90) % 360;
        }

        // Restore fit mode
        m_doc->setFitMode(static_cast<DocumentView::FitMode>(data.fitMode));
        // updatePanel();
    });
}

void
dodo::handleTabDetached(int index, const QPoint &globalPos) noexcept
{
    Q_UNUSED(globalPos);

    if (!validTabIndex(index))
        return;

    // Close the tab that was successfully moved to another window
    m_tab_widget->tabCloseRequested(index);
}

void
dodo::handleTabDetachedToNewWindow(
    int index, const DraggableTabBar::TabData &data) noexcept
{
    if (!validTabIndex(index))
        return;

    if (data.filePath.isEmpty())
        return;

    // Spawn a new dodo process with the file
    QStringList args;
    args << "-p" << QString::number(data.currentPage);
    args << data.filePath;

    bool started = QProcess::startDetached(
        QCoreApplication::applicationFilePath(), args);

    if (started)
    {
        // Close the tab in this window
        m_tab_widget->tabCloseRequested(index);
    }
    else
    {
        m_message_bar->showMessage("Failed to open tab in new window");
    }
}

void
dodo::closeEvent(QCloseEvent *e)
{
    // Update session file if in session
    if (!m_session_name.isEmpty())
        writeSessionToFile(m_session_name);

    // First pass: handle all unsaved changes dialogs and mark documents as
    // handled
    for (int i = 0; i < m_tab_widget->count(); i++)
    {
        DocumentView *doc
            = qobject_cast<DocumentView *>(m_tab_widget->widget(i));
        if (doc)
        {
            // Unsaved Changes
            if (doc->isModified())
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
                    doc->SaveFile();
                }
            }
        }
    }

    if (m_config.behavior.confirm_on_quit)
    {
        int ret = QMessageBox::question(
            this, "Confirm Quit", "Are you sure you want to quit dodo?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (ret == QMessageBox::No)
        {
            e->ignore();
            return;
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

    // Link Hint Handle Key Press
    if (m_link_hint_mode)
    {
        if (handleLinkHintEvent(event))
            return true;
    }

    // Context menu for the tab widgets
    if ((object == m_tab_widget->tabBar() || object == m_tab_widget)
        && type == QEvent::ContextMenu)
        return handleTabContextMenu(object, event);

    // Drop Event
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

bool
dodo::handleTabContextMenu(QObject *object, QEvent *event) noexcept
{
    auto *contextEvent = static_cast<QContextMenuEvent *>(event);
    if (!contextEvent || !m_tab_widget)
        return false;

    const QPoint tabPos = object == m_tab_widget->tabBar()
                              ? contextEvent->pos()
                              : m_tab_widget->tabBar()->mapFrom(
                                    m_tab_widget, contextEvent->pos());
    const int index     = m_tab_widget->tabBar()->tabAt(tabPos);
    if (index == -1)
        return true;

    QMenu menu;
    menu.addAction("Open Location", this,
                   [this, index]() { openInExplorerForIndex(index); });
    menu.addAction("File Properties", this,
                   [this, index]() { filePropertiesForIndex(index); });
    menu.addSeparator();
    menu.addAction("Move Tab to New Window", this, [this, index]()
    {
        DraggableTabBar::TabData data;
        handleTabDataRequested(index, &data);
        if (!data.filePath.isEmpty())
            handleTabDetachedToNewWindow(index, data);
    });
    menu.addAction("Close Tab", this,
                   [this, index]() { m_tab_widget->tabCloseRequested(index); });

    menu.exec(contextEvent->globalPos());
    return true;
}

bool
dodo::handleLinkHintEvent(QEvent *event) noexcept
{
    const QEvent::Type type = event->type();
    switch (type)
    {
        case QEvent::KeyPress:
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            switch (keyEvent->key())
            {
                case Qt::Key_Escape:
                    handleEscapeKeyPressed();
                    return true;

                case Qt::Key_Backspace:
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
                    m_lockedInputBuffer.removeLast();
#else
                    if (!m_currentHintInput.isEmpty())
                        m_currentHintInput.chop(1);
#endif
                    if (m_doc)
                        m_doc->UpdateKBHintsOverlay(m_lockedInputBuffer);
                    return true;
                default:
                    break;
            }

            QString text = keyEvent->text();
            if (text.isEmpty())
            {
                const int key = keyEvent->key();
                if (key >= Qt::Key_0 && key <= Qt::Key_9)
                    text = QString(QChar('0' + (key - Qt::Key_0)));
            }

            bool appended = false;
            if (text.size() == 1 && text.at(0).isDigit())
            {
                m_lockedInputBuffer += text;
                appended = true;
            }

            if (!appended)
                return true;

            if (m_doc)
                m_doc->UpdateKBHintsOverlay(m_lockedInputBuffer);

            int num = m_lockedInputBuffer.toInt();
            auto it = m_link_hint_map.find(num);
            if (it != m_link_hint_map.end())
            {
                const Model::LinkInfo &info = it.value();

                switch (m_link_hint_current_mode)
                {
                    case LinkHintMode::None:
                        break;

                    case LinkHintMode::Visit:
                        m_doc->FollowLink(info);
                        break;

                    case LinkHintMode::Copy:
                        m_clipboard->setText(info.uri);
                        break;
                }

                m_lockedInputBuffer.clear();
                m_link_hint_map.clear();
                m_doc->ClearKBHintsOverlay();
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

    return false;
}

// Used for waiting input events like marks etc.
// bool
// dodo::handleGetInputEvent(QEvent *event) noexcept
// {
//     const QEvent::Type type = event->type();
//     switch (type)
//     {
//         case QEvent::KeyPress:
//         {
//             QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
//             switch (keyEvent->key())
//             {
//                 case Qt::Key_Escape:
//                     handleEscapeKeyPressed();
//                     return true;

//                 case Qt::Key_Backspace:
// #if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
//                     m_lockedInputBuffer.removeLast();
// #else
//                     if (!m_lockedInputBuffer.isEmpty())
//                         m_lockedInputBuffer.chop(1);
// #endif
//                     return true;
//                 default:
//                     break;
//             }

//             QString text = keyEvent->text();

//             bool appended = false;
//             if (text.size() == 1 && text.at(0).isDigit())
//             {
//                 m_lockedInputBuffer += text;
//                 appended = true;
//             }

//             if (!appended)
//                 return true;

//             int num = m_lockedInputBuffer.toInt();
//             keyEvent->accept();
//             return true;
//         }

//         case QEvent::ShortcutOverride:
//             event->accept();
//             return true;
//         default:
//             break;
//     }

//     return false;
// }

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
    // doc->setInvertColor(!doc->invertColor());
    if (doc)
        doc->FileProperties();
}

// Initialize connections on each tab addition
void
dodo::initTabConnections(DocumentView *docwidget) noexcept
{
    connect(docwidget, &DocumentView::panelNameChanged, this,
            [this](const QString &name) { m_statusbar->setFileName(name); });

    connect(docwidget, &DocumentView::currentPageChanged, m_statusbar,
            &Statusbar::setPageNo);

    connect(docwidget, &DocumentView::searchBarSpinnerShow, m_search_bar,
            &SearchBar::showSpinner);

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

    connect(m_statusbar, &Statusbar::modeChangeRequested, docwidget,
            &DocumentView::NextSelectionMode);

    connect(m_statusbar, &Statusbar::fitModeChangeRequested, docwidget,
            &DocumentView::NextFitMode);

    connect(docwidget, &DocumentView::fileNameChanged, this,
            &dodo::handleFileNameChanged);

    connect(docwidget, &DocumentView::pageChanged, m_statusbar,
            &Statusbar::setPageNo);

    connect(docwidget, &DocumentView::searchCountChanged, m_search_bar,
            &SearchBar::setSearchCount);

    // connect(docwidget, &DocumentView::searchModeChanged, m_panel,
    //         &SearchBar::setSearchMode);

    connect(docwidget, &DocumentView::searchIndexChanged, m_search_bar,
            &SearchBar::setSearchIndex);

    connect(docwidget, &DocumentView::totalPageCountChanged, m_statusbar,
            &Statusbar::setTotalPageCount);

    connect(docwidget, &DocumentView::highlightColorChanged, m_statusbar,
            &Statusbar::setHighlightColor);

    connect(docwidget, &DocumentView::selectionModeChanged, m_statusbar,
            &Statusbar::setMode);

    connect(docwidget, &DocumentView::clipboardContentChanged, this,
            [&](const QString &text) { m_clipboard->setText(text); });

    connect(docwidget, &DocumentView::autoResizeActionUpdate, this,
            [&](bool state) { m_actionAutoresize->setChecked(state); });

    connect(docwidget, &DocumentView::insertToDBRequested, this,
            &dodo::insertFileToDB);
}

// Insert file to store when tab is closed to track
// recent files
void
dodo::insertFileToDB(const QString &fname, int pageno) noexcept
{
    const QDateTime now = QDateTime::currentDateTime();
    m_recent_files_store.upsert(fname, pageno, now);
    if (!m_recent_files_store.save())
        qWarning() << "Failed to save recent files store";
}

// Update the menu actions based on the current document state
void
dodo::updateMenuActions() noexcept
{
    const bool valid = m_doc != nullptr;

    m_statusbar->hidePageInfo(!valid);
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
        m_actionAnnotPopup->setChecked(false);
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
        m_actionAnnotPopup->setChecked(false);
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
#ifndef NDEBUG
        qDebug() << "dodo::updatePanel() Updating panel for document:"
                 << m_doc->fileName();
#endif
        Model *model = m_doc->model();
        if (!model)
            return;

        if (m_config.ui.statusbar.file_name_only)
            m_statusbar->setFileName(m_doc->fileName());
        else
            m_statusbar->setFileName(m_doc->filePath());

        m_statusbar->setHighlightColor(model->highlightAnnotColor());
        m_statusbar->setMode(m_doc->selectionMode());
        m_statusbar->setTotalPageCount(model->numPages());
        m_statusbar->setPageNo(m_doc->pageNo() + 1);
    }
    else
    {
        m_statusbar->hidePageInfo(true);
        m_statusbar->setFileName("");
        m_statusbar->setHighlightColor("");
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
            this, "Load Session",
            "Session to load (existing sessions are listed): ",
            existingSessions, 0, true, &ok);
    }

    QFile file(m_session_dir.filePath(sessionName + ".json"));

    if (file.open(QIODevice::ReadOnly))
    {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);

        if (err.error != QJsonParseError::NoError)
        {
            QMessageBox::critical(this, "Session File Parse Error",
                                  err.errorString());
#ifndef NDEBUG
            qDebug() << "JSON parse error:" << err.errorString();
#endif
            return;
        }

        if (!doc.isArray())
        {
            QMessageBox::critical(this, "Session File Parse Error",
                                  "Session file root is not an array");
#ifndef NDEBUG
            qDebug() << "Session file root is not an array";
#endif
            return;
        }

        // Create a new dodo window to load the session into if there's document
        // already opened in the current window
        if (m_tab_widget->count() > 0)
        {
            new dodo(sessionName, doc.array());
        }
        else
        {
            // Open here in this window
            openSessionFromArray(doc.array());
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
             QStringList() << "*.json", QDir::Files | QDir::NoSymLinks))
        sessions << QFileInfo(file).completeBaseName();

    return sessions;
}

// Saves the current session
void
dodo::SaveSession() noexcept
{
    if (!m_doc)
    {
        QMessageBox::information(this, "Save Session",
                                 "No files in session to save the session");
        return;
    }

    const QStringList &existingSessions = getSessionFiles();

    while (true)
    {
        SaveSessionDialog dialog(existingSessions, this);

        if (dialog.exec() != QDialog::Accepted)
            return;

        const QString &sessionName = dialog.sessionName();

        if (sessionName.isEmpty())
        {
            QMessageBox::information(this, "Save Session",
                                     "Session name cannot be empty");
            return;
        }

        if (m_session_name != sessionName)
        {
            // Ask for overwrite if session with same name exists
            if (existingSessions.contains(sessionName))
            {
                auto choice = QMessageBox::warning(
                    this, "Overwrite Session",
                    QString(
                        "Session named \"%1\" already exists. Do you want to "
                        "overwrite it?")
                        .arg(sessionName),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

                if (choice == QMessageBox::No)
                    continue;
                if (choice == QMessageBox::Yes)
                {
                    setSessionName(sessionName);
                    break;
                }
            }
            else
            {
                setSessionName(sessionName);
                break;
            }
        }
    }
    // Save the session now
    writeSessionToFile(m_session_dir.filePath(m_session_name + ".json"));
}

void
dodo::writeSessionToFile(const QString &sessionName) noexcept
{
    QJsonArray sessionArray;

    for (int i = 0; i < m_tab_widget->count(); ++i)
    {
        DocumentView *doc
            = qobject_cast<DocumentView *>(m_tab_widget->widget(i));
        if (!doc)
            continue;

        QJsonObject entry;
        entry["file_path"]    = doc->filePath();
        entry["current_page"] = doc->pageNo() + 1;
        entry["zoom"]         = doc->zoom();
        entry["invert_color"] = doc->invertColor();
        entry["rotation"]     = doc->model()->rotation();
        entry["fit_mode"]     = static_cast<int>(doc->fitMode());

        sessionArray.append(entry);
    }

    QFile file(m_session_dir.filePath(m_session_name + ".json"));
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
    file.close();
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
        "dodo session files (*.json); All Files (*.*)");

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
        = m_session_dir.filePath(m_session_name + ".json");
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

    m_startup_widget = new StartupWidget(&m_recent_files_store, m_tab_widget);
    connect(m_startup_widget, &StartupWidget::openFileRequested, this,
            [this](const QString &path, int page)
    {
        OpenFile(path, [this, page]()
        {
            gotoPage(page);
            int index = m_tab_widget->indexOf(m_startup_widget);
            if (index != -1)
                m_tab_widget->tabCloseRequested(index);
        });
    });
    int index = m_tab_widget->addTab(m_startup_widget, "Startup");
    m_tab_widget->setCurrentIndex(index);
    m_statusbar->setFileName("Startup");
}

// Update actions and stuff for system tabs
void
dodo::updateActionsAndStuffForSystemTabs() noexcept
{
    m_statusbar->hidePageInfo(true);
    updateUiEnabledState();
    m_statusbar->setFileName("Startup");
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
    {name, [this](const QStringList &) { func(); }}

// Initialize the action map
void
dodo::initActionMap() noexcept
{
    m_actionMap = {
        // Actions with arguments
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
            m_message_bar->showMessage(QStringLiteral("Invalid DPR"));
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
            m_message_bar->showMessage(QStringLiteral("Invalid tab index"));
    }},

    // Actions without arguments
#ifdef ENABLE_LLM_SUPPORT
        ACTION_NO_ARGS("toggle_llm_widget", ToggleLLMWidget),
#endif

        ACTION_NO_ARGS("tabs_close_left", TabsCloseLeft),
        ACTION_NO_ARGS("tabs_close_right", TabsCloseRight),
        ACTION_NO_ARGS("tabs_close_others", TabsCloseOthers),
        ACTION_NO_ARGS("command_palette", ToggleCommandPalette),
        ACTION_NO_ARGS("open_containing_folder", OpenContainingFolder),
        ACTION_NO_ARGS("encrypt", EncryptDocument),
        ACTION_NO_ARGS("reload", reloadDocument),
        ACTION_NO_ARGS("undo", Undo),
        ACTION_NO_ARGS("redo", Redo),
        ACTION_NO_ARGS("text_highlight_current_selection",
                       TextHighlightCurrentSelection),
        ACTION_NO_ARGS("toggle_tabs", ToggleTabBar),
        ACTION_NO_ARGS("save", SaveFile),
        ACTION_NO_ARGS("save_as", SaveAsFile),
        ACTION_NO_ARGS("yank", YankSelection),
        ACTION_NO_ARGS("cancel_selection", ClearTextSelection),
        ACTION_NO_ARGS("about", ShowAbout),
        ACTION_NO_ARGS("link_hint_visit", VisitLinkKB),
        ACTION_NO_ARGS("link_hint_copy", CopyLinkKB),
        ACTION_NO_ARGS("outline", ShowOutline),
        ACTION_NO_ARGS("highlight_annot_search", ShowHighlightSearch),
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
        ACTION_NO_ARGS("region_select_mode", ToggleRegionSelect),

        ACTION_NO_ARGS("annot_edit_mode", ToggleAnnotSelect),
        ACTION_NO_ARGS("annot_popup_mode", ToggleAnnotPopup),
        ACTION_NO_ARGS("annot_rect_mode", ToggleAnnotRect),

        ACTION_NO_ARGS("text_select_mode", ToggleTextSelection),
        ACTION_NO_ARGS("text_highlight_mode", ToggleTextHighlight),
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
        ACTION_NO_ARGS("save_session", SaveSession),
        ACTION_NO_ARGS("save_as_session", SaveAsSession),
        ACTION_NO_ARGS("load_session", LoadSession),
        ACTION_NO_ARGS("show_startup", showStartupWidget),
        ACTION_NO_ARGS("tab_first", FirstTab),
        ACTION_NO_ARGS("tab_last", LastTab),
        ACTION_NO_ARGS("tab_next", NextTab),
        ACTION_NO_ARGS("tab_prev", PrevTab),
        ACTION_NO_ARGS("tab_close", CloseTab),
        ACTION_NO_ARGS("tutorial_file", showTutorialFile),
        ACTION_NO_ARGS("reselect_last_selection", ReselectLastTextSelection),
        ACTION_NO_ARGS("search", Search),

        {"layout_single", [this](const QStringList &)
    { SetLayoutMode(DocumentView::LayoutMode::SINGLE); }},
        {"layout_left_to_right", [this](const QStringList &)
    { SetLayoutMode(DocumentView::LayoutMode::LEFT_TO_RIGHT); }},
        {"layout_top_to_bottom", [this](const QStringList &)
    { SetLayoutMode(DocumentView::LayoutMode::TOP_TO_BOTTOM); }},

        {"tab1", [this](const QStringList &) { GotoTab(1); }},
        {"tab2", [this](const QStringList &) { GotoTab(2); }},
        {"tab3", [this](const QStringList &) { GotoTab(3); }},
        {"tab4", [this](const QStringList &) { GotoTab(4); }},
        {"tab5", [this](const QStringList &) { GotoTab(5); }},
        {"tab6", [this](const QStringList &) { GotoTab(6); }},
        {"tab7", [this](const QStringList &) { GotoTab(7); }},
        {"tab8", [this](const QStringList &) { GotoTab(8); }},
        {"tab9", [this](const QStringList &) { GotoTab(9); }},
    };
}

#undef ACTION_NO_ARGS

// Trims the recent files store to `num_recent_files` number of files
void
dodo::trimRecentFilesDatabase() noexcept
{
    // If num_recent_files config entry has negative value,
    // retain all the recent files
    if (m_config.behavior.num_recent_files < 0)
        return;

    m_recent_files_store.trim(m_config.behavior.num_recent_files);
    if (!m_recent_files_store.save())
        qWarning() << "Failed to trim recent files store";
}

// Sets the DPR of the current document
void
dodo::setDPR(float dpr) noexcept
{
    if (m_doc)
    {
        m_doc->GotoFirstPage();
        m_doc->setDPR(dpr);
    }
}

// Reload the document in place
void
dodo::reloadDocument() noexcept
{
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
    const int count = m_doc ? m_doc->numPages() : 0;

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
        m_doc->EncryptDocument();
    }
}

void
dodo::DecryptDocument() noexcept
{
    if (m_doc)
        m_doc->DecryptDocument();
}

// Update selection mode actions (QAction) in QMenu based on current
// selection mode
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
        case GraphicsView::Mode::AnnotPopup:
            m_actionAnnotPopup->setChecked(true);
            break;
        default:
            break;
    }
}

void
dodo::updateGUIFromConfig() noexcept
{
    m_tab_widget->setTabsClosable(m_config.ui.tabs.closable);
    m_tab_widget->setMovable(m_config.ui.tabs.movable);

    if (m_config.ui.tabs.bar_position == "top")
        m_tab_widget->setTabPosition(QTabWidget::North);
    else if (m_config.ui.tabs.bar_position == "bottom")
        m_tab_widget->setTabPosition(QTabWidget::South);
    else if (m_config.ui.tabs.bar_position == "left")
        m_tab_widget->setTabPosition(QTabWidget::West);
    else if (m_config.ui.tabs.bar_position == "right")
        m_tab_widget->setTabPosition(QTabWidget::East);

    if (m_config.ui.outline.type == "overlay" && m_outline_overlay)
    {
        m_outline_overlay->setVisible(m_config.ui.outline.visible);
    }
    else
    {
        m_outline_widget->setVisible(m_config.ui.outline.visible);
    }

    if (m_config.ui.highlight_search.type == "overlay" && m_highlight_overlay)
    {
        m_highlight_overlay->setVisible(m_config.ui.highlight_search.visible);
    }
    else
    {
        m_highlight_search_widget->setVisible(
            m_config.ui.highlight_search.visible);
    }

    m_layout->addWidget(m_search_bar);
    m_layout->addWidget(m_message_bar);
    m_layout->addWidget(m_statusbar);

    m_tab_widget->setTabBarAutoHide(m_config.ui.tabs.auto_hide);
    m_statusbar->setVisible(m_config.ui.statusbar.visible);
    m_menuBar->setVisible(m_config.ui.window.menubar);
    m_tab_widget->tabBar()->setVisible(m_config.ui.tabs.visible);
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
        m_statusbar->setVisible(false);
        m_tab_widget->tabBar()->setVisible(false);
    }
    else
    {
        m_menuBar->setVisible(m_config.ui.window.menubar);
        m_statusbar->setVisible(m_config.ui.statusbar.visible);
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

void
dodo::search(const QString &term) noexcept
{
    if (m_doc)
        m_doc->Search(term);
}

void
dodo::Search() noexcept
{
    if (m_doc)
    {
        m_search_bar->setVisible(true);
        m_search_bar->focusSearchInput();
    }
}

void
dodo::setSessionName(const QString &name) noexcept
{
    m_session_name = name;
    m_statusbar->setSessionName(name);
}

void
dodo::openSessionFromArray(const QJsonArray &sessionArray) noexcept
{
    for (const QJsonValue &value : sessionArray)
    {
        const QJsonObject entry = value.toObject();
        const QString filePath  = entry["file_path"].toString();
        const int page          = entry["current_page"].toInt();
        const double zoom       = entry["zoom"].toDouble();
        const int fitMode       = entry["fit_mode"].toInt();
        const bool invert       = entry["invert_color"].toBool();

        if (filePath.isEmpty())
            continue;

        // Use a lambda to capture session settings and apply them after file
        // opens
        OpenFile(filePath, [this, page, zoom, fitMode, invert]()
        {
            if (m_doc)
            {
                if (invert)
                    m_doc->setInvertColor(true);
                m_doc->setFitMode(static_cast<DocumentView::FitMode>(fitMode));
                m_doc->setZoom(zoom);
                m_doc->GotoPage(page);
            }
        });
    }
}

void
dodo::modeColorChangeRequested(const GraphicsView::Mode mode) noexcept
{
    // FIXME: Make this a function
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
        else if (mode == GraphicsView::Mode::AnnotPopup)
            model->setPopupColor(color);

        m_statusbar->setHighlightColor(color);
    }
}

void
dodo::ReselectLastTextSelection() noexcept
{
    if (m_doc)
        m_doc->ReselectLastTextSelection();
}

void
dodo::SetLayoutMode(DocumentView::LayoutMode mode) noexcept
{
    if (m_doc)
        m_doc->setLayoutMode(mode);
}

// Handle Escape key press for the entire application
void
dodo::handleEscapeKeyPressed() noexcept
{
#ifndef NDEBUG
    qDebug() << "Escape key pressed handled";
#endif

    m_lockedInputBuffer.clear();

    if (m_link_hint_mode)
    {
        m_doc->ClearKBHintsOverlay();
        m_link_hint_map.clear();
        m_link_hint_mode = false;
        return;
    }
}

void
dodo::ToggleCommandPalette() noexcept
{

    if (!m_command_palette_widget)
    {

        m_command_palette_widget = new CommandPaletteWidget(m_config, this);
        connect(m_command_palette_widget,
                &CommandPaletteWidget::commandSelected, this,
                [this](const QString &command, const QStringList &args)
        {
            m_actionMap[command](args);
            m_command_palette_overlay->setVisible(false);
        });
        m_command_palette_overlay = new FloatingOverlayWidget(this);
        m_command_palette_overlay->setContentWidget(m_command_palette_widget);
    }

    m_command_palette_overlay->setVisible(
        !m_command_palette_overlay->isVisible());
}

#ifdef ENABLE_LLM_SUPPORT
void
dodo::ToggleLLMWidget() noexcept
{
    m_llm_widget->setVisible(!m_llm_widget->isVisible());
}
#endif

void
dodo::showTutorialFile() noexcept
{
#if defined(__linux__)
    const QString doc_path = QString("%1%2")
                                 .arg(APP_INSTALL_PREFIX)
                                 .arg("/share/doc/dodo/tutorial.pdf");
    OpenFile(doc_path);
#elif defined(__APPLE__) && defined(__MACH__)
    QMessageBox::warning(this, "Show Tutorial File",
                         "Not yet implemented for Macintosh");
#elif defined(_WIN64)
    QMessageBox::warning(this, "Show Tutorial File",
                         "Not yet implemented for Windows");
#endif
}

// void
// dodo::SetMark() noexcept
// {
//     m_message_bar->showMessage("**SetMark**, Waiting for mark: ", -1);
// }

// void
// dodo::GotoMark() noexcept
// {
//     m_message_bar->showMessage("**GotoMark**, Waiting for mark: ", -1);
//     // Wait for key input
// }

// void
// dodo::DeleteMark() noexcept
// {
//     m_message_bar->showMessage("**GotoMark**, Waiting for mark: ", -1);
// }

// void
// dodo::setMark(const QString &key, const int pageno,
//               const DocumentView::PageLocation location) noexcept
// {
// }

// void
// dodo::gotoMark(const QString &key) noexcept
// {
// }

// void
// dodo::deleteMark(const QString &key) noexcept
// {
// }

void
dodo::TabsCloseLeft() noexcept
{
    const int currentIndex = m_tab_widget->currentIndex();

    if (currentIndex == 0)
        return;

    for (int i = 0; i < currentIndex; i++)
        m_tab_widget->tabCloseRequested(i);
}

void
dodo::TabsCloseRight() noexcept
{
    const int currentIndex = m_tab_widget->currentIndex();
    const int ntabs        = m_tab_widget->count();

    if (currentIndex == ntabs - 1)
        return;

    for (int i = currentIndex; i < ntabs; i++)
        m_tab_widget->tabCloseRequested(i);
}

void
dodo::TabsCloseOthers() noexcept
{
    const int ntabs = m_tab_widget->count();

    if (ntabs == 0)
        return;

    const int currentIndex = m_tab_widget->currentIndex();

    for (int i = 0; i < ntabs && i != currentIndex; i++)
        m_tab_widget->tabCloseRequested(i);
}
