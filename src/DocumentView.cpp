#include "DocumentView.hpp"

#include "GraphicsView.hpp"
#include "utils.hpp"

#include <QVBoxLayout>
#include <algorithm>
#include <qnamespace.h>
#include <qtextcursor.h>

DocumentView::DocumentView(const QString &filepath, const Config &config,
                           QWidget *parent) noexcept
    : QWidget(parent), m_config(config)
{
    m_model = new Model(filepath);
    m_model->open();

    m_high_quality_render_timer = new QTimer(this);
    m_high_quality_render_timer->setSingleShot(true);
    m_high_quality_render_timer->setInterval(200);

    m_gview  = new GraphicsView(this);
    m_gscene = new GraphicsScene(m_gview);
    m_gview->setScene(m_gscene);

    m_gview->setAlignment(Qt::AlignCenter);

    m_hscroll = m_gview->horizontalScrollBar();
    m_vscroll = m_gview->verticalScrollBar();

    initConnections();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_gview);

    cachePageStride();
    renderVisiblePages();
}

void
DocumentView::initConnections() noexcept
{
    connect(m_vscroll, &QScrollBar::valueChanged, this,
            [this](int) { scheduleHighQualityRender(); });

    connect(m_high_quality_render_timer, &QTimer::timeout, this, [&]()
    {
        renderVisiblePages(true);
        m_current_zoom = m_target_zoom;
    });

    connect(m_gview, &GraphicsView::textSelectionDeletionRequested, this, [&]()
    {
        // Remove existing selection items from scene
        for (QGraphicsItem *item : m_text_selection_items)
        {
            m_gscene->removeItem(item);
            delete item;
        }
        m_text_selection_items.clear();
    });

    connect(m_gview, &GraphicsView::textSelectionRequested, this,
            &DocumentView::handleTextSelection);
}

void
DocumentView::handleTextSelection(const QPointF &start,
                                  const QPointF &end) noexcept
{
    int pageIndex           = -1;
    QGraphicsItem *pageItem = nullptr;

    if (!pageAtScenePos(start, pageIndex, pageItem))
        return; // selection start outside visible pages?

    // 🔴 CRITICAL FIX: map to page-local coordinates
    const QPointF pageStart = pageItem->mapFromScene(start);
    const QPointF pageEnd   = pageItem->mapFromScene(end);

    const std::vector<QPolygonF> quads
        = m_model->computeTextSelectionQuad(pageIndex, pageStart, pageEnd);

    // Add to scene
    const QBrush brush(Qt::yellow);
    for (const QPolygonF &poly : quads)
    {
        QPolygonF scenePoly = pageItem->mapToScene(poly);

        auto *item = m_gscene->addPolygon(scenePoly, Qt::NoPen, brush);
        item->setFlag(QGraphicsItem::ItemIsSelectable, false);
        item->setFlag(QGraphicsItem::ItemIgnoresTransformations, false);
        item->setData(0, "selection");
        item->setData(1, pageIndex);
        m_text_selection_items.push_back(item);
    }
}

void
DocumentView::RotateClock() noexcept
{
    m_rotation += 90;
    if (m_rotation >= 360)
        m_rotation = 0;
}

void
DocumentView::RotateAnticlock() noexcept
{
    m_rotation -= 90;
    if (m_rotation < 0)
        m_rotation = 270;
}

void
DocumentView::NextFitMode() noexcept
{
    FitMode nextMode = static_cast<FitMode>((static_cast<int>(m_fit_mode) + 1)
                                            % static_cast<int>(FitMode::COUNT));
    m_fit_mode       = nextMode;
    setFitMode(nextMode);
    fitModeChanged(nextMode);
    // Fit(static_cast<FitMode>(nextMode));
}

void
DocumentView::NextSelectionMode() noexcept
{
    GraphicsView::Mode nextMode = m_gview->getNextMode();
    m_gview->setMode(nextMode);
    emit selectionModeChanged(nextMode);
}

