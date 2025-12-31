#include "DocumentView.hpp"

#include "GraphicsPixmapItem.hpp"
#include "GraphicsView.hpp"
#include "PropertiesWidget.hpp"

#include <QClipboard>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>
#include <algorithm>
#include <qguiapplication.h>
#include <qicon.h>
#include <qnamespace.h>
#include <qpoint.h>
#include <qpolygon.h>
#include <qstyle.h>
#include <qtextcursor.h>
#include <strings.h>

DocumentView::DocumentView(const QString &filepath, const Config &config,
                           QWidget *parent) noexcept
    : QWidget(parent), m_config(config)
{
    m_model = new Model(filepath);

    m_gview  = new GraphicsView(this);
    m_gscene = new GraphicsScene(m_gview);
    m_gview->setScene(m_gscene);

    m_selection_path_item = m_gscene->addPath(QPainterPath());
    m_selection_path_item->setBrush(QBrush(m_config.ui.colors["selection"]));
    m_selection_path_item->setPen(Qt::NoPen);
    m_selection_path_item->setZValue(ZVALUE_TEXT_SELECTION);

    m_current_search_hit_item = m_gscene->addPath(QPainterPath());
    m_current_search_hit_item->setBrush(m_config.ui.colors["search_index"]);
    m_current_search_hit_item->setPen(Qt::NoPen);
    m_current_search_hit_item->setZValue(ZVALUE_SEARCH_HITS + 1);

    m_hq_render_timer = new QTimer(this);
    m_hq_render_timer->setInterval(200);
    m_hq_render_timer->setSingleShot(true);

    m_scroll_page_update_timer = new QTimer(this);
    m_scroll_page_update_timer->setInterval(100);
    m_scroll_page_update_timer->setSingleShot(true);

    m_jump_marker = new JumpMarker(m_config.ui.colors["jump_marker"]);
    m_jump_marker->setZValue(ZVALUE_JUMP_MARKER);
    m_gscene->addItem(m_jump_marker);

    m_gview->setAlignment(Qt::AlignCenter);

    m_hscroll = m_gview->horizontalScrollBar();
    m_vscroll = m_gview->verticalScrollBar();

    initConnections();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_gview);
}

void
DocumentView::open() noexcept
{
    m_model->open();
    cachePageStride();
    renderVisiblePages();
}

// Initialize signal-slot connections
void
DocumentView::initConnections() noexcept
{

    connect(m_model, &Model::searchResultsReady, this,
            &DocumentView::handleSearchResults);

    connect(m_model, &Model::reloadRequested, this, &DocumentView::reloadPage);

    connect(m_hq_render_timer, &QTimer::timeout, this,
            &DocumentView::renderVisiblePages);

    connect(m_vscroll, &QScrollBar::valueChanged, m_scroll_page_update_timer,
            static_cast<void (QTimer::*)()>(&QTimer::start));

    connect(m_scroll_page_update_timer, &QTimer::timeout, this,
            &DocumentView::renderVisiblePages);

    connect(m_vscroll, &QScrollBar::valueChanged, this,
            &DocumentView::updateCurrentPage);

    connect(m_scroll_page_update_timer, &QTimer::timeout, this,
            &DocumentView::renderVisiblePages);

    connect(m_gview, &GraphicsView::textSelectionDeletionRequested, this,
            &DocumentView::ClearTextSelection);

    connect(m_gview, &GraphicsView::textSelectionRequested, this,
            &DocumentView::handleTextSelection);

    connect(m_gview, &GraphicsView::doubleClickRequested, this,
            [this](const QPointF &pos) { handleClickSelection(2, pos); });

    connect(m_gview, &GraphicsView::tripleClickRequested, this,
            [this](const QPointF &pos) { handleClickSelection(3, pos); });

    connect(m_gview, &GraphicsView::quadrupleClickRequested, this,
            [this](const QPointF &pos) { handleClickSelection(4, pos); });

    connect(m_gview, &GraphicsView::contextMenuRequested, this,
            &DocumentView::handleContextMenuRequested);
}

void
DocumentView::handleSearchResults(
    const QMap<int, std::vector<Model::SearchHit>> &results) noexcept
{
    // Clear previous search hits
    clearSearchHits();

    m_search_hits = results;
    buildFlatSearchHitIndex();
    renderVisiblePages();
}

void
DocumentView::buildFlatSearchHitIndex() noexcept
{
    m_search_hit_flat_refs.clear();
    m_search_hit_flat_refs.reserve(m_model->searchMatchesCount());

    for (auto it = m_search_hits.constBegin(); it != m_search_hits.constEnd();
         ++it)
    {
        const int page   = it.key();
        const auto &hits = it.value();

        for (int i = 0; i < hits.size(); ++i)
            m_search_hit_flat_refs.push_back({page, i});
    }
}

