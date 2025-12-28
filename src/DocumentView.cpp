#include "DocumentView.hpp"

#include "GraphicsPixmapItem.hpp"
#include "GraphicsView.hpp"
#include "utils.hpp"

#include <QClipboard>
#include <QVBoxLayout>
#include <algorithm>
#include <qguiapplication.h>
#include <qnamespace.h>
#include <qtextcursor.h>
#include <strings.h>

DocumentView::DocumentView(const QString &filepath, const Config &config,
                           QWidget *parent) noexcept
    : QWidget(parent), m_config(config)
{
    m_model = new Model(filepath);
    m_model->open();

    m_high_quality_render_timer = new QTimer(this);
    m_high_quality_render_timer->setSingleShot(true);
    m_high_quality_render_timer->setInterval(120);

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

// Initialize signal-slot connections
void
DocumentView::initConnections() noexcept
{
    connect(m_vscroll, &QScrollBar::valueChanged, this,
            [this](int) { scheduleHighQualityRender(); });

    connect(m_high_quality_render_timer, &QTimer::timeout, this, [this]()
    {
        // Save viewport center in scene coordinates
        QPointF sceneCenter
            = m_gview->mapToScene(m_gview->viewport()->rect().center());

        // Apply HQ zoom
        m_current_zoom = m_target_zoom;

        m_model->setZoom(m_current_zoom);
        cachePageStride();
        updateSceneRect();

        // Clear and render pages & links at HQ
        m_gscene->clear();
        m_page_links_hash.clear();
        m_page_items_hash.clear();
        m_text_selection_items.clear();

        renderVisiblePages(true);

        // Restore viewport center
        m_gview->centerOn(sceneCenter);
    });

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

// Set zoom factor directly
void
DocumentView::setZoom(double factor) noexcept
{
    m_current_zoom = factor;
    m_target_zoom  = factor;
}

// Go to specific page number
void
DocumentView::GotoPage(int pageno) noexcept
{
}

// Go to next page
void
DocumentView::GotoNextPage() noexcept
{
}

// Go to previous page
void
DocumentView::GotoPrevPage() noexcept
{
}

// Perform search for the given term
void
DocumentView::Search(const QString &term) noexcept
{
}

// Function that is common to zoom-in and zoom-out
void
DocumentView::zoomHelper() noexcept
{
    m_high_quality_render_timer->stop(); // Stop any pending renders
    scheduleHighQualityRender();
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
}

// Navigate to the previous search hit
void
DocumentView::PrevHit() noexcept
{
}

// Navigate to a specific search hit by index
void
DocumentView::GotoHit(int index) noexcept
{
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
}

// Show file properties dialog
void
DocumentView::FileProperties() noexcept
{
}

// Save the current file
void
DocumentView::SaveFile() noexcept
{
}

// Save the current file as a new file
void
DocumentView::SaveAsFile() noexcept
{
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
    double top    = visibleSceneRect.top();
    double bottom = visibleSceneRect.bottom();

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

// Render links for a specific page
void
DocumentView::renderLinksForPage(int pageno) noexcept
{
    if (!m_page_items_hash.contains(pageno))
        return;

    clearLinksForPage(pageno);

    GraphicsPixmapItem *pageItem        = m_page_items_hash[pageno];
    std::vector<BrowseLinkItem *> links = m_model->getLinks(pageno);

    for (auto *link : links)
    {
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
        QRectF sceneRect = pageItem->mapToScene(link->rect()).boundingRect();
        link->setRect(sceneRect);
        m_gscene->addItem(link);
        m_page_links_hash[pageno]
            = links; // Store them so we can actually delete them later
    }
}

// Render annotations for a specific page
void
DocumentView::renderAnnotationsForPage(int pageno) noexcept
{
    if (!m_page_items_hash.contains(pageno))
        return;

    clearAnnotationsForPage(pageno);

    std::vector<Annotation *> annotations = m_model->getAnnotations(pageno);

    for (Annotation *annot : annotations)
    {
        // Map link rect to scene coordinates
        m_gscene->addItem(annot);
        connect(annot, &Annotation::annotDeleteRequested, [&](int index)
        {
            // m_model->annotDeleteRequested(index);
            // setDirty(true);
            renderPage(m_pageno, true);
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
            //     renderPage(m_pageno, true);
            // }
        });
    }
}

// Render a specific page to the scene
void
DocumentView::renderPage(int pageno, bool force) noexcept
{
    if (force == false && m_page_items_hash.contains(pageno))
        return;

    // removePageItem(pageno);
    m_gview->resetTransform(); // 🔴 critical
    const QPixmap pix = m_model->renderPageToPixmap(pageno);
    auto *item        = new GraphicsPixmapItem();
    item->setPixmap(pix);
    double pageWidthLogical = pix.width() / pix.devicePixelRatio();
    m_page_x_offset = (m_gview->sceneRect().width() - pageWidthLogical) / 2.0;
    item->setPos(m_page_x_offset, pageno * m_page_stride);
    m_gscene->addItem(item);
    m_page_items_hash[pageno] = item;
}

// Render all visible pages, optionally forcing re-render
void
DocumentView::renderVisiblePages(bool force) noexcept
{
    // Render visible pages
    std::set<int> visiblePages = getVisiblePages();

    for (int pageno : visiblePages)
    {
        renderPage(pageno, force);
        renderLinksForPage(pageno);
        renderAnnotationsForPage(pageno);
    }

    // Remove unused page items
    removeUnusedLinks(visiblePages);
    removeUnusedPageItems(visiblePages);

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
    // Nuclear option to stop artifacts:
    m_gscene->clear();
    m_page_items_hash.clear();
    m_text_selection_items.clear();
    m_page_links_hash.clear();

    renderVisiblePages();
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
    }
}

// Schedule high-quality rendering of visible pages
void
DocumentView::scheduleHighQualityRender() noexcept
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

    if (!m_high_quality_render_timer->isActive())
        m_high_quality_render_timer->start(); // ms
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
