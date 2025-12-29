#include "DocumentView.hpp"

#include "GraphicsPixmapItem.hpp"
#include "GraphicsView.hpp"
#include "PropertiesWidget.hpp"
#include "utils.hpp"

#include <QClipboard>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QVBoxLayout>
#include <algorithm>
#include <qguiapplication.h>
#include <qicon.h>
#include <qnamespace.h>
#include <qstyle.h>
#include <qtextcursor.h>
#include <strings.h>

DocumentView::DocumentView(const QString &filepath, const Config &config,
                           QWidget *parent) noexcept
    : QWidget(parent), m_config(config)
{
    m_model = new Model(filepath);
    m_model->open();

    m_gview  = new GraphicsView(this);
    m_gscene = new GraphicsScene(m_gview);
    m_gview->setScene(m_gscene);

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

    cachePageStride();
    renderVisiblePages();
}

// Initialize signal-slot connections
void
DocumentView::initConnections() noexcept
{
    connect(m_vscroll, &QScrollBar::valueChanged, this,
            [this](int) { updateCurrentPage(); });

    connect(m_scroll_page_update_timer, &QTimer::timeout, this,
            &DocumentView::scheduleHighQualityRender);

    // connect(this, &DocumentView::currentPageChanged, this,
    //         &DocumentView::scheduleHighQualityRender);

    connect(m_gview, &GraphicsView::textSelectionDeletionRequested, this, [&]()
    {
        // Remove existing selection items from scene
        for (QGraphicsItem *item : m_text_selection_items)
        {
            if (item->scene() == m_gscene)
                m_gscene->removeItem(item);
            delete item;
        }
        m_text_selection_items.clear();
    });

    connect(m_gview, &GraphicsView::textSelectionRequested, this,
            &DocumentView::handleTextSelection);

    connect(m_gview, &GraphicsView::populateContextMenuRequested, this,
            &DocumentView::populateContextMenu);
}

// Handle text selection from GraphicsView
void
DocumentView::handleTextSelection(const QPointF &start,
                                  const QPointF &end) noexcept
{

    // Remove previous selection quads
    for (auto *item : m_text_selection_items)
    {
        if (item->scene() == m_gscene)
            m_gscene->removeItem(item);
        delete item;
    }
    m_text_selection_items.clear();

    int pageIndex                = -1;
    GraphicsPixmapItem *pageItem = nullptr;

    if (!pageAtScenePos(start, pageIndex, pageItem))
        return; // selection start outside visible pages?

    // 🔴 CRITICAL FIX: map to page-local coordinates
    const QPointF pageStart = pageItem->mapFromScene(start);
    const QPointF pageEnd   = pageItem->mapFromScene(end);

    const std::vector<QPolygonF> quads
        = m_model->computeTextSelectionQuad(pageIndex, pageStart, pageEnd);

    // Add to scene
    const QBrush brush(m_config.ui.colors["selection"]);
    for (const QPolygonF &poly : quads)
    {
        QPolygonF scenePoly = pageItem->mapToScene(poly);

        auto *item = m_gscene->addPolygon(scenePoly, Qt::NoPen, brush);
        item->setZValue(ZVALUE_TEXT_SELECTION); // on top of everything
        item->setFlag(QGraphicsItem::ItemIsSelectable, false);
        item->setFlag(QGraphicsItem::ItemIgnoresTransformations, false);
        item->setData(0, "selection");
        item->setData(1, pageIndex);
        m_text_selection_items.push_back(item);
    }
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
            const double newZoom
                = viewWidth / (m_model->pageWidthPts() * m_model->DPR());
            setZoom(newZoom);
            scheduleHighQualityRender();
        }
        break;

        case FitMode::Height:
        {
            const int viewHeight = m_gview->viewport()->height();
            const double newZoom
                = viewHeight / (m_model->pageHeightPts() * m_model->DPR());
            setZoom(newZoom);
            scheduleHighQualityRender();
        }
        break;

        case FitMode::Window:
            break;

        default:
            break;
    }
}