void
DocumentView::handleClickSelection(int clickType,
                                   const QPointF &scenePos) noexcept
{
    int pageIndex                = -1;
    GraphicsPixmapItem *pageItem = nullptr;

    if (!pageAtScenePos(scenePos, pageIndex, pageItem))
        return;

    // Map to page-local coordinates
    const QPointF pagePos = pageItem->mapFromScene(scenePos);
    fz_point pdfPos       = m_model->toPDFSpace(pageIndex, pagePos);

    std::vector<QPolygonF> quads;

    switch (clickType)
    {
        case 2: // double click → select word
            quads = m_model->selectWordAt(pageIndex, pdfPos);
            break;

        case 3: // triple click → select line
            quads = m_model->selectLineAt(pageIndex, pdfPos);
            break;

        case 4: // quadruple click → select entire page
            quads = m_model->selectParagraphAt(pageIndex, pdfPos);
            break;

        default:
            return;
    }

    updateSelectionPath(pageIndex, quads);
}

// Handle text selection from GraphicsView
void
DocumentView::handleTextSelection(const QPointF &start,
                                  const QPointF &end) noexcept
{
    int pageIndex                = -1;
    GraphicsPixmapItem *pageItem = nullptr;

    if (!pageAtScenePos(start, pageIndex, pageItem))
        return; // selection start outside visible pages?

    // 🔴 CRITICAL FIX: map to page-local coordinates
    const QPointF pageStart = pageItem->mapFromScene(start);
    const QPointF pageEnd   = pageItem->mapFromScene(end);

    const std::vector<QPolygonF> quads
        = m_model->computeTextSelectionQuad(pageIndex, pageStart, pageEnd);

    // 3. Batch all polygons into ONE path
    QPainterPath path;
    for (const QPolygonF &poly : quads)
    {
        // We map to scene here, or better yet, make pageItem the parent
        // of m_selection_path_item once to avoid mapping every frame.
        path.addPolygon(pageItem->mapToScene(poly));
    }

    updateSelectionPath(pageIndex, quads);
}

void
DocumentView::updateSelectionPath(int pageno,
                                  std::vector<QPolygonF> quads) noexcept
{
    // 3. Batch all polygons into ONE path
    QPainterPath path;
    GraphicsPixmapItem *pageItem = m_page_items_hash.value(pageno, nullptr);
    if (!pageItem)
        return;

    for (const QPolygonF &poly : quads)
    {
        // We map to scene here, or better yet, make pageItem the parent
        // of m_selection_path_item once to avoid mapping every frame.
        path.addPolygon(pageItem->mapToScene(poly));
    }

    m_selection_path_item->setPath(path);
    m_selection_path_item->show();
    m_last_selection_page = pageno;
    m_selection_path_item->setData(0, pageno); // store page number

    const auto selectionRange = m_model->getTextSelectionRange();
    m_selection_start = QPointF(selectionRange.first.x, selectionRange.first.y);
    m_selection_end = QPointF(selectionRange.second.x, selectionRange.second.y);
}

// Rotate page clockwise
void
DocumentView::RotateClock() noexcept
{
    m_rotation += 90;
    if (m_rotation >= 360)
        m_rotation = 0;
}

// Rotate page anticlockwise
void
DocumentView::RotateAnticlock() noexcept
{
    m_rotation -= 90;
    if (m_rotation < 0)
        m_rotation = 270;
}

// Cycle to the next fit mode
void
DocumentView::NextFitMode() noexcept
{
    FitMode nextMode = static_cast<FitMode>((static_cast<int>(m_fit_mode) + 1)
                                            % static_cast<int>(FitMode::COUNT));
    m_fit_mode       = nextMode;
    setFitMode(nextMode);
    fitModeChanged(nextMode);
}

// Cycle to the next selection mode
void
DocumentView::NextSelectionMode() noexcept
{
    GraphicsView::Mode nextMode = m_gview->getNextMode();
    m_gview->setMode(nextMode);
    emit selectionModeChanged(nextMode);
}

// Set the fit mode and adjust zoom accordingly
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
            const int viewWidth = m_gview->viewport()->width();

            // Calculate the 'base' width of the page in logical pixels at the
            // current DPI We do NOT multiply by DPR here because viewWidth is
            // already logical.
            const double logicalPageWidth
                = m_model->pageWidthPts() * (m_model->DPI() / 72.0);

            // The new zoom is simply the ratio of available space to the page
            // size
            const double newZoom
                = static_cast<double>(viewWidth) / logicalPageWidth;

            setZoom(newZoom);
            renderVisiblePages();
        }
        break;

        case FitMode::Height:
        {
            const int viewHeight = m_gview->viewport()->height();

            // Same logic for height
            const double logicalPageHeight
                = m_model->pageHeightPts() * (m_model->DPI() / 72.0);

            const double newZoom
                = static_cast<double>(viewHeight) / logicalPageHeight;

            setZoom(newZoom);
            renderVisiblePages();
        }
        break;

        case FitMode::Window:
        {
            const int viewWidth  = m_gview->viewport()->width();
            const int viewHeight = m_gview->viewport()->height();

            const double logicalPageWidth
                = m_model->pageWidthPts() * (m_model->DPI() / 72.0);
            const double logicalPageHeight
                = m_model->pageHeightPts() * (m_model->DPI() / 72.0);

            const double zoomX
                = static_cast<double>(viewWidth) / logicalPageWidth;
            const double zoomY
                = static_cast<double>(viewHeight) / logicalPageHeight;

            const double newZoom = std::min(zoomX, zoomY);

            setZoom(newZoom);
            renderVisiblePages();
        }
        break;

        default:
            break;
    }
}

