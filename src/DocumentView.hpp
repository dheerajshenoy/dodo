#pragma once

#include "AboutDialog.hpp"
#include "Config.hpp"
#include "GraphicsPixmapItem.hpp"
#include "GraphicsScene.hpp"
#include "GraphicsView.hpp"
#include "JumpMarker.hpp"
#include "Model.hpp"
#include "VerticalScrollBar.hpp"

#ifdef HAS_SYNCTEX
extern "C"
{
#include <synctex/synctex_parser.h>
#include <synctex/synctex_parser_utils.h>
#include <synctex/synctex_version.h>
}
#endif

#include <QFileInfo>
#include <QGraphicsItem>
#include <QHash>
#include <QScrollBar>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <qevent.h>
#include <set>

#define ZVALUE_PAGE 0
#define ZVALUE_ANNOTATION 800
#define ZVALUE_TEXT_SELECTION 900
#define ZVALUE_LINK 1000
#define ZVALUE_JUMP_MARKER 1100
#define ZVALUE_SEARCH_HITS 1200

#define CSTR(x) x.toStdString().c_str()

class DocumentView : public QWidget
{
    Q_OBJECT
public:
    DocumentView(const QString &filepath, const Config &config,
                 QWidget *parent = nullptr) noexcept;

    enum class FitMode
    {
        None = 0,
        Width,
        Height,
        Window,
        COUNT
    };

    struct Location
    {
        int pageno{-1};
        float x{0.0f}, y{0.0f};
    };

    inline bool passwordRequired() const noexcept
    {
        return m_model->passwordRequired();
    }

    inline Model *model() const noexcept
    {
        return m_model;
    }

    inline GraphicsView *graphicsView() const noexcept
    {
        return m_gview;
    }

    inline GraphicsScene *graphicsScene() const noexcept
    {
        return m_gscene;
    }

    inline FitMode fitMode() const noexcept
    {
        return m_fit_mode;
    }

    inline GraphicsView::Mode selectionMode() const noexcept
    {
        return m_gview->mode();
    }

    inline int pageNo() const noexcept
    {
        return m_pageno;
    }

    inline int numPages() const noexcept
    {
        return m_model->numPages();
    }

    inline void setDPR(float dpr) noexcept
    {
        m_model->setDPR(dpr);
    }

    inline QString fileName() const noexcept
    {
        return QFileInfo(m_model->filePath()).fileName();
    }

    inline QString filePath() const noexcept
    {
        return m_model->filePath();
    }

    inline float dpr() const noexcept
    {
        return m_model->DPR();
    }

    inline bool authenticate(const QString &password) const noexcept
    {
        return m_model->authenticate(password);
    }

    inline void setInvertColor(bool invert) noexcept
    {
        m_model->setInvertColor(invert);
    }

    inline bool invertColor() const noexcept
    {
        return m_model->invertColor();
    }

    inline void setAutoReload(bool state) noexcept
    {
        m_auto_reload = state;
    }

    inline bool fileOpenedSuccessfully() const noexcept
    {
        return m_model->success();
    }

    inline bool autoReload() const noexcept
    {
        return m_auto_reload;
    }

    inline void setAutoResize(bool state) noexcept
    {
        m_auto_resize = state;
    }

    inline bool autoResize() const noexcept
    {
        return m_auto_resize;
    }

    inline double zoom() noexcept
    {
        return m_current_zoom;
    }

    inline double rotation() noexcept
    {
        return m_rotation;
    }

    inline void FollowLink(const Model::LinkInfo &info) noexcept
    {
        m_model->followLink(info);
    }

    void open() noexcept;
    bool EncryptDocument() noexcept;
    bool DecryptDocument() noexcept;
    void ReselectLastTextSelection() noexcept;

    void createAndAddPageItem(int pageno, const QPixmap &pixmap) noexcept;
    void renderVisiblePages() noexcept;
    void setFitMode(FitMode mode) noexcept;
    void GotoPage(int pageno) noexcept;
    void GotoLocation(int pageno, float x, float y) noexcept;
    void GotoNextPage() noexcept;
    void GotoPrevPage() noexcept;
    void GotoFirstPage() noexcept;
    void GotoLastPage() noexcept;
    void setZoom(double factor) noexcept;
    void Search(const QString &term) noexcept;
    void ZoomIn() noexcept;
    void ZoomOut() noexcept;
    void ZoomReset() noexcept;
    void NextHit() noexcept;
    void PrevHit() noexcept;
    void GotoHit(int index) noexcept;
    void ScrollLeft() noexcept;
    void ScrollRight() noexcept;
    void ScrollUp() noexcept;
    void ScrollDown() noexcept;
    void RotateClock() noexcept;
    void RotateAnticlock() noexcept;
    QMap<int, Model::LinkInfo> LinkKB() noexcept;
    void ClearTextSelection() noexcept;
    void YankSelection() noexcept;
    void FileProperties() noexcept;
    void SaveFile() noexcept;
    void SaveAsFile() noexcept;
    void CloseFile() noexcept;
    void ToggleAutoResize() noexcept;
    void ToggleTextHighlight() noexcept;
    void ToggleRegionSelect() noexcept;
    void ToggleAnnotRect() noexcept;
    void ToggleAnnotSelect() noexcept;
    void ToggleAnnotPopup() noexcept;
    void GoBackHistory() noexcept;
    void TextHighlightCurrentSelection() noexcept;
    void ClearKBHintsOverlay() noexcept;
    void NextSelectionMode() noexcept;
    void NextFitMode() noexcept;
    void resizeEvent(QResizeEvent *event) override;

signals:
    void pageChanged(int pageno);
    void zoomChanged(double factor);
    void fitModeChanged(FitMode mode);
    void selectionModeChanged(GraphicsView::Mode mode);
    void panelNameChanged(const QString &name);
    void fileNameChanged(const QString &name);
    void searchCountChanged(int count);
    void searchIndexChanged(int index);
    void totalPageCountChanged(int total);
    void clipboardContentChanged(const QString &content);
    void insertToDBRequested(const QString &filepath, int pageno);
    void highlightColorChanged(const QColor &color);
    void autoResizeActionUpdate(bool state);
    void currentPageChanged(int pageno);

public slots:
    void handleTextSelection(const QPointF &start, const QPointF &end) noexcept;
    void handleClickSelection(int clickType, const QPointF &scenePos) noexcept;
    void handleSearchResults(
        const QMap<int, std::vector<Model::SearchHit>> &results) noexcept;
#ifdef HAS_SYNCTEX
    void handleSynctexJumpRequested(const QPointF &scenePos) noexcept;
#endif

protected:
    void handleContextMenuRequested(const QPointF &scenePos) noexcept;

private:
    struct PendingJump
    {
        int pageno{-1};
        float x{0.0f}, y{0.0f};
    };

