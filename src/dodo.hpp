#pragma once

#include <QApplication>
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QFile>
#include <QGraphicsPixmapItem>
#include <QShortcut>
#include <QKeySequence>
#include <poppler/qt6/poppler-qt6.h>
#include <QCache>
#include <QScrollBar>
#include <QPainter>
#include <QThread>
#include <vector>
#include <QStandardPaths>
#include <QDir>
#include <QMap>
#include <QTimer>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>

#include "Panel.hpp"
#include "toml.hpp"
#include "RenderTask.hpp"

class dodo : public QMainWindow {
public:
    dodo() noexcept;
    ~dodo() noexcept;

    public slots:
    void handleRenderResult(int pageno, QImage img, bool lowQuality);

private:
    void initGui() noexcept;
    void initConfig() noexcept;
    void initKeybinds() noexcept;
    void gotoPage(const int &pageno) noexcept;
    void openFile(const QString &fileName) noexcept;

    // Interactive functions
    void OpenFile() noexcept;
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
    void FitToWidth() noexcept;
    void FitToHeight() noexcept;
    void RotateClock() noexcept;
    void RotateAntiClock() noexcept;

    void renderPage(const int &pageno,
                    const float &dpi,
                    const bool &lowQuality = true) noexcept;


    bool isPrefetchPage(int page, int currentPage) noexcept;
    void prefetchAround(int currentPage) noexcept;

    int m_pageno = -1, m_rotation = 0;

    double m_scale_factor = 1.0f;

    Panel *m_panel = new Panel(this);

    QString m_default_bg;

    float m_dpix = 300.0f,
    m_dpiy = 300.0f,
    m_total_pages = 0;

    const float LOW_DPI = 72.0;
    float DPI_FRAC;

    void updateUiEnabledState() noexcept;

    QAction *m_actionZoomIn = nullptr;
    QAction *m_actionZoomOut = nullptr;
    QAction *m_actionFitWidth = nullptr;
    QAction *m_actionFitHeight = nullptr;

    QAction *m_actionFirstPage = nullptr;
    QAction *m_actionPrevPage = nullptr;
    QAction *m_actionNextPage = nullptr;
    QAction *m_actionLastPage = nullptr;

    QCache<int, QPixmap> m_highResCache,
    m_pixmapCache;

    QThreadPool tp;

    QString m_filename;
    std::unique_ptr<Poppler::Document> m_document { nullptr };
    QGraphicsView *m_gview = new QGraphicsView();
    QGraphicsScene *m_gscene = new QGraphicsScene();
    QGraphicsPixmapItem *m_pix_item = new QGraphicsPixmapItem();
    QVBoxLayout *m_layout = new QVBoxLayout();
};