// Set zoom factor directly
void
DocumentView::setZoom(double factor) noexcept
{
    m_target_zoom  = factor;
    m_current_zoom = factor;
    zoomHelper();
}

void
DocumentView::GotoLocation(int pageno, float x, float y) noexcept
{
    if (m_model->numPages() == 0)
        return;

    qDebug() << "GotoLocation: Page" << pageno << "X:" << x << "Y:" << y;

    if (!m_page_items_hash.contains(pageno))
    {
        m_pending_jump = {pageno, x, y};
        requestPageRender(pageno);
        return;
    }

    GraphicsPixmapItem *pageItem = m_page_items_hash[pageno];

    // Use the model to get the local pixel offset on that page
    const QPointF localPos = m_model->toPixelSpace(pageno, fz_point{x, y});

    // Map the local pixmap coordinate to the global scene coordinate
    const QPointF scenePos = pageItem->mapToScene(localPos);

    // Center view
    m_gview->centerOn(scenePos);
    m_jump_marker->showAt(scenePos.x(), scenePos.y());

    m_loc_history.push_back(Location{m_pageno, static_cast<float>(scenePos.x()),
                                     static_cast<float>(scenePos.y())});
}

// Go to specific page number
// Does not render page directly, just adjusts scrollbar
void
DocumentView::GotoPage(int pageno) noexcept
{
    // Goto page by adjusting scrollbar value
    if (pageno < 0 || pageno >= m_model->numPages())
        return;

    pageno         = std::clamp(pageno, 0, m_model->numPages() - 1);
    const double y = pageno * m_page_stride + m_page_stride / 2.0;
    m_gview->centerOn(QPointF(m_gview->sceneRect().width() / 2.0, y));

    m_loc_history.push_back(Location{m_pageno, 0.0f, 0.0f});
}

// Go to next page
void
DocumentView::GotoNextPage() noexcept
{
    if (m_pageno >= m_model->numPages() - 1)
        return;
    GotoPage(m_pageno + 1);
}

// Go to previous page
void
DocumentView::GotoPrevPage() noexcept
{
    if (m_pageno == 0)
        return;
    GotoPage(m_pageno - 1);
}

void
DocumentView::clearSearchHits() noexcept
{
    for (auto *item : m_search_items)
    {
        if (item && item->scene() == m_gscene)
            item->setPath(QPainterPath()); // clear instead of delete
    }
    m_search_items.clear();
    m_search_hits.clear();
    m_search_hit_flat_refs.clear();
}

// Perform search for the given term
void
DocumentView::Search(const QString &term) noexcept
{
    if (term.isEmpty())
    {
        clearSearchHits();
        m_current_search_hit_item->setPath(QPainterPath());
        return;
    }

    // m_search_hits = m_model->search(term);
    m_model->search(term);
}

// Function that is common to zoom-in and zoom-out
void
DocumentView::zoomHelper() noexcept
{
    m_current_zoom = m_target_zoom;
    cachePageStride();

    const int targetPixelHeight = m_model->pageHeightPts() * m_model->DPR()
                                  * m_target_zoom * m_model->DPI() / 72.0;

    for (auto it = m_page_items_hash.begin(); it != m_page_items_hash.end();
         ++it)
    {
        int i                    = it.key();
        GraphicsPixmapItem *item = it.value();

        const QPixmap pix = item->pixmap();

        // 3. Calculate scale based on ACTUAL pixmap height vs TARGET pixel
        // height This ensures the item perfectly fills the 'pixelHeight'
        // portion of the stride
        double currentPixmapHeight = item->pixmap().height();
        double perfectScale
            = static_cast<double>(targetPixelHeight) / currentPixmapHeight;
        item->setScale(perfectScale);

        const double pageWidthScene
            = item->boundingRect().width() * item->scale();

        m_page_x_offset = (m_gview->sceneRect().width() - pageWidthScene) / 2.0;
        item->setPos(m_page_x_offset, i * m_page_stride);
    }

    m_model->setZoom(m_current_zoom);

    QList<int> trackedPages = m_page_links_hash.keys();
    for (int pageno : trackedPages)
    {
        m_model->invalidatePageCache(pageno);
        clearLinksForPage(pageno);
        clearAnnotationsForPage(pageno);
        clearSearchItemsForPage(pageno);
    }

    m_hq_render_timer->start();
}

// Zoom in by a fixed factor
void
DocumentView::ZoomIn() noexcept
{
    m_target_zoom = m_current_zoom * 1.2;
    zoomHelper();
}

// Zoom out by a fixed factor
void
DocumentView::ZoomOut() noexcept
{
    m_target_zoom = m_current_zoom / 1.2;
    zoomHelper();
}

// Reset zoom to 100%
void
DocumentView::ZoomReset() noexcept
{
    m_current_zoom = 1.0f;
    m_target_zoom  = 1.0f;
    m_model->setZoom(m_current_zoom);
    renderVisiblePages();
}

