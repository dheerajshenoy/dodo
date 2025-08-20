#pragma once

#include "Annotation.hpp"
#include "BrowseLinkItem.hpp"
#include "Config.hpp"
#include "GraphicsScene.hpp"
#include "GraphicsView.hpp"
#include "HighlightItem.hpp"
#include "JumpMarker.hpp"
#include "LinkHint.hpp"
#include "Model.hpp"
#include "OutlineWidget.hpp"
#include "PropertiesWidget.hpp"

#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QWidget>
#include <QWindow>
#include <qboxlayout.h>
#include <qfilesystemwatcher.h>

extern "C"
{
#include <mupdf/fitz/geometry.h>
#include <synctex/synctex_parser.h>
#include <synctex/synctex_parser_utils.h>
#include <synctex/synctex_version.h>
}

class DocumentView : public QWidget
{
    Q_OBJECT
public:
    DocumentView(const QString &fileName, const Config &config,
                 QWidget *parent = nullptr);
    ~DocumentView();

    enum class FitMode
    {
        None = 0,
        Width,
        Height,
        Window,
    };

    // For use with text annotations click
    struct TextHighlight
    {
        QString content;
        int pageno;
        // BrowseLinkItem::Location location; // TODO:
    };

    struct CacheKey
    {
        int page;
        int rotation;
        float scale;

        bool operator==(const CacheKey &other) const
        {
            return page == other.page && rotation == other.rotation
                   && qFuzzyCompare(scale, other.scale);
        }
    };

    struct CacheValue
    {
        QPixmap pixmap;
        QList<BrowseLinkItem *> links;
        QList<Annotation *> annot_highlights;

        CacheValue(const QPixmap &pix, const QList<BrowseLinkItem *> &lnks,
                   QList<Annotation *> annoth)
            : pixmap(pix), links(lnks), annot_highlights(annoth)
        {
        }
    };

    struct HistoryLocation
    {
        float x, y, zoom;
        int pageno;
    };

    bool autoResize() noexcept
    {
        return m_auto_resize;
    }

    void setDPR(qreal DPR) noexcept;
    void CloseFile() noexcept;
    void FileProperties() noexcept;
    bool FirstPage() noexcept;
    bool LastPage() noexcept;
    bool NextPage() noexcept;
    bool PrevPage() noexcept;
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
    void Search(const QString &term) noexcept;
    void GoBackHistory() noexcept;
    void TableOfContents() noexcept;
    void ToggleRectAnnotation() noexcept;
    void ToggleAnnotSelect() noexcept;
    void SaveFile() noexcept;
    void SaveAsFile() noexcept;
    QMap<int, Model::LinkInfo> LinkKB() noexcept;
    void GotoPage(int pageno) noexcept;
    void TopOfThePage() noexcept;
    void InvertColor() noexcept;
    void ToggleAutoResize() noexcept;
    void YankSelection() noexcept;
    void ToggleTextHighlight() noexcept;
    void TextHighlightCurrentSelection() noexcept;
    void TextSelectionMode() noexcept;
    void YankAll() noexcept;
    void PrevHit();
    void NextHit();
    void ClearTextSelection() noexcept;

    inline QDateTime lastModified() noexcept
    {
        return m_last_modified_time;
    }

    inline Model *model() noexcept
    {
        return m_model;
    }

    inline void setLinkHintForeground(const QColor &fg) noexcept
    {
        m_link_hint_fg = fg;
    }

    inline void setLinkHintBackground(const QColor &bg) noexcept
    {
        m_link_hint_bg = bg;
    }

    // inline QString windowTitle() noexcept { return
    // m_config.window_title_format; }

    inline QString fileName() noexcept
    {
        return m_filename;
    }

    inline int pageNo() noexcept
    {
        return m_pageno + 1;
    }
    inline GraphicsView *gview() noexcept
    {
        return m_gview;
    }
    inline GraphicsView::Mode selectionMode() noexcept
    {
        return m_gview->mode();
    }
    inline FitMode fitMode() noexcept
    {
        return m_fit_mode;
    }
    inline float zoom() noexcept
    {
        return m_model->zoom();
    }
    void Fit(DocumentView::FitMode mode) noexcept;
    inline float rotation() noexcept
    {
        return m_rotation;
    }

    void clearKBHintsOverlay() noexcept;
    void reloadDocument() noexcept;

