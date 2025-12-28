#pragma once

#include "AboutDialog.hpp"
#include "Config.hpp"
#include "GraphicsPixmapItem.hpp"
#include "GraphicsScene.hpp"
#include "GraphicsView.hpp"
#include "Model.hpp"

#include <QFileInfo>
#include <QHash>
#include <QScrollBar>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <qevent.h>

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

    inline bool authenticate(const QString &password) noexcept
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

    void renderVisiblePages(bool force = false) noexcept;
    void setFitMode(FitMode mode) noexcept;
    void GotoPage(int pageno) noexcept;
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
    void recenterPages() noexcept;

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

private:
    Model *m_model{nullptr};
    GraphicsView *m_gview{nullptr};
    GraphicsScene *m_gscene{nullptr};
    Config m_config;
    FitMode m_fit_mode{FitMode::None};
    int m_pageno{-1};
    float m_spacing{1.0f}, m_page_stride{0.0f};
    double m_target_zoom{1.0}, m_current_zoom{1.0}, m_rotation{0.0};
    bool m_auto_resize{false}, m_auto_reload{false};
    QHash<int, GraphicsPixmapItem *> m_page_items_hash;
    QScrollBar *m_hscroll{nullptr}, *m_vscroll{nullptr};

    void zoomHelper() noexcept;
    void scheduleHighQualityRender() noexcept;
    void cachePageStride() noexcept;
    void updateSceneRect() noexcept;
    void initConnections() noexcept;
    std::vector<int> getVisiblePages() noexcept;
    void renderPage(int pageno, bool force = false) noexcept;
    void removePageItem(int pageno) noexcept;
    QTimer *m_high_quality_render_timer{nullptr};
};