// Navigate to the next search hit
void
DocumentView::NextHit() noexcept
{
    GotoHit(m_search_index + 1);
}

// Navigate to the previous search hit
void
DocumentView::PrevHit() noexcept
{
    GotoHit(m_search_index - 1);
}

// Navigate to a specific search hit by index
void
DocumentView::GotoHit(int index) noexcept
{
    if (index < 0 || index >= m_search_hit_flat_refs.size())
        return;
    const HitRef ref = m_search_hit_flat_refs[index];

    m_search_index = index;
    m_pageno       = ref.page;

    GotoPage(ref.page);
    updateCurrentHitHighlight();
    emit currentPageChanged(ref.page + 1);
}

// Scroll left by a fixed amount
void
DocumentView::ScrollLeft() noexcept
{
    m_hscroll->setValue(m_hscroll->value() - 50);
}

// Scroll right by a fixed amount
void
DocumentView::ScrollRight() noexcept
{
    m_hscroll->setValue(m_hscroll->value() + 50);
}

// Scroll up by a fixed amount
void
DocumentView::ScrollUp() noexcept
{
    m_vscroll->setValue(m_vscroll->value() - 50);
}

// Scroll down by a fixed amount
void
DocumentView::ScrollDown() noexcept
{
    m_vscroll->setValue(m_vscroll->value() + 50);
}

// Get the link KB for the current document
QMap<int, Model::LinkInfo>
DocumentView::LinkKB() noexcept
{
    // TODO: Implement link KB functionality
    return QMap<int, Model::LinkInfo>();
}

// Show file properties dialog
void
DocumentView::FileProperties() noexcept
{
    if (!m_model->success())
        return;

    PropertiesWidget *propsWidget{nullptr};
    if (!propsWidget)
    {
        propsWidget = new PropertiesWidget(this);
        auto props  = m_model->properties();
        propsWidget->setProperties(props);
    }
    propsWidget->exec();
}

// Save the current file
void
DocumentView::SaveFile() noexcept
{
    if (!m_model->SaveChanges())
    {
        QMessageBox::critical(
            this, "Saving failed",
            "Could not save the current file. Try 'Save As' instead.");
    }
}

// Save the current file as a new file
void
DocumentView::SaveAsFile() noexcept
{
    const QString filename
        = QFileDialog::getSaveFileName(this, "Save as", QString());

    if (filename.isEmpty())
        return;

    if (!m_model->SaveAs(filename))
    {
        QMessageBox::critical(
            this, "Saving as failed",
            "Could not perform save as operation on the file");
    }
}

// Close the current file
void
DocumentView::CloseFile() noexcept
{
}

// Toggle auto-resize mode
void
DocumentView::ToggleAutoResize() noexcept
{
    m_auto_resize = !m_auto_resize;
}

// Toggle text highlight mode
void
DocumentView::ToggleTextHighlight() noexcept
{
}

// Toggle region selection mode
void
DocumentView::ToggleRegionSelect() noexcept
{
}

// Toggle annotation rectangle mode
void
DocumentView::ToggleAnnotRect() noexcept
{
}

// Toggle annotation selection mode
void
DocumentView::ToggleAnnotSelect() noexcept
{
}

// Toggle annotation popup mode
void
DocumentView::ToggleAnnotPopup() noexcept
{
}

// Highlight the current text selection
void
DocumentView::TextHighlightCurrentSelection() noexcept
{
    if (m_selection_start.isNull())
        return;

    // 1. Get the page index where the selection happened
    int pageIndex = m_selection_path_item->data(0).toInt();

    // 2. Find the corresponding PageItem in the scene
    GraphicsPixmapItem *pageItem = m_page_items_hash.value(pageIndex, nullptr);
    if (!pageItem)
        return;

    // 🔴 CRITICAL FIX: Map scene coordinates to page-local coordinates
    // This ensures the points passed to highlightTextSelection match
    // the points used in handleTextSelection.
    QPointF localStart = pageItem->mapFromScene(m_selection_start);
    QPointF localEnd   = pageItem->mapFromScene(m_selection_end);

    m_model->highlightTextSelection(pageIndex, localStart, localEnd);

    setModified(true);
    // Render page where selection exists
}

// Clear keyboard hints overlay
void
DocumentView::ClearKBHintsOverlay() noexcept
{
}

// Clear the current text selection
void
DocumentView::ClearTextSelection() noexcept
{
    if (m_selection_start.isNull())
        return;

    if (m_selection_path_item)
    {
        m_selection_path_item->setPath(QPainterPath());
        m_selection_path_item->hide();
        m_selection_path_item->setData(0, -1);
    }
    m_model->clear_fz_stext_page_cache(); // clear cached text pages
}

// Yank the current text selection to clipboard
void
DocumentView::YankSelection() noexcept
{
    if (m_selection_start.isNull())
        return;

    const int pageIndex   = selectionPage();
    QClipboard *clipboard = QGuiApplication::clipboard();
    const auto range      = m_model->getTextSelectionRange();
    const std::string text
        = m_model->getSelectedText(pageIndex, range.first, range.second);
    clipboard->setText(text.c_str());

    // ClearTextSelection();
}

