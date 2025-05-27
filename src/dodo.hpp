#pragma once

#include "AboutDialog.hpp"
#include "BrowseLinkItem.hpp"
#include "EditLastPagesWidget.hpp"
#include "GraphicsView.hpp"
#include "Model.hpp"
#include "Panel.hpp"
#include "PropertiesWidget.hpp"
#include "RenderTask.hpp"
#include "argparse.hpp"
#include "toml.hpp"

#include <QActionGroup>
#include <QApplication>
#include <QCache>
#include <QClipboard>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QInputDialog>
#include <QKeySequence>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QSqlError>
#include <QStack>
#include <QStandardPaths>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>
#include <QtConcurrent/QtConcurrent>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <functional>
#include <qgraphicsitem.h>
#include <vector>

extern "C"
{
#include <synctex/synctex_parser.h>
#include <synctex/synctex_parser_utils.h>
#include <synctex/synctex_version.h>
}

#define __DODO_VERSION "0.2.0"

class dodo : public QMainWindow
{
public:
    dodo() noexcept;
    ~dodo() noexcept;

    inline QWindow* windowHandle() noexcept
    {
        return this->windowHandle();
    }
    void readArgsParser(argparse::ArgumentParser& argparser) noexcept;
    void construct() noexcept;

    struct CacheKey
    {
        int page;
        int rotation;
        float scale;

        bool operator==(const CacheKey& other) const
        {
            return page == other.page && rotation == other.rotation && qFuzzyCompare(scale, other.scale);
        }
    };

    struct CacheValue
    {
        QPixmap pixmap;
        QList<BrowseLinkItem*> links;
        QList<HighlightItem*> annot_highlights;

        CacheValue(const QPixmap& pix, const QList<BrowseLinkItem*>& lnks, QList<HighlightItem*> annoth)
            : pixmap(pix), links(lnks), annot_highlights(annoth)
        {
        }
    };

