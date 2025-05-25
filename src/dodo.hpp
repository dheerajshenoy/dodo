#pragma once

#include <QApplication>
#include <QWidget>
#include <QGraphicsScene>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QFile>
#include <QGraphicsPixmapItem>
#include <QShortcut>
#include <QKeySequence>
#include <QStack>
#include <QCache>
#include <QScrollBar>
#include <QPainter>
#include <QThread>
#include <QStandardPaths>
#include <QDir>
#include <QMap>
#include <QTimer>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
#include <QInputDialog>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QCloseEvent>
#include <QMessageBox>
#include <qgraphicsitem.h>
#include <QResizeEvent>
#include <vector>
#include <functional>
#include <QActionGroup>
#include <QWindow>
#include <QSqlError>

#include "argparse.hpp"

#include "AboutDialog.hpp"
#include "GraphicsView.hpp"
#include "Panel.hpp"
#include "toml.hpp"
#include "RenderTask.hpp"
#include "BrowseLinkItem.hpp"
#include "Model.hpp"
#include "PropertiesWidget.hpp"

extern "C" {
#include <synctex/synctex_version.h>
#include <synctex/synctex_parser.h>
#include <synctex/synctex_parser_utils.h>
}

#define __DODO_VERSION "0.2.0"

class dodo : public QMainWindow {
public:
    dodo() noexcept;
    ~dodo() noexcept;

    inline QWindow* windowHandle() noexcept { return this->windowHandle(); }
    void readArgsParser(argparse::ArgumentParser &argparser) noexcept;
    void construct() noexcept;

public slots:
    void handleRenderResult(int pageno, QImage img);

protected:
    void closeEvent(QCloseEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *e) override;

private:

    enum class FitMode {
        None = 0,
        Width,
        Height,
        Window,
    };


    void initSynctex() noexcept;
    void initConnections() noexcept;
    void initMenubar() noexcept;
    void initDB() noexcept;
    void initGui() noexcept;
    void initConfig() noexcept;
    void initKeybinds() noexcept;
    void gotoPage(const int &pageno) noexcept;
    void gotoPageInternal(const int &pageno) noexcept;
    void openFile(const QString &fileName) noexcept;
    void renderLinks() noexcept;
    void search(const QString &term) noexcept;
    void renderPage(int pageno, bool renderonly = false) noexcept;
    void renderPixmap(const QPixmap &pix) noexcept;
    void cachePage(int pageno) noexcept;
    void scrollToXY(float x, float y) noexcept;
    void scrollToNormalizedTop(const double &top) noexcept;
    bool isPrefetchPage(int page, int currentPage) noexcept;
    void prefetchAround(int currentPage) noexcept;
    void setupKeybinding(const QString &action,
                         const QString &key) noexcept;
    void setFitMode(const FitMode &mode) noexcept;
    void synctexLocateInFile(const char *texFile, int line) noexcept;
    void synctexLocateInPdf(const char *texFile, int line, int column) noexcept;
    fz_point mapToPdf(QPointF loc) noexcept;

    // Interactive functions
    void ToggleFullscreen() noexcept;
    void OpenFile() noexcept;
    void CloseFile() noexcept;
    void FileProperties() noexcept;
    void FirstPage() noexcept;
    void LastPage() noexcept;
    void NextPage() noexcept;
    void PrevPage() noexcept;
    void ScrollDown() noexcept;
    void ScrollUp() noexcept;
    void ScrollLeft() noexcept;
    void ScrollRight() noexcept;
    void ZoomReset() noexcept;
    void ZoomIn() noexcept;
    void ZoomOut() noexcept;
    void Zoom(float factor) noexcept;
    void FitNone() noexcept;
    void FitWidth() noexcept;
    void FitHeight() noexcept;
    void FitWindow() noexcept;
    void RotateClock() noexcept;
    void RotateAntiClock() noexcept;
    void Search() noexcept;
    void SearchPage() noexcept;
    void GoBackHistory() noexcept;
    void TableOfContents() noexcept;
    void ToggleHighlight() noexcept;
    void SaveFile() noexcept;
    void VisitLinkKB() noexcept;
    void CopyLinkKB() noexcept;
    void GotoPage() noexcept;
    void TopOfThePage() noexcept;
    void InvertColor() noexcept;
    void ToggleAutoResize() noexcept;
    void ToggleMenubar() noexcept;
    void TogglePanel() noexcept;
    void ShowAbout() noexcept;