// Go to the first page
void
DocumentView::GotoFirstPage() noexcept
{
    m_vscroll->setValue(0);
    m_loc_history.push_back(Location{m_pageno, 0.0f, 0.0f});
}

// Go to the last page
void
DocumentView::GotoLastPage() noexcept
{
    int maxValue = m_vscroll->maximum();
    m_vscroll->setValue(maxValue);
    m_loc_history.push_back(Location{m_pageno, 0.0f, 0.0f});
}

// Go back in history
void
DocumentView::GoBackHistory() noexcept
{
    if (m_loc_history.empty())
        return;

    if (m_loc_history.back().pageno == m_pageno)
    {
        // Pop current location
        m_loc_history.pop_back();
        if (m_loc_history.empty())
            return;
    }
    const Location loc = m_loc_history.back();
    m_loc_history.pop_back();
    GotoLocation(loc.pageno, loc.x, loc.y);
}

// Get the list of currently visible pages
std::set<int>
DocumentView::getVisiblePages() noexcept
{
    std::set<int> visiblePages;

    const double preloadMargin = m_page_stride;

    // if (m_config.behavior.cache_pages > 0)
    //     preloadMargin *= m_config.behavior.cache_pages;

    const QRectF visibleSceneRect
        = m_gview->mapToScene(m_gview->viewport()->rect()).boundingRect();

    double top    = visibleSceneRect.top();
    double bottom = visibleSceneRect.bottom();
    top -= preloadMargin;
    bottom += preloadMargin;

    int firstPage = static_cast<int>(std::floor(top / m_page_stride));
    int lastPage  = static_cast<int>(std::floor(bottom / m_page_stride));

    firstPage = std::max(0, firstPage);
    lastPage  = std::min(m_model->numPages() - 1, lastPage);

    for (int pageno = firstPage; pageno <= lastPage; ++pageno)
        visiblePages.insert(pageno);

    return visiblePages;
}

// Clear links for a specific page
void
DocumentView::clearLinksForPage(int pageno) noexcept
{
    if (!m_page_links_hash.contains(pageno))
        return;

    auto links = m_page_links_hash.take(pageno); // removes from hash
    for (auto *link : links)
    {
        if (!link)
            continue;

        // Remove from scene if still present
        if (link->scene() == m_gscene)
            m_gscene->removeItem(link);

        delete link; // safe: we "own" these
    }
}

void
DocumentView::clearSearchItemsForPage(int pageno) noexcept
{
    if (!m_search_items.contains(pageno))
        return;

    QGraphicsPathItem *item
        = m_search_items.take(pageno); // removes item from hash
    if (item)
    {
        if (item->scene() == m_gscene)
            m_gscene->removeItem(item);
        delete item;
    }
}

// Clear links for a specific page
void
DocumentView::clearAnnotationsForPage(int pageno) noexcept
{
    if (!m_page_annotations_hash.contains(pageno))
        return;

    auto annotations
        = m_page_annotations_hash.take(pageno); // removes from hash
    for (auto *annotation : annotations)
    {
        if (!annotation)
            continue;

        // Remove from scene if still present
        if (annotation->scene() == m_gscene)
            m_gscene->removeItem(annotation);

        delete annotation; // safe: we "own" these
    }
}

// Render all visible pages, optionally forcing re-render
void
DocumentView::renderVisiblePages() noexcept
{
    std::set<int> visiblePages = getVisiblePages();

    removeUnusedPageItems(visiblePages);
    // ClearTextSelection();

    for (int pageno : visiblePages)
        requestPageRender(pageno);

    updateSceneRect();

    // Update text selection position after re-render
}

void
DocumentView::removeUnusedPageItems(const std::set<int> &visibleSet) noexcept
{
    // Copy keys first to avoid iterator invalidation
    QList<int> trackedPages = m_page_links_hash.keys();
    for (int pageno : trackedPages)
    {
        if (visibleSet.find(pageno) == visibleSet.end())
        {
            auto links = m_page_links_hash.take(pageno); // removes from hash
            for (auto *link : links)
            {
                if (!link)
                    continue;

                // Remove from scene if still present
                if (link->scene() == m_gscene)
                    m_gscene->removeItem(link);

                delete link; // safe: we "own" these
            }

            auto *item = m_page_items_hash.take(pageno);
            if (item)
            {
                if (item->scene() == m_gscene)
                    m_gscene->removeItem(item);
                delete item;
            }

            auto annots
                = m_page_annotations_hash.take(pageno); // removes from hash
            for (auto *annot : annots)
            {
                if (!annot)
                    continue;

                // Remove from scene if still present
                if (annot->scene() == m_gscene)
                    m_gscene->removeItem(annot);

                delete annot; // safe: we "own" these
            }

            // Remove search hits for this page
            auto searchHits
                = m_search_hits.take(pageno); // removes SearchHits from hash
            clearSearchItemsForPage(pageno);
        }
    }
}

