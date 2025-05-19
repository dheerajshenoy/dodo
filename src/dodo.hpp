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

#include "Panel.hpp"
#include "RenderWorker.hpp"
#include "toml.hpp"

class dodo : public QMainWindow {
public:
    dodo() noexcept;
    ~dodo() noexcept;

private:
    void initGui() noexcept;
    void initConfig() noexcept;
    void initKeybinds() noexcept;
    void gotoPage(const int &pageno) noexcept;
    void openFile(const QString &fileName) noexcept;

    void firstPage() noexcept;
    void lastPage() noexcept;
    void nextPage() noexcept;
    void prevPage() noexcept;
    void scrollDown() noexcept;
    void scrollUp() noexcept;
    void scrollLeft() noexcept;
    void scrollRight() noexcept;
    void renderPage(const int &pageno,
                    const float &dpi,
                    const bool &lowQuality = true) noexcept;

    void zoomIn() noexcept;
    void zoomOut() noexcept;

    bool isPrefetchPage(int page, int currentPage) noexcept;
    void prefetchAround(int currentPage) noexcept;

    int m_pageno = -1;

    double m_scale_factor = 1.0f;

    Panel *m_panel = new Panel(this);

    QString m_default_bg;

    float m_dpix = 300.0f,
    m_dpiy = 300.0f,
    m_total_pages = 0;

    const float LOW_DPI = 72.0;
    float DPI_FRAC;

    std::atomic<int> m_latestRequest = 0;
    const int MAX_WORKERS = QThread::idealThreadCount(); // Or fixed number

    QCache<int, QPixmap> m_highResCache;
    QCache<int, QPixmap> m_pixmapCache;

    QString m_filename;
    std::unique_ptr<Poppler::Document> m_document { nullptr };
    QGraphicsView *m_gview = new QGraphicsView();
    QGraphicsScene *m_gscene = new QGraphicsScene();
    QGraphicsPixmapItem *m_pix_item = new QGraphicsPixmapItem();
    QVBoxLayout *m_layout = new QVBoxLayout();
};