    enum class LinkHintMode
    {
        None = 0,
        Visit,
        Copy
    };

public slots:
    void handleRenderResult(int pageno, QImage img);

protected:
    void closeEvent(QCloseEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    enum class FitMode
    {
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
    void gotoPage(const int& pageno) noexcept;
    void gotoPageInternal(const int& pageno) noexcept;
    void openFile(const QString& fileName) noexcept;
    void search(const QString& term) noexcept;
    void renderPage(int pageno, bool renderonly = false) noexcept;
    void renderLinks(const QList<BrowseLinkItem*>& links) noexcept;
    void renderAnnotations(const QList<HighlightItem*>& annots) noexcept;
    void renderPixmap(const QPixmap& pix) noexcept;
    void scrollToXY(float x, float y) noexcept;
    void scrollToNormalizedTop(const double& top) noexcept;
    bool isPrefetchPage(int page, int currentPage) noexcept;
    void prefetchAround(int currentPage) noexcept;
    void setupKeybinding(const QString& action, const QString& key) noexcept;
    void setFitMode(const FitMode& mode) noexcept;
    void synctexLocateInFile(const char* texFile, int line) noexcept;
    void synctexLocateInPdf(const QString& texFile, int line, int column) noexcept;
    fz_point mapToPdf(QPointF loc) noexcept;
    void clearLinks() noexcept;
    void clearAnnots() noexcept;

    void clearPixmapItems() noexcept;

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
    void GoBackHistory() noexcept;
    void TableOfContents() noexcept;
    void ToggleRectAnnotation() noexcept;
    void ToggleAnnotSelect() noexcept;
    void SaveFile() noexcept;
    void SaveAsFile() noexcept;
    void VisitLinkKB() noexcept;
    void CopyLinkKB() noexcept;
    void GotoPage() noexcept;
    void TopOfThePage() noexcept;
    void InvertColor() noexcept;
    void ToggleAutoResize() noexcept;
    void ToggleMenubar() noexcept;
    void TogglePanel() noexcept;
    void ShowAbout() noexcept;
    void YankSelection() noexcept;
    void ToggleTextHighlight() noexcept;
    void TextSelectionMode() noexcept;
    void SelectAll() noexcept;

    QDir m_config_dir;
    bool m_prefetch_enabled, m_suppressHistory{false}, m_remember_last_visited;

    synctex_scanner_p m_synctex_scanner{nullptr};

    int m_pageno{-1}, m_start_page_override{-1}, m_rotation{0}, m_search_index{-1}, m_total_pages{0},
        m_search_hit_page{-1}, m_prefetch_distance, m_page_history_limit;
    QList<int> m_page_history_list;
    float m_scale_factor{1.0f}, m_zoom_by;
    QString m_last_search_term;
    Panel* m_panel{nullptr};
    float m_dpi;

    void updateUiEnabledState() noexcept;
    void jumpToHit(int page, int index);
    bool askForPassword() noexcept;
    void highlightSingleHit() noexcept;
    void highlightHitsInPage();
    void clearIndexHighlights();
    void clearHighlights();
    void prevHit();
    void nextHit();
    bool hasUpperCase(const QString& text) noexcept;
    void rehighlight() noexcept;
    void zoomHelper() noexcept;
    QRectF fzQuadToQRect(const fz_quad& q) noexcept;
    void clearTextSelection() noexcept;
    void selectAnnots() noexcept;
    void clearAnnotSelection() noexcept;
    void populateRecentFiles() noexcept;
    void editLastPages() noexcept;
    void deleteKeyAction() noexcept;
    void setDirty(bool state) noexcept;

    QMenuBar* m_menuBar{nullptr};
    QMenu* m_fitMenu{nullptr};
    QMenu* m_recentFilesMenu{nullptr};

    QAction* m_actionZoomIn{nullptr};
    QAction* m_actionInvertColor{nullptr};
    QAction* m_actionFileProperties{nullptr};
    QAction* m_actionSaveFile{nullptr};
    QAction* m_actionSaveAsFile{nullptr};
    QAction* m_actionCloseFile{nullptr};
    QAction* m_actionZoomOut{nullptr};
    QAction* m_actionFitWidth{nullptr};
    QAction* m_actionFitHeight{nullptr};
    QAction* m_actionFitWindow{nullptr};
    QAction* m_actionFitNone{nullptr};
    QAction* m_actionAutoresize{nullptr};
    QAction* m_actionToggleMenubar{nullptr};
    QAction* m_actionTogglePanel{nullptr};
    QAction* m_actionToggleOutline{nullptr};
    QAction* m_actionFirstPage{nullptr};
    QAction* m_actionPrevPage{nullptr};
    QAction* m_actionNextPage{nullptr};
    QAction* m_actionLastPage{nullptr};
    QAction* m_actionPrevLocation{nullptr};
    QAction* m_actionAbout{nullptr};
    QAction* m_actionTextHighlight{nullptr};
    QAction* m_actionAnnotRect{nullptr};
    QAction* m_actionTextSelect{nullptr};
    QAction* m_actionAnnotEdit{nullptr};

    QCache<CacheKey, CacheValue> m_cache;
    QMap<int, QList<Model::SearchResult>> m_searchRectMap;
    QString m_filename, m_basename;
    GraphicsView* m_gview           = new GraphicsView();
    QGraphicsScene* m_gscene        = new QGraphicsScene();
    QGraphicsPixmapItem* m_pix_item = new QGraphicsPixmapItem();
    QVBoxLayout* m_layout           = new QVBoxLayout();
    float m_dpr{1.0f}, m_inv_dpr{1.0f};

    Model* m_model{nullptr};
    QTimer* m_HQRenderTimer = new QTimer(this);
    QMap<QString, QRgb> m_colors;
    bool m_highlights_present, m_selection_present, m_annot_selection_present, m_full_file_path_in_panel,
        m_dirty{false};
    OutlineWidget* m_owidget{nullptr};
    PropertiesWidget* m_propsWidget{nullptr};
    QString m_currentHintInput;
    QMap<QString, Model::LinkInfo> m_link_hint_map;
    QMap<QString, QString> m_shortcuts_map;
    QMap<QString, std::function<void()>> m_actionMap;
    bool m_load_default_keybinding{true};
    bool m_auto_resize;
    FitMode m_fit_mode, m_initial_fit{FitMode::None};
    QString m_window_title;
    QString m_synctex_editor_command;
    QClipboard* m_clipboard = QGuiApplication::clipboard();
    QSet<int> m_selected_annots;
    bool m_link_boundary;
    QSqlDatabase m_last_pages_db;
    LinkHintMode m_link_hint_mode{LinkHintMode::None};
};
