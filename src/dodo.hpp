#pragma once

#include "CommandBar.hpp"
#include "Config.hpp"
#include "DocumentView.hpp"
#include "MessageBar.hpp"
#include "OutlineWidget.hpp"
#include "Panel.hpp"
#include "PropertiesWidget.hpp"
#include "ShortcutsWidget.hpp"
#include "StartupWidget.hpp"
#include "TabWidget.hpp"
#include "argparse.hpp"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenuBar>
#include <QShortcut>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabWidget>
#include <qstackedlayout.h>

#define __DODO_VERSION "v0.2.4-alpha"

class dodo : public QMainWindow
{
public:
    dodo() noexcept;
    ~dodo() noexcept;

    void readArgsParser(argparse::ArgumentParser &argparser) noexcept;
    void construct() noexcept;

protected:
    void closeEvent(QCloseEvent *e) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void setDPR(float dpr) noexcept;
    void initDB() noexcept;
    void initDefaults() noexcept;
    void initMenubar() noexcept;
    void initGui() noexcept;
    void initConfig() noexcept;
    void initKeybinds() noexcept;
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
    bool validTabIndex(int index) noexcept;

    // Interactive functions
    void Undo() noexcept;
    void Redo() noexcept;
    void ShowAbout() noexcept;
    void TextHighlightCurrentSelection() noexcept;
    void ShowKeybindings() noexcept;
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
    void TableOfContents() noexcept;
    void InvertColor() noexcept;
    void TextSelectionMode() noexcept;
    void GoBackHistory() noexcept;
    void LastPage() noexcept;
    void NextPage() noexcept;
    void OpenFiles(const std::vector<std::string> &files) noexcept;
    void OpenFiles(const QList<QString> &files) noexcept;
    bool OpenFile(QString filename = QString()) noexcept;
    bool OpenFile(DocumentView *view) noexcept;
    void PrevPage() noexcept;
    void FirstPage() noexcept;
    void ToggleTextHighlight() noexcept;
    void ToggleRectAnnotation() noexcept;
    void ToggleAnnotSelect() noexcept;
    void YankAll() noexcept;
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
    void PrevHit() noexcept;
    void ZoomReset() noexcept;
    void GotoPage() noexcept;
    void gotoPage(int pageno) noexcept;
    void LoadSession(QString name = QString()) noexcept;
    void SaveSession(QString name = QString()) noexcept;
    void SaveAsSession(const QString &name = QString()) noexcept;
    void GotoTab(int tabno) noexcept;
    void LastTab() noexcept;
    void FirstTab() noexcept;
    void CloseTab(int tabno = -1) noexcept;
    void NextTab() noexcept;
    void PrevTab() noexcept;
    void someFunction();

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
    void processCommand(const QString &cmd) noexcept;
    void invokeCommand() noexcept;
    void invokeSearch() noexcept;

    QDir m_config_dir, m_session_dir;
    float m_default_zoom{0.0f};
    Panel *m_panel{nullptr};

    QMenuBar *m_menuBar{nullptr};
    QMenu *m_fitMenu{nullptr};
    QMenu *m_recentFilesMenu{nullptr};
    QMenu *m_editMenu{nullptr};

    QAction *m_actionUndo{nullptr};
    QAction *m_actionRedo{nullptr};
    QAction *m_actionToggleTabBar{nullptr};
    QAction *m_actionFullscreen{nullptr};
    QAction *m_actionZoomIn{nullptr};
    QAction *m_actionInvertColor{nullptr};
    QAction *m_actionFileProperties{nullptr};
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
    QAction *m_actionGotoPage{nullptr};
    QAction *m_actionFirstPage{nullptr};
    QAction *m_actionPrevPage{nullptr};
    QAction *m_actionNextPage{nullptr};
    QAction *m_actionLastPage{nullptr};
    QAction *m_actionPrevLocation{nullptr};
    QAction *m_actionAbout{nullptr};
    QAction *m_actionTextHighlight{nullptr};
    QAction *m_actionAnnotRect{nullptr};
    QAction *m_actionTextSelect{nullptr};
    QAction *m_actionAnnotEdit{nullptr};

    Config m_config;
    float m_dpr{1.0f};

    enum class LinkHintMode
    {
        None = 0,
        Visit,
        Copy
    };

    QString m_currentHintInput;
    bool m_link_hint_mode{false};
    StartupWidget *m_startup_widget{nullptr};
    LinkHintMode m_link_hint_current_mode{LinkHintMode::None};
    QMap<int, Model::LinkInfo> m_link_hint_map;
    DocumentView *m_doc{nullptr};
    TabWidget *m_tab_widget = new TabWidget();
    QVBoxLayout *m_layout   = new QVBoxLayout();
    OutlineWidget *m_owidget{nullptr};
    PropertiesWidget *m_propsWidget{nullptr};
    QMap<QString, std::function<void(const QStringList &args)>> m_actionMap;
    bool m_load_default_keybinding{true};
    QClipboard *m_clipboard = QGuiApplication::clipboard();
    QSqlDatabase m_last_pages_db;
    QString m_session_name;
    QFileSystemWatcher *m_fs_watcher{nullptr};
    QTimer m_debounceTimer;
    QMap<QString, DocumentView *> m_path_tab_map;
    CommandBar *m_command_bar;
    MessageBar *m_message_bar;
};