    QDir m_config_dir;
    bool m_prefetch_enabled,
    m_suppressHistory,
    m_remember_last_visited;

    synctex_scanner_p m_synctex_scanner { nullptr };

    int m_pageno { -1 },
    m_start_page_override { -1 },
    m_rotation { 0 },
    m_search_index { -1 },
    m_total_pages { 0 },
    m_search_hit_page { -1 },
    m_prefetch_distance,
    m_page_history_limit;
    QList<int> m_page_history_list;
    float m_scale_factor { 1.0f }, m_zoom_by;
    QString m_last_search_term;
    Panel *m_panel { nullptr };
    float m_dpi,
    m_low_dpi { 72.0f },
    DPI_FRAC;

    void updateUiEnabledState() noexcept;
    void jumpToHit(int page, int index);
    bool askForPassword() noexcept;
    void highlightSingleHit() noexcept;
    void highlightHitsInPage();
    void clearIndexHighlights();
    void clearHighlights();
    void prevHit();
    void nextHit();
    bool hasUpperCase(const QString &text) noexcept;
    void rehighlight() noexcept;
    void zoomHelper() noexcept;
    QRectF fzQuadToQRect(const fz_quad &q) noexcept;

    QMenuBar *m_menuBar { nullptr };
    QMenu *m_fitMenu { nullptr };

    QAction *m_actionZoomIn { nullptr };
    QAction *m_actionFileProperties { nullptr };
    QAction *m_actionCloseFile { nullptr };
    QAction *m_actionZoomOut { nullptr };
    QAction *m_actionFitWidth { nullptr };
    QAction *m_actionFitHeight { nullptr };
    QAction *m_actionFitWindow { nullptr };
    QAction *m_actionFitNone { nullptr };
    QAction *m_actionAutoresize { nullptr };
    QAction *m_actionToggleMenubar { nullptr };
    QAction *m_actionTogglePanel { nullptr };
    QAction *m_actionToggleOutline { nullptr };
    QAction *m_actionFirstPage { nullptr };
    QAction *m_actionPrevPage { nullptr };
    QAction *m_actionNextPage { nullptr };
    QAction *m_actionLastPage { nullptr };
    QAction *m_actionPrevLocation { nullptr };
    QAction *m_actionAbout { nullptr };

    QCache<int, QPixmap> m_highResCache,
    m_pixmapCache;

    QThreadPool tp;
    QMap<int, QList<Model::SearchResult>> m_searchRectMap;

    QString m_filename;
    GraphicsView *m_gview = new GraphicsView();
    QGraphicsScene *m_gscene = new QGraphicsScene();
    QGraphicsPixmapItem *m_pix_item = new QGraphicsPixmapItem();
    QVBoxLayout *m_layout = new QVBoxLayout();
    float m_dpr { 1.0f }, m_inv_dpr { 1.0f };

    Model *m_model { nullptr };
    QTimer *m_HQRenderTimer = new QTimer(this);
    QMap<QString, QRgb> m_colors;
    bool m_highlights_present;
    bool m_linkHintMode { false };
    bool m_full_file_path_in_panel;
    OutlineWidget *m_owidget { nullptr };
    PropertiesWidget *m_propsWidget { nullptr };
    QString m_currentHintInput;
    QMap<QString, Model::LinkInfo> m_link_hint_map;
    QMap<QString, QString> m_shortcuts_map;
    QMap<QString, std::function<void()>> m_actionMap;
    bool m_load_default_keybinding { true };
    bool m_auto_resize;
    FitMode m_fit_mode, m_initial_fit { FitMode::None };
    QString m_window_title;
    QString m_synctex_editor_command;
};

