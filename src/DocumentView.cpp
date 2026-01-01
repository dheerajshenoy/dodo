#include "DocumentView.hpp"

#include "BrowseLinkItem.hpp"
#include "GraphicsPixmapItem.hpp"
#include "GraphicsView.hpp"
#include "PropertiesWidget.hpp"

#include <QClipboard>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
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

#ifdef HAS_SYNCTEX
    initSynctex();
#endif

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
    m_vscroll = new VerticalScrollBar(Qt::Vertical, this);
    m_gview->setVerticalScrollBar(m_vscroll);

    initConnections();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_gview);
}

void
DocumentView::initSynctex() noexcept
{
    if (m_synctex_scanner)
    {
        synctex_scanner_free(m_synctex_scanner);
        m_synctex_scanner = nullptr;
    }
    m_synctex_scanner = synctex_scanner_new_with_output_file(
        CSTR(m_model->filePath()), nullptr, 1);
    if (!m_synctex_scanner)
        return;
}

void
DocumentView::open() noexcept
{
#ifndef NDEBUG
    qDebug() << "DocumentView::open(): Opening file:" << m_model->filePath();
#endif

    m_model->open();
    m_pageno = 0;
    cachePageStride();
    renderVisiblePages();
}

// Initialize signal-slot connections
void
DocumentView::initConnections() noexcept
{

#ifndef NDEBUG
    qDebug() << "DocumentView::initConnections(): Initializing connections";
#endif

#ifdef HAS_SYNCTEX
    connect(m_gview, &GraphicsView::synctexJumpRequested, this,
            &DocumentView::handleSynctexJumpRequested);
#endif

    // connect(m_gview, &GraphicsView::rightClickRequested, this,
    //         [&](const QPointF &scenePos)
    // {
    // int pageIndex                = -1;
    // GraphicsPixmapItem *pageItem = nullptr;

    // if (!pageAtScenePos(scenePos, pageIndex, pageItem))
    //     return; // selection start outside visible pages?

    // const QPointF pagePos = pageItem->mapFromScene(scenePos);
    // m_hit_pixmap = m_model->hitTestImage(pageIndex, pagePos,
    // m_current_zoom,
    //                                      m_rotation);
    // });

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
#ifndef NDEBUG
    qDebug() << "DocumentView::handleSearchResults(): Received"
             << results.size() << "pages with search hits.";
#endif

    // Clear previous search hits
    clearSearchHits();

    if (results.isEmpty())
    {
        QMessageBox::information(this, tr("Search"),
                                 tr("No matches found for "
                                    "the given term."));
        return;
    }

    m_search_hits  = results;
    m_search_index = 0;
    buildFlatSearchHitIndex();
    renderVisiblePages();
    updateCurrentHitHighlight();

    if (m_config.ui.search_hits_on_scrollbar)
        renderSearchHitsInScrollbar();

    emit searchCountChanged(m_model->searchMatchesCount());
}