void
DocumentView::setFitMode(FitMode mode) noexcept
{
    m_fit_mode = mode;

    switch (mode)
    {
        case FitMode::None:
            break;

        case FitMode::Width:
        {
            double viewWidth = m_gview->viewport()->width();
            // Calculate zoom needed to make page width match view width
            double newZoom = (viewWidth / m_model->pageWidthPts());
            setZoom(newZoom);
            m_model->setZoom(newZoom);
        }
        break;

        case FitMode::Height:
            break;

        case FitMode::Window:
            break;

        default:
            break;
    }
}

void
DocumentView::setZoom(double factor) noexcept
{
    m_current_zoom = factor;
    m_target_zoom  = factor;
}

void
DocumentView::GotoPage(int pageno) noexcept
{
}

void
DocumentView::GotoNextPage() noexcept
{
}

void
DocumentView::GotoPrevPage() noexcept
{
}

void
DocumentView::Search(const QString &term) noexcept
{
}

// Function that is common to zoom-in and zoom-out
void
DocumentView::zoomHelper() noexcept
{
    m_model->setZoom(m_current_zoom);
    cachePageStride();
    scheduleHighQualityRender();
}

void
DocumentView::ZoomIn() noexcept
{
    m_target_zoom = m_current_zoom * 1.2;
    zoomHelper();
}

void
DocumentView::ZoomOut() noexcept
{
    m_target_zoom = m_current_zoom / 1.2;
    zoomHelper();
}

void
DocumentView::ZoomReset() noexcept
{
    m_current_zoom = 1.0f;
    m_target_zoom  = 1.0f;
    m_model->setZoom(m_current_zoom);
    renderVisiblePages();
}

void
DocumentView::NextHit() noexcept
{
}

void
DocumentView::PrevHit() noexcept
{
}

void
DocumentView::GotoHit(int index) noexcept
{
}

void
DocumentView::ScrollLeft() noexcept
{
    m_hscroll->setValue(m_hscroll->value() - 50);
}

void
DocumentView::ScrollRight() noexcept
{
    m_hscroll->setValue(m_hscroll->value() + 50);
}

void
DocumentView::ScrollUp() noexcept
{
    m_vscroll->setValue(m_vscroll->value() - 50);
}

void
DocumentView::ScrollDown() noexcept
{
    m_vscroll->setValue(m_vscroll->value() + 50);
}

QMap<int, Model::LinkInfo>
DocumentView::LinkKB() noexcept
{
}

void
DocumentView::FileProperties() noexcept
{
}

void
DocumentView::SaveFile() noexcept
{
}

void
DocumentView::SaveAsFile() noexcept
{
}

void
DocumentView::CloseFile() noexcept
{
}

void
DocumentView::ToggleAutoResize() noexcept
{
}

void
DocumentView::ToggleTextHighlight() noexcept
{
}

void
DocumentView::ToggleRegionSelect() noexcept
{
}

void
DocumentView::ToggleAnnotRect() noexcept
{
}

void
DocumentView::ToggleAnnotSelect() noexcept
{
}

void
DocumentView::ToggleAnnotPopup() noexcept
{
}

void
DocumentView::TextHighlightCurrentSelection() noexcept
{
}

void
DocumentView::ClearKBHintsOverlay() noexcept
{
}

void
DocumentView::ClearTextSelection() noexcept
{
}

void
DocumentView::YankSelection() noexcept
{
}

void
DocumentView::GotoFirstPage() noexcept
{
}

void
DocumentView::GotoLastPage() noexcept
{
}

void
DocumentView::GoBackHistory() noexcept
{
}

std::vector<int>
DocumentView::getVisiblePages() noexcept
{
    std::vector<int> visiblePages;

    QRectF visibleSceneRect
        = m_gview->mapToScene(m_gview->viewport()->rect()).boundingRect();
    double top    = visibleSceneRect.top();
    double bottom = visibleSceneRect.bottom();

    int firstPage = static_cast<int>(std::floor(top / m_page_stride));
    int lastPage  = static_cast<int>(std::floor(bottom / m_page_stride));

    firstPage = std::max(0, firstPage);
    lastPage  = std::min(m_model->numPages() - 1, lastPage);

    for (int pageno = firstPage; pageno <= lastPage; ++pageno)
        visiblePages.push_back(pageno);

    return visiblePages;
}

