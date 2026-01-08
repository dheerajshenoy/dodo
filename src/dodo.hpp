#pragma once

#include "Config.hpp"
#include "DocumentView.hpp"
#include "DraggableTabBar.hpp"
#include "FloatingOverlayWidget.hpp"
#include "HighlightSearchWidget.hpp"
#include "MessageBar.hpp"
#include "OutlineWidget.hpp"
#include "Panel.hpp"
#include "PropertiesWidget.hpp"
#include "RecentFilesStore.hpp"
#include "SearchBar.hpp"
#include "ShortcutsWidget.hpp"
#include "StartupWidget.hpp"
#include "TabWidget.hpp"
#include "argparse.hpp"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenuBar>
#include <QShortcut>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabWidget>

#define __DODO_VERSION "v0.5.3"

class dodo : public QMainWindow
{
    Q_OBJECT

public:
    dodo() noexcept;
    dodo(const QString &sessionName,
         const QJsonArray &sessionArray) noexcept; // load from session
    ~dodo() noexcept;

    void readArgsParser(argparse::ArgumentParser &argparser) noexcept;
    bool OpenFile(DocumentView *view) noexcept;

protected:
    void closeEvent(QCloseEvent *e) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void construct() noexcept;
    void setDPR(float dpr) noexcept;
    void initDB() noexcept;
    void initDefaults() noexcept;
    void initMenubar() noexcept;
    void initGui() noexcept;
    void initConfig() noexcept;
    void initKeybinds() noexcept;
    void warnShortcutConflicts() noexcept;
    void setupKeybinding(const QString &action, const QString &key) noexcept;
    void populateRecentFiles() noexcept;
    void updateUiEnabledState() noexcept;
    void editLastPages() noexcept;
    void openLastVisitedFile() noexcept;
    void initConnections() noexcept;
    void initTabConnections(DocumentView *) noexcept;
    void initActionMap() noexcept;
    void trimRecentFilesDatabase() noexcept;
    void reloadDocument() noexcept;

    // Tab drag and drop handlers
    void handleTabDataRequested(int index,
                                DraggableTabBar::TabData *outData) noexcept;
    void handleTabDropReceived(const DraggableTabBar::TabData &data) noexcept;
    void handleTabDetached(int index, const QPoint &globalPos) noexcept;
    void
    handleTabDetachedToNewWindow(int index,
                                 const DraggableTabBar::TabData &data) noexcept;

    inline bool validTabIndex(int index) const noexcept
    {
        return m_tab_widget && index >= 0 && index < m_tab_widget->count();
    }

    // Interactive functions
    void EncryptDocument() noexcept;
    void DecryptDocument() noexcept;
    void Undo() noexcept;
    void Redo() noexcept;
    void ShowAbout() noexcept;
    void TextHighlightCurrentSelection() noexcept;
    void ShowKeybindings() noexcept;
    void ToggleSearchBar() noexcept;
    void ShowHighlightSearch() noexcept;
    void ToggleFocusMode() noexcept;
    void ToggleMenubar() noexcept;
    void ToggleTabBar() noexcept;
    void TogglePanel() noexcept;
    void ToggleFullscreen() noexcept;
    void FileProperties() noexcept;
    void SaveFile() noexcept;
    void SaveAsFile() noexcept;
    void CloseFile() noexcept;
    void ZoomIn() noexcept;
    void ZoomOut() noexcept;
    void FitNone() noexcept;
    void FitWidth() noexcept;
    void FitHeight() noexcept;
    void FitWindow() noexcept;
    void ToggleAutoResize() noexcept;
    void ShowOutline() noexcept;
    void InvertColor() noexcept;
    void TextSelectionMode() noexcept;
    void RegionSelectionMode() noexcept;
    void GoBackHistory() noexcept;
    void LastPage() noexcept;
    void NextPage() noexcept;
    void OpenContainingFolder() noexcept;
    void OpenFiles(const std::vector<std::string> &files) noexcept;
    void OpenFiles(const QList<QString> &files) noexcept;
    bool OpenFile(const QString &filename               = QString(),
                  const std::function<void()> &callback = {}) noexcept;
    void PrevPage() noexcept;
    void FirstPage() noexcept;
    void ToggleTextSelection() noexcept;
    void ToggleTextHighlight() noexcept;
    void ToggleRegionSelect() noexcept;
    void ToggleAnnotRect() noexcept;
    void ToggleAnnotSelect() noexcept;
    void ToggleAnnotPopup() noexcept;
    void YankSelection() noexcept;
    void ClearTextSelection() noexcept;
    void VisitLinkKB() noexcept;
    void CopyLinkKB() noexcept;
    void RotateClock() noexcept;
    void RotateAnticlock() noexcept;
    void ScrollDown() noexcept;
    void ScrollUp() noexcept;
    void ScrollLeft() noexcept;
    void ScrollRight() noexcept;
    void Search(const QString &term) noexcept;
    void NextHit() noexcept;
    void GotoHit(int index) noexcept;
    void PrevHit() noexcept;
    void ZoomReset() noexcept;
    void GotoPage() noexcept;
    void GotoLocation(int pageno, float x, float y) noexcept;
    void GotoLocation(const DocumentView::PageLocation &loc) noexcept;
    void gotoPage(int pageno) noexcept;
    void LoadSession(QString name = QString()) noexcept;
    void SaveSession() noexcept;
    void writeSessionToFile(const QString &sessionName) noexcept;
    void SaveAsSession(const QString &name = QString()) noexcept;
    void GotoTab(int tabno) noexcept;
    void LastTab() noexcept;
    void FirstTab() noexcept;
    void CloseTab(int tabno = -1) noexcept;
    void NextTab() noexcept;
    void PrevTab() noexcept;
    void ReselectLastTextSelection() noexcept;
    void SetLayoutMode(DocumentView::LayoutMode mode) noexcept;
    void setFocusMode(bool state) noexcept;