void
DocumentView::buildFlatSearchHitIndex() noexcept
{
#ifndef NDEBUG
    qDebug() << "DocumentView::buildFlatSearchHitIndex(): Building flat index";
#endif
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
#ifndef NDEBUG
    qDebug() << "DocumentView::handleClickSelection(): Handling click type"
             << clickType << "at scene position" << scenePos;
#endif
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

// Handle SyncTeX jump request
#ifdef HAS_SYNCTEX
void
DocumentView::handleSynctexJumpRequested(const QPointF &scenePos) noexcept
{
#ifndef NDEBUG
    qDebug() << "DocumentView::handleSynctexJumpRequested(): Handling "
             << "SyncTeX jump to scene position" << scenePos;
#endif

    if (m_synctex_scanner)
    {
        int pageIndex                = -1;
        GraphicsPixmapItem *pageItem = nullptr;

        if (!pageAtScenePos(scenePos, pageIndex, pageItem))
            return;

        // Map to page-local coordinates
        const QPointF pagePos = pageItem->mapFromScene(scenePos);
        fz_point pdfPos       = m_model->toPDFSpace(pageIndex, pagePos);

        if (synctex_edit_query(m_synctex_scanner, pageIndex + 1, pdfPos.x,
                               pdfPos.y)
            > 0)
        {
            synctex_node_p node;
            while ((node = synctex_scanner_next_result(m_synctex_scanner)))
                synctexLocateInDocument(synctex_node_get_name(node),
                                        synctex_node_line(node));
        }
        else
        {
            QMessageBox::warning(this, "SyncTeX Error",
                                 "No matching source found!");
        }
    }
    else
    {
        QMessageBox::warning(this, "SyncTex", "Not a valid synctex document");
    }
}
#endif

void
DocumentView::synctexLocateInDocument(const char *texFileName,
                                      int line) noexcept
{
    QString tmp = m_config.behavior.synctex_editor_command;
    if (!tmp.contains("%f") || !tmp.contains("%l"))
    {
        QMessageBox::critical(this, "SyncTeX error",
                              "Invalid SyncTeX editor command: missing "
                              "placeholders (%l and/or %f).");
        return;
    }

    auto args   = QProcess::splitCommand(tmp);
    auto editor = args.takeFirst();
    args.replaceInStrings("%l", QString::number(line));
    args.replaceInStrings("%f", texFileName);

    QProcess::startDetached(editor, args);
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

#ifndef NDEBUG
    qDebug() << "DocumentView::handleTextSelection(): Handling text selection"
             << "from" << start << "to" << end;
#endif

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
#ifndef NDEBUG
    qDebug() << "DocumentView::updateSelectionPath(): Updating selection path"
             << "for page" << pageno << "with" << quads.size() << "polygons.";
#endif

    // Batch all polygons into ONE path
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
#ifndef NDEBUG
    qDebug() << "setFitMode(): Setting fit mode to:" << static_cast<int>(mode);
#endif

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
#ifndef NDEBUG
    qDebug() << "DocumentView::setZoom(): Setting zoom to factor:" << factor;
#endif

    m_target_zoom  = factor;
    m_current_zoom = factor;
    zoomHelper();
}

void
DocumentView::GotoLocation(const PageLocation &targetLocation,
                           const PageLocation &sourceLocation) noexcept
{
    if (m_model->numPages() == 0)
        return;

    // RECORD HISTORY BEFORE JUMPING
    // Only record if not currently navigating through history
    if (!m_suppress_history_recording)
    {
        if (sourceLocation.pageno != -1)
        {
            // Use provided source location
            qDebug() << "Recording source location from provided data:"
                     << sourceLocation.pageno << sourceLocation.x
                     << sourceLocation.y;
            m_loc_history.push_back(
                {sourceLocation.pageno, sourceLocation.x, sourceLocation.y});
        }
        else
        {
            // Map the center of the current viewport back to PDF space
            const QPointF centerScene
                = m_gview->mapToScene(m_gview->viewport()->rect().center());
            int dummyIdx;
            GraphicsPixmapItem *pageItem = nullptr;

            if (pageAtScenePos(centerScene, dummyIdx, pageItem))
            {
                QPointF localPos = pageItem->mapFromScene(centerScene);
                fz_point pdfLoc  = m_model->toPDFSpace(dummyIdx, localPos);
                m_loc_history.push_back({dummyIdx, pdfLoc.x, pdfLoc.y});
            }
        }
    }

    // HANDLE PENDING RENDERS
    if (!m_page_items_hash.contains(targetLocation.pageno))
    {
        qDebug() << "DocumentView::GotoLocation(): Target page"
                 << targetLocation.pageno
                 << "not yet rendered. Deferring jump until render.";
        m_pending_jump = targetLocation;
        requestPageRender(targetLocation.pageno);
        return;
    }

#ifndef NDEBUG
    qDebug() << "DocumentView::GotoLocation(): Requested "
                "targetLocationation:"
             << targetLocation.pageno << targetLocation.x << targetLocation.y
             << "in document with" << m_model->numPages() << "pages.";
#endif

    GraphicsPixmapItem *pageItem = m_page_items_hash[targetLocation.pageno];
    const QPointF targetLocationalPos = m_model->toPixelSpace(
        targetLocation.pageno, fz_point{targetLocation.x, targetLocation.y});
    const QPointF scenePos = pageItem->mapToScene(targetLocationalPos);

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

#ifndef NDEBUG
    qDebug() << "DocumentView::GotoPage(): Going to page:" << pageno;
#endif

    m_pageno       = pageno;
    const double y = pageno * m_page_stride + m_page_stride / 2.0;
    m_gview->centerOn(QPointF(m_gview->sceneRect().width() / 2.0, y));

    if (!m_suppress_history_recording)
    {
    }
}

// Go to next page
void
DocumentView::GotoNextPage() noexcept
{
    if (m_pageno >= m_model->numPages() - 1)
        return;

#ifndef NDEBUG
    qDebug() << "DocumentView::GotoNextPage(): Going to next page from"
             << m_pageno;
#endif
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
#ifndef NDEBUG
    qDebug()
        << "DocumentView::clearSearchHits(): Clearing previous search hits";
#endif
    for (auto *item : m_search_items)
    {
        if (item && item->scene() == m_gscene)
            item->setPath(QPainterPath()); // clear instead of delete
    }
    m_search_index = -1;
    m_search_items.clear();
    m_search_hits.clear();
    m_search_hit_flat_refs.clear();
    m_vscroll->setSearchMarkers({});
}

// Perform search for the given term
void
DocumentView::Search(const QString &term) noexcept
{
#ifndef NDEBUG
    qDebug() << "DocumentView::Search(): Searching for term:" << term;
#endif

    clearSearchHits();
    if (term.isEmpty())
    {
        m_current_search_hit_item->setPath(QPainterPath());
        return;
    }

    // Check if term has atleast one uppercase letter
    bool caseSensitive = std::any_of(term.cbegin(), term.cend(),
                                     [](QChar c) { return c.isUpper(); });

    // m_search_hits = m_model->search(term);
    m_model->search(term, caseSensitive);
}

// Function that is common to zoom-in and zoom-out
void
DocumentView::zoomHelper() noexcept
{
#ifndef NDEBUG
    qDebug() << "DocumentView::zoomHelper(): Zooming from" << m_current_zoom
             << "to" << m_target_zoom;
#endif
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

    renderSearchHitsInScrollbar();

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
    zoomHelper();
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
#ifndef NDEBUG
    qDebug() << "DocumentView::GotoHit(): Going to search hit index:" << index;
#endif

    if (index < 0)
        index = m_search_hit_flat_refs.size() - 1;
    else if (index >= m_search_hit_flat_refs.size())
        index = 0;
    const HitRef ref = m_search_hit_flat_refs[index];

    m_search_index = index;
    m_pageno       = ref.page;

    GotoPage(ref.page);
    updateCurrentHitHighlight();
    emit searchIndexChanged(index);
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
    m_model->close();
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

    m_model->highlightTextSelection(pageIndex, m_selection_start,
                                    m_selection_end);

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

#ifndef NDEBUG
    qDebug() << "ClearTextSelection(): Clearing text selection";
#endif

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
    if (m_loc_history.empty())
        return;

#ifndef NDEBUG
    qDebug() << "DocumentView::GoBackHistory(): Going back in history";
#endif

    const PageLocation loc = m_loc_history.back();
    m_loc_history.pop_back();
    m_suppress_history_recording = true;
    GotoLocation(loc);
    m_suppress_history_recording = false;
}

// Get the list of currently visible pages
std::set<int>
DocumentView::getVisiblePages() noexcept
{
#ifndef NDEBUG
    qDebug() << "DocumentView::getVisiblePages(): Calculating visible pages";
#endif
    std::set<int> visiblePages;

    const QRectF visibleSceneRect
        = m_gview->mapToScene(m_gview->viewport()->rect()).boundingRect();

    double top    = visibleSceneRect.top();
    double bottom = visibleSceneRect.bottom();
    top -= m_preload_margin;
    bottom += m_preload_margin;

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
    updateCurrentHitHighlight();
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

    m_preload_margin = m_page_stride; // preload one page above and below
    if (m_config.behavior.cache_pages > 0)
        m_preload_margin *= m_config.behavior.cache_pages;
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
#ifndef NDEBUG
    qDebug() << "DocumentView::pageAtScenePos(): Checking for page at scene "
             << "position:" << scenePos;
#endif

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
#ifndef NDEBUG
    qDebug() << "DocumentView::handleContextMenuRequested(): Context menu "
             << "requested at scene position:" << scenePos;
#endif
    QMenu *menu = new QMenu(this);
    menu->move(scenePos.x(), scenePos.y());

    auto addAction = [this, &menu](const QString &text, const auto &slot)
    {
        QAction *action = new QAction(text, menu); // sets parent = menu
        connect(action, &QAction::triggered, this, slot);
        menu->addAction(action);
    };

    // if (m_hit_pixmap)
    // {
    //     menu->show();
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
    {
        m_current_search_hit_item->setPath(QPainterPath());
        return;
    }

    QPolygonF poly;
    poly.reserve(4);
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

    // 4. Only update the underlying QGraphicsPathItem if the path has actually
    // changed
    if (m_current_search_hit_item->path() != path)
    {
        m_current_search_hit_item->setPath(path);
    }

    // m_current_search_hit_item->setPath(path);
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

#ifndef NDEBUG
    qDebug() << "updateCurrentPage(): Current page changed to:" << page;
#endif

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
            GotoLocation(m_pending_jump);
            centerOnPage(m_pending_jump.pageno);
            m_pending_jump = {-1, 0, 0};
        }

        // NEW: If the page we just rendered is the page the user is currently
        // looking for in search
        if (m_search_index != -1 && !m_search_hit_flat_refs.empty()
            && m_search_hit_flat_refs[m_search_index].page == pageno)
        {
            updateCurrentHitHighlight();
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
                connect(
                    link, &BrowseLinkItem::horizontalFitRequested, this,
                    [this](int pageno, const BrowseLinkItem::PageLocation &loc)
                {
                    GotoLocation({pageno, loc.x, loc.y});
                    setFitMode(FitMode::Width);
                });
            }
            break;

            case BrowseLinkItem::LinkType::FitV:
            {
                connect(
                    link, &BrowseLinkItem::verticalFitRequested, this,
                    [this](int pageno, const BrowseLinkItem::PageLocation &loc)
                {
                    GotoLocation({pageno, loc.x, loc.y});
                    setFitMode(FitMode::Height);
                });
            }
            break;

            case BrowseLinkItem::LinkType::Page:
            {
                connect(link, &BrowseLinkItem::jumpToPageRequested, this,
                        [this, pageno](int targetPageno,
                                       const BrowseLinkItem::PageLocation
                                           &sourceLocationOfLink)
                {
                    const DocumentView::PageLocation targetLocation{
                        targetPageno, 0, 0};
                    const DocumentView::PageLocation sourceLocation{
                        pageno, sourceLocationOfLink.x, sourceLocationOfLink.y};

                    GotoLocation(targetLocation, sourceLocation);
                });
            }
            break;

            case BrowseLinkItem::LinkType::Location:
            {

                connect(link, &BrowseLinkItem::jumpToLocationRequested, this,
                        [this, pageno](int targetPageno,
                                       const BrowseLinkItem::PageLocation
                                           &targetLocationOfLink,
                                       const BrowseLinkItem::PageLocation
                                           &sourceLocationOfLink)
                {
                    qDebug() << "-------------------------------";
                    qDebug() << "Link clicked to go to page" << targetPageno
                             << "at location" << targetLocationOfLink.x << ","
                             << targetLocationOfLink.y;

                    qDebug() << "Source location is on page" << pageno << "at"
                             << sourceLocationOfLink.x << ","
                             << sourceLocationOfLink.y;
                    qDebug() << "-------------------------------";

                    const DocumentView::PageLocation targetLocation{
                        targetPageno, targetLocationOfLink.x,
                        targetLocationOfLink.y};

                    const DocumentView::PageLocation sourceLocation{
                        pageno, sourceLocationOfLink.x, sourceLocationOfLink.y};

                    GotoLocation(targetLocation, sourceLocation);
                });
            }
            break;

            default:
                break;
        }

        connect(link, &BrowseLinkItem::linkCopyRequested, this,
                [this](const QString &link)
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

// Render search hits in the scrollbar
void
DocumentView::renderSearchHitsInScrollbar() noexcept
{
    if (m_search_hit_flat_refs.empty())
        return;

    std::vector<double> search_markers_pos;
    search_markers_pos.reserve(m_search_hit_flat_refs.size());

    // Scale factor to convert PDF points to current scene pixels
    const double pdfToSceneScale
        = (m_model->DPI() / 72.0) * m_current_zoom * m_model->DPR();

    for (const auto &hitRef : m_search_hit_flat_refs)
    {
        const auto &hit = m_search_hits[hitRef.page][hitRef.indexInPage];

        // 1. Calculate the start of the page in the scene
        double pageTopInScene = hitRef.page * m_page_stride;

        // 2. Calculate the Y offset within the page
        // We use the same scaling logic the renderer uses for placement
        double yOffsetInScene = hit.quad.ul.y * pdfToSceneScale;

        search_markers_pos.push_back(pageTopInScene + yOffsetInScene);
    }

    m_vscroll->setSearchMarkers(search_markers_pos);
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
    }
}

void
DocumentView::centerOnPage(int pageno) noexcept
{
    GraphicsPixmapItem *pageItem = m_page_items_hash.value(pageno, nullptr);
    if (!pageItem)
        return;

#ifndef NDEBUG
    qDebug() << "centerOnPage(): Centering on page:" << pageno;
#endif

    const QPointF localPos = QPointF(pageItem->boundingRect().width() / 2.0,
                                     pageItem->boundingRect().height() / 2.0);
    const QPointF scenePos = pageItem->mapToScene(localPos);
    m_gview->centerOn(scenePos);
}

void
DocumentView::OpenHitPixmapInExternalViewer() noexcept
{
}

void
DocumentView::SaveImageAs() noexcept
{
}

void
DocumentView::CopyImageToClipboard() noexcept
{
}