void
DocumentView::renderPage(int pageno, bool force) noexcept
{
    if (force == false && m_page_items_hash.contains(pageno))
        return;

    removePageItem(pageno);
    const QPixmap pix = m_model->renderPageToPixmap(pageno);
    auto *item        = new GraphicsPixmapItem();
    item->setPixmap(pix);
    item->setScale(1.0);
    double pageWidthLogical = pix.width() / pix.devicePixelRatio();
    double xOffset = (m_gview->sceneRect().width() - pageWidthLogical) / 2.0;
    item->setPos(xOffset, pageno * m_page_stride);
    m_gscene->addItem(item);
    m_page_items_hash[pageno] = item;
}

void
DocumentView::renderVisiblePages(bool force) noexcept
{
    std::vector<int> visiblePages = getVisiblePages();
    QSet<int> visibleSet;
    for (int pageno : visiblePages)
    {
        visibleSet.insert(pageno);
        renderPage(pageno, force);
    }

    for (auto it = m_page_items_hash.begin(); it != m_page_items_hash.end();)
    {
        if (!visibleSet.contains(it.key()))
        {
            m_gscene->removeItem(it.value());
            delete it.value();
            it = m_page_items_hash.erase(it);
        }
        else
        {
            ++it;
        }
    }

    updateSceneRect();
}

void
DocumentView::removePageItem(int pageno) noexcept
{
    if (m_page_items_hash.contains(pageno))
    {
        GraphicsPixmapItem *item = m_page_items_hash[pageno];
        m_gscene->removeItem(item);
        delete item;
        m_page_items_hash.remove(pageno);
    }
}

void
DocumentView::cachePageStride() noexcept
{
    m_page_stride = (m_model->pageHeightPts() * m_model->DPR()
                     + m_spacing * m_model->DPI())
                    * m_current_zoom;
}

void
DocumentView::updateSceneRect() noexcept
{
    double totalHeight = m_model->numPages() * m_page_stride;
    double width       = m_gview->viewport()->width();
    m_gview->setSceneRect(0, 0, width, totalHeight);
}

void
DocumentView::resizeEvent(QResizeEvent *event)
{
    updateSceneRect();
    recenterPages();
    QWidget::resizeEvent(event);
}

// Recenter pages in the view
void
DocumentView::recenterPages() noexcept
{
    for (auto it = m_page_items_hash.begin(); it != m_page_items_hash.end();
         ++it)
    {
        GraphicsPixmapItem *item = it.value();
        double pageWidthLogical
            = item->pixmap().width() / item->pixmap().devicePixelRatio();
        double xOffset
            = (m_gview->sceneRect().width() - pageWidthLogical) / 2.0;
        item->setPos(xOffset, it.key() * m_page_stride);
    }
}

void
DocumentView::scheduleHighQualityRender() noexcept
{
    std::vector<int> visiblePages = getVisiblePages();

    // Upscale the GraphicsPixmapItems for visible pages using setScale
    // temporarily

    for (int pageno : visiblePages)
    {
        if (m_page_items_hash.contains(pageno))
        {
            GraphicsPixmapItem *item = m_page_items_hash[pageno];
            QSizeF size              = item->pixmap().size() / m_model->DPR();
            item->setTransformOriginPoint(size.width() / 2.0,
                                          size.height() / 2.0);
            item->setScale(m_target_zoom / m_current_zoom);
            // item->setPos(item->pos().x(), pageno * m_page_stride);
        }
    }

    updateSceneRect();
    recenterPages();

    if (!m_high_quality_render_timer->isActive())
        m_high_quality_render_timer->start(); // ms
}

bool
DocumentView::pageAtScenePos(const QPointF &scenePos, int &outPageIndex,
                             QGraphicsItem *&outPageItem) const noexcept
{
    for (auto it = m_page_items_hash.begin(); it != m_page_items_hash.end();
         ++it)
    {
        QGraphicsItem *item = it.value();
        if (!item)
            continue;

        if (item->sceneBoundingRect().contains(scenePos))
        {
            outPageIndex = it.key();
            outPageItem  = item;
            return true;
        }
    }

    outPageIndex = -1;
    outPageItem  = nullptr;
    return false;
}