// Remove a page item from the scene and delete it
void
DocumentView::removePageItem(int pageno) noexcept
{
    if (m_page_items_hash.contains(pageno))
    {
        GraphicsPixmapItem *item = m_page_items_hash.take(pageno);
        if (item->scene() == m_gscene)
            m_gscene->removeItem(item);
        delete item;
    }
}

void
DocumentView::cachePageStride() noexcept
{
    // points → inches → scene units
    const double pageHeightScene
        = (m_model->pageHeightPts() / 72.0) * m_model->DPI() * m_current_zoom;

    const double spacingScene = m_spacing * m_current_zoom;

    m_page_stride = pageHeightScene + spacingScene;
}

// Update the scene rect based on number of pages and page stride
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
    clearDocumentItems();
    renderVisiblePages();

    if (m_auto_resize)
    {
        setFitMode(m_fit_mode);
        fitModeChanged(m_fit_mode);
    }

    QWidget::resizeEvent(event);
}

// Check if a scene position is within any page item
bool
DocumentView::pageAtScenePos(const QPointF &scenePos, int &outPageIndex,
                             GraphicsPixmapItem *&outPageItem) const noexcept
{
    for (auto it = m_page_items_hash.begin(); it != m_page_items_hash.end();
         ++it)
    {
        GraphicsPixmapItem *item = it.value();
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

void
DocumentView::clearVisiblePages() noexcept
{
    // m_page_items_hash
    for (auto it = m_page_items_hash.begin(); it != m_page_items_hash.end();
         ++it)
    {
        GraphicsPixmapItem *item = it.value();
        if (item->scene() == m_gscene)
            m_gscene->removeItem(item);
    }
    m_page_items_hash.clear();
}

void
DocumentView::clearVisibleLinks() noexcept
{
    QList<int> trackedPages = m_page_links_hash.keys();
    for (int pageno : trackedPages)
    {
        for (auto *link : m_page_links_hash.take(pageno))
        {
            if (link->scene() == m_gscene)
                m_gscene->removeItem(link);
            delete link; // only if you own the memory
        }
    }
}

void
DocumentView::clearVisibleAnnotations() noexcept
{
    QList<int> trackedPages = m_page_annotations_hash.keys();
    for (int pageno : trackedPages)
    {
        for (auto *annot : m_page_annotations_hash.take(pageno))
        {
            if (annot->scene() == m_gscene)
                m_gscene->removeItem(annot);
            delete annot; // only if you own the memory
        }
    }
}

void
DocumentView::handleContextMenuRequested(const QPointF &scenePos) noexcept
{
    QMenu *menu = new QMenu(this);
    menu->move(scenePos.x(), scenePos.y());

    auto addAction = [this, &menu](const QString &text, const auto &slot)
    {
        QAction *action = new QAction(text, menu); // sets parent = menu
        connect(action, &QAction::triggered, this, slot);
        menu->addAction(action);
    };

    // If right-clicked on an image
    // if (m_hit_pixmap)
    // {
    //     addAction("Open Image in External Viewer",
    //               &DocumentView::OpenHitPixmapInExternalViewer);
    //     addAction("Save Image As...", &DocumentView::SaveImageAs);
    //     addAction("Copy Image", &DocumentView::CopyImageToClipboard);
    //     return;
    // }

    switch (m_gview->mode())
    {
        case GraphicsView::Mode::TextSelection:
        {
            if (!m_selection_path_item
                || m_selection_path_item->path().isEmpty())
                return;

            addAction("Copy Text", &DocumentView::YankSelection);
            addAction("Highlight Text",
                      &DocumentView::TextHighlightCurrentSelection);
        }
        break;

        case GraphicsView::Mode::AnnotSelect:
        {
            // if (!m_annot_selection_present)
            //     return;
            // addAction("Delete Annotations",
            // &DocumentView::deleteKeyAction); addAction("Change color",
            // &DocumentView::annotChangeColor);
        }
        break;

        case GraphicsView::Mode::TextHighlight:
            // addAction("Change color",
            // &DocumentView::changeHighlighterColor);
            break;

        case GraphicsView::Mode::AnnotRect:
            // addAction("Change color",
            // &DocumentView::changeAnnotRectColor);
            break;

        default:
            break;
    }

    menu->show();
}

void
DocumentView::updateCurrentHitHighlight() noexcept
{
    if (m_search_index < 0 || m_search_index >= m_search_hit_flat_refs.size())
    {
        m_current_search_hit_item->setPath(QPainterPath());
        return;
    }

    const HitRef ref = m_search_hit_flat_refs[m_search_index];
    const auto &hit  = m_search_hits[ref.page][ref.indexInPage];

    GraphicsPixmapItem *pageItem = m_page_items_hash.value(ref.page, nullptr);
    if (!pageItem || !pageItem->scene())
        return;

    QPolygonF poly;
    poly << QPointF(hit.quad.ul.x * m_current_zoom,
                    hit.quad.ul.y * m_current_zoom)
         << QPointF(hit.quad.ur.x * m_current_zoom,
                    hit.quad.ur.y * m_current_zoom)
         << QPointF(hit.quad.lr.x * m_current_zoom,
                    hit.quad.lr.y * m_current_zoom)
         << QPointF(hit.quad.ll.x * m_current_zoom,
                    hit.quad.ll.y * m_current_zoom);

    QPainterPath path;
    path.addPolygon(pageItem->mapToScene(poly));

    m_current_search_hit_item->setPath(path);
}

void
DocumentView::updateCurrentPage() noexcept
{
    const int scrollY = m_vscroll->value();
    const int viewH   = m_gview->viewport()->height();

    const int centerY = scrollY + viewH / 2;

    int page = centerY / m_page_stride;
    page     = std::clamp(page, 0, m_model->numPages() - 1);

    if (page == m_pageno)
        return;

    m_pageno = page;
    emit currentPageChanged(page + 1);
}

void
DocumentView::clearDocumentItems() noexcept
{
    for (QGraphicsItem *item : m_gscene->items())
    {
        if (item != m_jump_marker && item != m_selection_path_item
            && item != m_current_search_hit_item)
        {
            m_gscene->removeItem(item);
            delete item;
        }
    }

    m_page_items_hash.clear();
    ClearTextSelection();
    m_search_items.clear();
    m_page_links_hash.clear();
    m_page_annotations_hash.clear();
}

// Request rendering of a specific page (ASYNC)
void
DocumentView::requestPageRender(int pageno) noexcept
{
    // if (m_page_items_hash.contains(pageno))
    //     return;

    auto job = m_model->createRenderJob(pageno);

    m_model->requestPageRender(
        job, [this, pageno](const Model::PageRenderResult &result)
    {
        const QImage &image = result.image;
        if (image.isNull())
            return;

        renderPageFromImage(pageno, result.image);
        renderLinks(pageno, result.links);
        renderAnnotations(pageno, result.annotations);
        renderSearchHitsForPage(pageno);

        if (m_pending_jump.pageno != -1)
        {
            GotoLocation(m_pending_jump.pageno, m_pending_jump.x,
                         m_pending_jump.y);
            m_pending_jump = {-1, 0, 0};
        }
    });
}

void
DocumentView::renderPageFromImage(int pageno, const QImage &image) noexcept
{
    removePageItem(pageno);
    createAndAddPageItem(pageno, QPixmap::fromImage(image));
}

void
DocumentView::createAndAddPageItem(int pageno, const QPixmap &pix) noexcept
{
    auto *item = new GraphicsPixmapItem();
    item->setPixmap(pix);
    double pageWidthLogical = pix.width() / pix.devicePixelRatio();
    m_page_x_offset = (m_gview->sceneRect().width() - pageWidthLogical) / 2.0;
    item->setPos(m_page_x_offset, pageno * m_page_stride);
    m_gscene->addItem(item);
    m_page_items_hash[pageno] = item;
}

void
DocumentView::renderLinks(int pageno,
                          const std::vector<BrowseLinkItem *> &links) noexcept
{
    if (m_page_links_hash.contains(pageno))
        return;

    clearLinksForPage(pageno);

    GraphicsPixmapItem *pageItem = m_page_items_hash[pageno];

    for (auto *link : links)
    {
        switch (link->linkType())
        {
            case BrowseLinkItem::LinkType::FitH:
            {
                connect(link, &BrowseLinkItem::horizontalFitRequested, this,
                        [&](int pageno, const BrowseLinkItem::Location &loc)
                {
                    // GotoXYZ(pageno, 0, loc.y, m_current_zoom);
                    // setFitMode(FitMode::Width);
                    // m_gview->fitToWidthAtY(loc.y);
                });
            }
            break;

            case BrowseLinkItem::LinkType::FitV:
            {
                connect(link, &BrowseLinkItem::verticalFitRequested, this,
                        [&](int pageno, const BrowseLinkItem::Location &loc)
                {
                    // GotoXYZ(pageno, 0, 0, m_current_zoom);
                    // setFitMode(FitMode::Height);
                    // m_gview->fitToHeight();
                });
            }
            break;

            case BrowseLinkItem::LinkType::Page:
            {
                connect(link, &BrowseLinkItem::jumpToPageRequested, this,
                        [&](int pageno) { GotoPage(pageno); });
            }
            break;

            case BrowseLinkItem::LinkType::XYZ:
            {
                connect(link, &BrowseLinkItem::jumpToLocationRequested, this,
                        [&](int pageno, const BrowseLinkItem::Location &loc)
                { GotoLocation(pageno, loc.x, loc.y); });
            }
            break;

            default:
                break;
        }

        connect(link, &BrowseLinkItem::linkCopyRequested, this,
                [&](const QString &link)
        {
            if (link.startsWith("#"))
            {
                auto equal_pos = link.indexOf("=");
                emit clipboardContentChanged(m_model->filePath() + "#"
                                             + link.mid(equal_pos + 1));
            }
            else
            {
                emit clipboardContentChanged(link);
            }
        });
        // Map link rect to scene coordinates
        const QRectF sceneRect
            = pageItem->mapToScene(link->rect()).boundingRect();
        link->setRect(sceneRect);
        link->setZValue(ZVALUE_LINK);
        m_gscene->addItem(link);
        m_page_links_hash[pageno]
            = links; // Store them so we can actually delete them later
    }
}

void
DocumentView::renderAnnotations(
    int pageno, const std::vector<Annotation *> &annotations) noexcept
{
    if (m_page_annotations_hash.contains(pageno))
        return;

    clearAnnotationsForPage(pageno);

    GraphicsPixmapItem *pageItem = m_page_items_hash[pageno];
    for (Annotation *annot : annotations)
    {
        annot->setZValue(ZVALUE_ANNOTATION);
        annot->setPos(
            pageItem->pos()); // Annotations are relative to page origin
        m_gscene->addItem(annot);
        m_page_annotations_hash[pageno]
            = annotations; // Store them so we can actually delete them
                           // later
        connect(annot, &Annotation::annotDeleteRequested, [&](int index)
        {
            m_model->removeHighlightAnnotation(pageno, {index});
            setModified(true);
        });

        connect(annot, &Annotation::annotColorChangeRequested,
                [this, annot](int index)
        {
            // auto color = QColorDialog::getColor(
            //     annot->data(3).value<QColor>(), this, "Highlight Color",
            //     QColorDialog::ColorDialogOption::ShowAlphaChannel);
            // if (color.isValid())
            // {
            //     m_model->annotChangeColorForIndex(index, color);
            //     setDirty(true);
            // }
        });
    }
}

void
DocumentView::setModified(bool modified) noexcept
{
    if (m_is_modified == modified)
        return;

    m_is_modified = modified;

    QString title     = m_config.ui.window_title_format;
    QString panelName = m_model->filePath();

    if (modified)
    {
        if (!title.endsWith("*"))
            title.append("*");
        if (!panelName.endsWith("*"))
            panelName.append("*");
    }
    else
    {
        if (title.endsWith("*"))
            title.chop(1);
        if (panelName.endsWith("*"))
            panelName.chop(1);
    }

    title = title.arg(fileName());
    emit panelNameChanged(panelName);
    this->setWindowTitle(title);
}

void
DocumentView::reloadPage(int pageno) noexcept
{
    removePageItem(pageno);
    requestPageRender(pageno);
}

bool
DocumentView::EncryptDocument() noexcept
{
    Model::EncryptInfo encryptInfo;
    bool ok;
    QString password = QInputDialog::getText(
        this, "Encrypt Document", "Enter password:", QLineEdit::Password,
        QString(), &ok);
    if (!ok || password.isEmpty())
        return false;
    encryptInfo.user_password = password;
    return m_model->encrypt(encryptInfo);
}

bool
DocumentView::DecryptDocument() noexcept
{
    if (m_model->passwordRequired())
    {
        bool ok;
        QString password;

        while (true)
        {
            password = QInputDialog::getText(
                this, "Decrypt Document",
                "Enter password:", QLineEdit::Password, QString(), &ok);
            if (!ok)
                return false;

            if (authenticate(password))
                return m_model->decrypt();
        }
    }
    return true;
}

void
DocumentView::renderSearchHitsForPage(int pageno) noexcept
{

    if (!m_search_hits.contains(pageno))
        return;

    const auto hits = m_search_hits.value(pageno); // Local copy

    // 2. Validate the Page Item still exists in the scene
    GraphicsPixmapItem *pageItem = m_page_items_hash.value(pageno, nullptr);
    if (!pageItem || !pageItem->scene())
        return;

    QGraphicsPathItem *item = ensureSearchItemForPage(pageno);
    if (!item)
        return;

    QPainterPath allPath;

    for (int i = 0; i < hits.size(); ++i)
    {
        const Model::SearchHit &hit = hits[i];
        QPolygonF poly;
        poly << QPointF(hit.quad.ul.x * m_current_zoom,
                        hit.quad.ul.y * m_current_zoom)
             << QPointF(hit.quad.ur.x * m_current_zoom,
                        hit.quad.ur.y * m_current_zoom)
             << QPointF(hit.quad.lr.x * m_current_zoom,
                        hit.quad.lr.y * m_current_zoom)
             << QPointF(hit.quad.ll.x * m_current_zoom,
                        hit.quad.ll.y * m_current_zoom);

        allPath.addPolygon(pageItem->mapToScene(poly));
    }

    // Set colors
    item->setPath(allPath);
    item->setBrush(m_config.ui.colors["search_match"]);
}

QGraphicsPathItem *
DocumentView::ensureSearchItemForPage(int pageno) noexcept
{
    if (m_search_items.contains(pageno))
        return m_search_items[pageno];

    auto *item = m_gscene->addPath(QPainterPath());
    item->setBrush(QColor(255, 230, 150, 120));
    item->setPen(Qt::NoPen);
    item->setZValue(ZVALUE_SEARCH_HITS);

    m_search_items[pageno] = item;
    return item;
}

void
DocumentView::ReselectLastTextSelection() noexcept
{
    if (m_selection_start.isNull())
        return;

    // Re-apply the selection path item
    if (m_selection_path_item)
    {
        m_selection_path_item->show();
        qDebug() << "DD";
    }
}