    inline void nextSelectionMode() noexcept
    {
        GraphicsView::Mode nextMode = m_gview->getNextMode();
        m_gview->setMode(nextMode);
        emit selectionModeChanged(nextMode);
    }

signals:
    void pageNumberChanged(int pageno);
    void searchCountChanged(int count);
    void searchModeChanged(bool state);
    void searchIndexChanged(int index);
    void fileNameChanged(const QString &fname);
    void totalPageCountChanged(int count);
    void clipboardContentChanged(const QString &text);
    void fitModeChanged(FitMode mode);
    void selectionModeChanged(GraphicsView::Mode mode);
    void highlightColorChanged(const QColor &color);
    void panelNameChanged(const QString &name);
    void invertColorActionUpdate(bool state);
    void autoResizeActionUpdate(bool state);
    void insertToDBRequested(const QString &filepath, int pageno);

protected:
    void closeEvent(QCloseEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    void initDefaults() noexcept;
    void initSynctex() noexcept;
    void synctexLocateInFile(const char *texFile, int line) noexcept;
    void synctexLocateInPdf(const QString &texFile, int line,
                            int column) noexcept;
    void initConnections() noexcept;
    void openFile(const QString &fileName) noexcept;
    bool gotoPage(int pageno, bool refresh = true) noexcept;
    bool gotoPageInternal(int pageno, bool refresh = true) noexcept;
    void search(const QString &term) noexcept;
    void setFitMode(const FitMode &mode) noexcept;
    bool renderPage(int pageno, bool refresh = true) noexcept;
    void renderLinkHints() noexcept;
    void renderLinks(const QList<BrowseLinkItem *> &links) noexcept;
    void renderPixmap(const QPixmap &pix) noexcept;
    void renderAnnotations(const QList<Annotation *> &annots) noexcept;

    void scrollToXY(float x, float y) noexcept;
    void scrollToNormalizedTop(double top) noexcept;
    fz_point mapToPdf(QPointF loc) noexcept;
    void clearLinks() noexcept;
    void clearAnnots() noexcept;
    void clearPixmapItems() noexcept;

    void jumpToHit(int page, int index);
    bool askForPassword() noexcept;
    void highlightSingleHit() noexcept;
    void highlightHitsInPage();
    void clearIndexHighlights();
    void clearHighlights();
    bool hasUpperCase(const QString &text) noexcept;
    void rehighlight() noexcept;
    void zoomHelper() noexcept;
    QRectF fzQuadToQRect(const fz_quad &q) noexcept;
    void selectAnnots() noexcept;
    void clearAnnotSelection() noexcept;
    void deleteKeyAction() noexcept;
    void setDirty(bool state) noexcept;
    void fadeJumpMarker(JumpMarker *item) noexcept;
    void populateContextMenu(QMenu *menu) noexcept;
    void annotChangeColor() noexcept;
    void changeHighlighterColor() noexcept;
    void changeAnnotRectColor() noexcept;
    void mouseWheelScrollRequested(int direction) noexcept;
    void synctexJumpRequested(const QPointF &loc) noexcept;
    void showJumpMarker(const QPointF &p) noexcept;
    void showJumpMarker(const fz_point &p) noexcept;

    PropertiesWidget *m_propsWidget{nullptr};
    OutlineWidget *m_owidget{nullptr};
    QCache<CacheKey, CacheValue> m_cache;
    QMap<int, QList<Model::SearchResult>> m_searchRectMap{};
    QString m_filename, m_basename;
    Model *m_model{nullptr};
    Config m_config;
    QVBoxLayout *m_layout{new QVBoxLayout};
    GraphicsView *m_gview{new GraphicsView};
    GraphicsScene *m_gscene{new GraphicsScene};
    QGraphicsPixmapItem *m_pix_item{new QGraphicsPixmapItem};
    synctex_scanner_p m_synctex_scanner{nullptr};
    int m_pageno{-1}, m_start_page_override{-1}, m_rotation{0},
        m_search_index{-1}, m_total_pages{0}, m_search_hit_page{-1},
        m_page_history_limit{5};
    float m_default_zoom{0.0}, m_zoom_by{1.25};
    bool m_suppressHistory{false}, m_highlights_present{false},
        m_selection_present{false}, m_annot_selection_present{false},
        m_dirty{false}, m_page_nav_with_mouse{true}, m_jump_marker_shown{true},
        m_auto_resize{false};
    float m_dpr{1.0f}, m_inv_dpr{1.0f};
    QString m_last_search_term;
    QSet<int> m_selected_annots;
    QList<HistoryLocation> m_page_history_list;
    JumpMarker *m_jump_marker{nullptr};
    FitMode m_fit_mode, m_initial_fit{FitMode::None};
    QScrollBar *m_vscrollbar{nullptr};
    QScrollBar *m_hscrollbar{nullptr};
    bool m_link_hints_present{false};
    QColor m_link_hint_fg{QColor::fromRgba(0x000000)},
        m_link_hint_bg{QColor::fromRgba(0xFFFF00)};
    QList<LinkHint *> m_link_hints{};
    QList<BrowseLinkItem *> m_link_items{};

    int m_scroll_accumulator{0};
    QElapsedTimer m_scroll_cooldown;
    QDateTime m_last_modified_time;
    const int kScrollCooldownMs{300}; // Prevent rapid-fire page turns
    const int kPageThreshold{300};
    QList<TextHighlight>
        m_highlight_text_list{}; // Holds the text of all highlight annotations
};