    // private helpers
    void handleFileNameChanged(const QString &name) noexcept;
    void handleCurrentTabChanged(int index) noexcept;
    void openInExplorerForIndex(int index) noexcept;
    void filePropertiesForIndex(int index) noexcept;
    void updateMenuActions() noexcept;
    void updatePanel() noexcept;
    QStringList getSessionFiles() noexcept;
    void insertFileToDB(const QString &fname, int pageno) noexcept;
    void clearKBHintsOverlay() noexcept;
    void handleFSFileChanged(const QString &filePath) noexcept;
    void onFileReloadTimer() noexcept;
    void showStartupWidget() noexcept;
    void updateActionsAndStuffForSystemTabs() noexcept;
    void updatePageNavigationActions() noexcept;
    void updateSelectionModeActions() noexcept;
    void updateGUIFromConfig() noexcept;
    void updateTabbarVisibility() noexcept;
    void setSessionName(const QString &name) noexcept;
    void openSessionFromArray(const QJsonArray &sessionArray) noexcept;
    void modeColorChangeRequested(const GraphicsView::Mode mode) noexcept;
    void handleEscapeKeyPressed() noexcept;

    QDir m_config_dir, m_session_dir;
    Panel *m_panel{nullptr};

    QMenuBar *m_menuBar{nullptr};
    QMenu *m_fitMenu{nullptr};
    QMenu *m_recentFilesMenu{nullptr};
    QMenu *m_navMenu{nullptr};
    QMenu *m_toggleMenu{nullptr};
    QMenu *m_viewMenu{nullptr};
    QMenu *m_layoutMenu{nullptr};

    QAction *m_actionLayoutSingle{nullptr};
    QAction *m_actionLayoutLeftToRight{nullptr};
    QAction *m_actionLayoutTopToBottom{nullptr};

    QAction *m_actionEncrypt{nullptr};
    QAction *m_actionDecrypt{nullptr};
    QMenu *m_modeMenu{nullptr};
    QAction *m_actionUndo{nullptr};
    QAction *m_actionRedo{nullptr};
    QAction *m_actionToggleTabBar{nullptr};
    QAction *m_actionFullscreen{nullptr};
    QAction *m_actionZoomIn{nullptr};
    QAction *m_actionInvertColor{nullptr};
    QAction *m_actionFileProperties{nullptr};
    QAction *m_actionOpenContainingFolder{nullptr};
    QAction *m_actionSaveFile{nullptr};
    QAction *m_actionSaveAsFile{nullptr};
    QAction *m_actionCloseFile{nullptr};
    QAction *m_actionZoomOut{nullptr};
    QAction *m_actionFitWidth{nullptr};
    QAction *m_actionFitHeight{nullptr};
    QAction *m_actionFitWindow{nullptr};
    QAction *m_actionFitNone{nullptr};
    QAction *m_actionAutoresize{nullptr};
    QAction *m_actionToggleMenubar{nullptr};
    QAction *m_actionTogglePanel{nullptr};
    QAction *m_actionToggleOutline{nullptr};
    QAction *m_actionToggleHighlightAnnotSearch{nullptr};
    QAction *m_actionGotoPage{nullptr};
    QAction *m_actionFirstPage{nullptr};
    QAction *m_actionPrevPage{nullptr};
    QAction *m_actionNextPage{nullptr};
    QAction *m_actionLastPage{nullptr};
    QAction *m_actionPrevLocation{nullptr};
    QAction *m_actionAbout{nullptr};
    QAction *m_actionTextHighlight{nullptr};
    QAction *m_actionAnnotRect{nullptr};
    QAction *m_actionAnnotPopup{nullptr};
    QAction *m_actionTextSelect{nullptr};
    QAction *m_actionRegionSelect{nullptr};
    QAction *m_actionAnnotEdit{nullptr};
    QAction *m_actionSessionLoad{nullptr};
    QAction *m_actionSessionSave{nullptr};
    QAction *m_actionSessionSaveAs{nullptr};
    QAction *m_actionHighlightSearch{nullptr};
    QTabWidget *m_side_panel_tabs{nullptr};
    FloatingOverlayWidget *m_outline_overlay{nullptr};
    FloatingOverlayWidget *m_highlight_overlay{nullptr};

    OutlineWidget *m_outline_widget{nullptr};

    Config m_config;
    float m_dpr{1.0f};

    enum class LinkHintMode
    {
        None = 0,
        Visit,
        Copy
    };

    QMap<QString, float> m_screen_dpr_map; // DPR per screen
    QString m_config_file_path;
    QString m_currentHintInput;
    bool m_link_hint_mode{false}, m_focus_mode{false},
        m_load_default_keybinding{true};
    StartupWidget *m_startup_widget{nullptr};
    LinkHintMode m_link_hint_current_mode{LinkHintMode::None};
    QMap<int, Model::LinkInfo> m_link_hint_map;
    DocumentView *m_doc{nullptr};
    TabWidget *m_tab_widget{nullptr};
    QVBoxLayout *m_layout = {nullptr};
    QMap<QString, std::function<void(const QStringList &args)>> m_actionMap;
    QClipboard *m_clipboard = QGuiApplication::clipboard();
    RecentFilesStore m_recent_files_store;
    QString m_recent_files_path;
    QString m_session_name;
    QFileSystemWatcher *m_config_watcher{nullptr};
    QMap<QString, DocumentView *> m_path_tab_map;
    MessageBar *m_message_bar;
    SearchBar *m_search_bar;
    HighlightSearchWidget *m_highlight_search_widget{nullptr};
};