// Set zoom factor directly
void
DocumentView::setZoom(double factor) noexcept
{
    m_current_zoom = factor;
    m_target_zoom  = factor;
    zoomHelper();
}

void
DocumentView::GotoXYZ(int pageno, float x, float y, double zoom) noexcept
{
    if (m_model->numPages() == 0)
        return;

    pageno = std::clamp(pageno, 0, m_model->numPages() - 1);

    // Apply zoom first (XYZ semantics)
    if (zoom > 0.0)
    {
        setZoom(zoom);
        scheduleHighQualityRender();
    }

    if (!m_page_items_hash.contains(pageno))
    {
        m_pending_jump = {pageno, x, y, zoom};
        requestPageRender(pageno);
        return;
    }

    GraphicsPixmapItem *pageItem = m_page_items_hash[pageno];

    // Use the model to get the local pixel offset on that page
    QPointF localPos = m_model->mapPdfToPixmap(pageno, x, y);

    // Map the local pixmap coordinate to the global scene coordinate
    QPointF scenePos = pageItem->mapToScene(localPos);

    // Center view
    m_gview->centerOn(scenePos);
    m_jump_marker->showAt(scenePos.x(), scenePos.y());
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

// Perform search for the given term
void
DocumentView::Search(const QString &term) noexcept
{
    // TODO: Implement search functionality
}

// Function that is common to zoom-in and zoom-out
void
DocumentView::zoomHelper() noexcept
{
    // Apply low-quality zoom to existing items
    for (auto it = m_page_items_hash.begin(); it != m_page_items_hash.end();
         ++it)
    {
        GraphicsPixmapItem *item = it.value();
        if (!item)
            continue;

        double scaleFactor = m_target_zoom / m_current_zoom;
        item->setScale(scaleFactor);
        double pageWidthLogical
            = item->pixmap().width() / item->pixmap().devicePixelRatio();
        m_page_x_offset
            = (m_gview->sceneRect().width() - pageWidthLogical * scaleFactor)
              / 2.0;
        item->setPos(m_page_x_offset, it.key() * m_page_stride);
    }

    m_current_zoom = m_target_zoom;
    m_model->setZoom(m_current_zoom);
    cachePageStride();
    renderVisiblePages(true);
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
    // TODO: Implement next hit functionality
}

// Navigate to the previous search hit
void
DocumentView::PrevHit() noexcept
{
    // TODO: Implement previous hit functionality
}

// Navigate to a specific search hit by index
void
DocumentView::GotoHit(int index) noexcept
{
    // TODO: Implement goto hit functionality
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
    for (auto *item : m_text_selection_items)
    {
        if (item->scene() == m_gscene)
            m_gscene->removeItem(item);
        delete item;
    }
    m_text_selection_items.clear();
}

// Yank the current text selection to clipboard
void
DocumentView::YankSelection() noexcept
{
    if (m_text_selection_items.empty())
        return;
    QClipboard *clipboard = QGuiApplication::clipboard();
    auto range            = m_model->getTextSelectionRange();
    int pageIndex         = m_text_selection_items[0]->data(1).toInt();
    clipboard->setText(
        m_model->getSelectedText(pageIndex, range.first, range.second).c_str());
    ClearTextSelection();
}

// Go to the first page
void
DocumentView::GotoFirstPage() noexcept
{
    m_vscroll->setValue(0);
}

// Go to the last page
void
DocumentView::GotoLastPage() noexcept
{
    int maxValue = m_vscroll->maximum();
    m_vscroll->setValue(maxValue);
}

// Go back in history
void
DocumentView::GoBackHistory() noexcept
{
}

// Get the list of currently visible pages
std::set<int>
DocumentView::getVisiblePages() noexcept
{
    std::set<int> visiblePages;

    QRectF visibleSceneRect
        = m_gview->mapToScene(m_gview->viewport()->rect()).boundingRect();
    double top           = visibleSceneRect.top();
    double bottom        = visibleSceneRect.bottom();
    double preloadMargin = m_page_stride; // 1 page above and below
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
DocumentView::renderVisiblePages(bool force) noexcept
{
    // Render visible pages
    if (force)
    {
        clearDocumentItems();
    }

    std::set<int> visiblePages = getVisiblePages();

    for (int pageno : visiblePages)
        requestPageRender(pageno);

    // Remove unused page items
    removeUnusedLinks(visiblePages);
    removeUnusedPageItems(visiblePages);
    removeUnusedAnnotations(visiblePages);

    updateSceneRect();
}

void
DocumentView::removeUnusedLinks(const std::set<int> &visibleSet) noexcept
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
        }
    }
}