    struct HitRef
    {
        int page;
        int indexInPage;
    };

    inline int selectionPage() const noexcept
    {
        if (!m_selection_path_item)
            return -1;
        return m_selection_path_item->data(0).toInt();
    }

    Model *m_model{nullptr};
    GraphicsView *m_gview{nullptr};
    GraphicsScene *m_gscene{nullptr};
    Config m_config;
    FitMode m_fit_mode{FitMode::None};
    int m_pageno{-1};
    float m_spacing{10.0f}, m_page_stride{0.0f}, m_page_x_offset{0.0f};
    double m_target_zoom{1.0}, m_current_zoom{1.0}, m_rotation{0.0};
    bool m_auto_resize{false}, m_auto_reload{false};
    QScrollBar *m_hscroll{nullptr};
    VerticalScrollBar *m_vscroll{nullptr};
    QHash<int, GraphicsPixmapItem *> m_page_items_hash;
    QHash<int, std::vector<BrowseLinkItem *>> m_page_links_hash;
    QHash<int, std::vector<Annotation *>> m_page_annotations_hash;

    void setModified(bool state) noexcept;
    bool pageAtScenePos(const QPointF &scenePos, int &outPageIndex,
                        GraphicsPixmapItem *&outPageItem) const noexcept;
    void requestPageRender(int pageno) noexcept;
    void clearLinksForPage(int pageno) noexcept;
    void clearAnnotationsForPage(int pageno) noexcept;
    void clearSearchItemsForPage(int pageno) noexcept;
    void clearVisibleAnnotations() noexcept;
    void clearVisiblePages() noexcept;
    void clearVisibleLinks() noexcept;
    void renderPageFromImage(int pageno, const QImage &image) noexcept;
    void renderLinks(int pageno,
                     const std::vector<BrowseLinkItem *> &links) noexcept;
    void renderAnnotations(int pageno,
                           const std::vector<Annotation *> &annots) noexcept;
    void buildFlatSearchHitIndex() noexcept;
    void removeUnusedPageItems(const std::set<int> &visiblePages) noexcept;
    void reloadPage(int pageno) noexcept;
    void clearDocumentItems() noexcept;
    void updateCurrentPage() noexcept;
    void updateCurrentHitHighlight() noexcept;
    void zoomHelper() noexcept;
    void cachePageStride() noexcept;
    void updateSceneRect() noexcept;
    void initConnections() noexcept;
    std::set<int> getVisiblePages() noexcept;
    void removePageItem(int pageno) noexcept;
    void renderSearchHitsForPage(int pageno) noexcept;
    void renderSearchHitsInScrollbar() noexcept;
    void clearSearchHits() noexcept;
    QGraphicsPathItem *ensureSearchItemForPage(int pageno) noexcept;
    QGraphicsPathItem *m_current_search_hit_item{nullptr};
    void updateSelectionPath(int pageno, std::vector<QPolygonF> quads) noexcept;
    void centerOnPage(int pageno) noexcept;
    void OpenHitPixmapInExternalViewer() noexcept;
    void SaveImageAs() noexcept;
    void CopyImageToClipboard() noexcept;

#ifdef HAS_SYNCTEX
    void initSynctex() noexcept;
    void synctexLocateInDocument(const char *fileName, int line) noexcept;
#endif

    float m_old_y{0.0f};
    JumpMarker *m_jump_marker{nullptr};
    QTimer *m_scroll_page_update_timer{nullptr};
    PendingJump m_pending_jump;

    int m_search_index{-1};
    QMap<int, std::vector<Model::SearchHit>> m_search_hits;
    std::vector<HitRef> m_search_hit_flat_refs;
    QHash<int, QGraphicsPathItem *> m_search_items;

    QPointF m_selection_start, m_selection_end;
    int m_last_selection_page{-1};
    QGraphicsPathItem *m_selection_path_item{nullptr};
    QTimer *m_hq_render_timer{nullptr};
    std::vector<Location> m_loc_history;
    bool m_is_modified{false};

#ifdef HAS_SYNCTEX
    synctex_scanner_p m_synctex_scanner;
#endif
};