// Remove page items that are not in visibleSet
void
DocumentView::removeUnusedPageItems(const std::set<int> &visibleSet) noexcept
{
    // Copy keys first to avoid iterator issues
    QList<int> keys = m_page_items_hash.keys();

    for (int pageno : keys)
    {
        if (!visibleSet.contains(pageno))
        {
            auto *item = m_page_items_hash.take(pageno);
            if (item)
            {
                if (item->scene() == m_gscene)
                    m_gscene->removeItem(item);
                delete item;
            }
        }
    }
}

void
DocumentView::removeUnusedAnnotations(const std::set<int> &visibleSet) noexcept
{
    // Copy keys first to avoid iterator invalidation
    QList<int> trackedPages = m_page_annotations_hash.keys();

    for (int pageno : trackedPages)
    {
        if (visibleSet.find(pageno) == visibleSet.end())
        {
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
        }
    }
}

// Remove a page item from the scene and delete it
void
DocumentView::removePageItem(int pageno) noexcept
{
    if (m_page_items_hash.contains(pageno))
    {
        GraphicsPixmapItem *item = m_page_items_hash[pageno];
        if (item->scene() == m_gscene)
            m_gscene->removeItem(item);
        delete item;
        m_page_items_hash.remove(pageno);
    }
}

// Cache the page stride in scene coordinates
void
DocumentView::cachePageStride() noexcept
{
    m_page_stride = (m_model->pageHeightPts() * m_model->DPR()
                     + m_spacing * m_model->DPI())
                    * m_current_zoom;
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

    // Center links
    for (auto it = m_page_items_hash.begin(); it != m_page_items_hash.end();
         ++it)
    {
        const int pageno = it.key();
        if (!m_page_items_hash.contains(pageno))
            continue;

        GraphicsPixmapItem *pageItem        = m_page_items_hash[pageno];
        std::vector<BrowseLinkItem *> links = m_model->getLinks(pageno);

        for (auto *link : links)
        {
            // Map link rect to scene coordinates
            QRectF sceneRect
                = pageItem->mapToScene(link->rect()).boundingRect();
            link->setRect(sceneRect);
        }

        // TODO: Center text selections
        // for (auto *selection : m_text_selection_items)
        // {
        //     if (selection->data(1).toInt() != pageno)
        //         continue;

        //     QRectF sceneRect =
        //     pageItem->mapToScene(selection->boundingRect())
        //                            .boundingRect();
        //     selection->setPolygon(sceneRect);
        // }

        // TODO: Center annotations
        for (auto *annot : m_page_annotations_hash[pageno])
        {
            // Map link rect to scene coordinates
            QRectF sceneRect
                = pageItem->mapToScene(annot->boundingRect()).boundingRect();
            annot->setPos(sceneRect.topLeft());
        }
    }
}

// Schedule high-quality rendering of visible pages
void
DocumentView::scheduleHighQualityRender() noexcept
{

    // Save viewport center in scene coordinates
    // const QPointF sceneCenter
    //     = m_gview->mapToScene(m_gview->viewport()->rect().center());

    // Apply HQ zoom
    m_current_zoom = m_target_zoom;
    m_model->setZoom(m_current_zoom);

    const auto pages = getVisiblePages();
    for (int p : pages)
        requestPageRender(p); // HQ render replaces pixmap when ready

    // cachePageStride();
    // updateSceneRect();

    // Clear and render pages & links at HQ
    // clearDocumentItems();

    // renderVisiblePages(true);

    // Restore viewport center
    // m_gview->centerOn(sceneCenter);
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
        std::vector<BrowseLinkItem *> links = m_model->getLinks(pageno);
        for (auto *link : links)
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
        std::vector<Annotation *> annots = m_model->getAnnotations(pageno);
        for (auto *annot : annots)
        {
            if (annot->scene() == m_gscene)
                m_gscene->removeItem(annot);
            delete annot; // only if you own the memory
        }
    }
}

void
DocumentView::populateContextMenu(QMenu *menu) noexcept
{
    auto addAction = [menu, this](const QString &text, const auto &slot)
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
            if (m_text_selection_items.empty())
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
            // addAction("Delete Annotations", &DocumentView::deleteKeyAction);
            // addAction("Change color", &DocumentView::annotChangeColor);
        }
        break;

        case GraphicsView::Mode::TextHighlight:
            // addAction("Change color", &DocumentView::changeHighlighterColor);
            break;

        case GraphicsView::Mode::AnnotRect:
            // addAction("Change color", &DocumentView::changeAnnotRectColor);
            break;

        default:
            break;
    }
}

int
DocumentView::currentPage() const noexcept
{
    if (m_page_stride <= 0)
        return 0;

    const QPointF sceneCenter
        = m_gview->mapToScene(m_gview->viewport()->rect().center());

    int page = static_cast<int>(std::floor(sceneCenter.y() / m_page_stride));

    page = std::clamp(page, 0, m_model->numPages() - 1);
    return page;
}

void
DocumentView::updateCurrentPage() noexcept
{
    const int page = currentPage();
    if (page == m_pageno)
        return;

    m_pageno = page;

    m_scroll_page_update_timer->start();
    emit currentPageChanged(page + 1);
}

void
DocumentView::clearDocumentItems() noexcept
{
    for (QGraphicsItem *item : m_gscene->items())
    {
        if (item != m_jump_marker)
        {
            m_gscene->removeItem(item);
            delete item;
        }
    }

    m_page_items_hash.clear();
    m_text_selection_items.clear();
    m_page_links_hash.clear();
    m_page_annotations_hash.clear();
}

// Request rendering of a specific page (ASYNC)
void
DocumentView::requestPageRender(int pageno) noexcept
{
    if (m_page_items_hash.contains(pageno))
        return;

    auto job = m_model->createRenderJob(pageno);

    m_model->requestPageRender(job,
                               [this, pageno](Model::PageRenderResult result)
    {
        const QImage &image = result.image;
        if (image.isNull())
            return;

        renderPageFromImage(pageno, result.image);
        renderLinks(pageno, result.links);
        renderAnnotations(pageno, result.annotations);

        if (m_pending_jump.pageno != -1)
        {
            GotoXYZ(m_pending_jump.pageno, m_pending_jump.x, m_pending_jump.y,
                    m_pending_jump.zoom);
            m_pending_jump = {-1, 0, 0, 0.0};
        }
    });
}

void
DocumentView::renderPageFromImage(int pageno, const QImage &image) noexcept
{
    m_gview->resetTransform(); // 🔴 critical
    auto *item        = new GraphicsPixmapItem();
    const QPixmap pix = QPixmap::fromImage(image);
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
                { GotoXYZ(pageno, loc.x, loc.y, loc.zoom); });
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
            // m_model->annotDeleteRequested(index);
            // setDirty(true);
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
