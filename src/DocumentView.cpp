#include "DocumentView.hpp"

#include "BrowseLinkItem.hpp"
#include "GraphicsView.hpp"
#include "HighlightsDialog.hpp"

#include <QClipboard>
#include <algorithm>
#include <mupdf/fitz/context.h>
#include <mupdf/fitz/image.h>
#include <mupdf/fitz/output.h>
#include <mupdf/fitz/write-pixmap.h>
#include <qcolordialog.h>
#include <qcontainerinfo.h>
#include <qgraphicsitem.h>
#include <qnamespace.h>
#include <qstandardpaths.h>
#include <qthread.h>

#ifdef NDEBUG
#else
#include <sys/resource.h>
struct rusage usage;
#endif

DocumentView::DocumentView(const QString &fileName, const Config &config,
                           QWidget *parent)
    : QWidget(parent), m_config(config)
{
    setAcceptDrops(false);
    // important for tab differentiating doc and dodo internal tabs like startup
    // and other tabs
    setProperty("tabRole", "doc");
    m_page_history_list.reserve(m_page_history_limit);
    m_gview->setPageNavWithMouse(m_page_nav_with_mouse);
    m_model = new Model(m_gscene, this);
    m_pix_item->setScale(m_model->zoom());
    m_vscrollbar = m_gview->verticalScrollBar();
    m_hscrollbar = m_gview->horizontalScrollBar();
    m_gscene->addItem(m_pix_item);
    m_gview->setPixmapItem(m_pix_item);
    m_gview->setEnabled(false);
    m_gview->setRenderHint(QPainter::Antialiasing);
    m_gview->setScene(m_gscene);
    m_gview->setMode(m_config.behavior.initial_mode);
    m_default_mode = m_config.behavior.initial_mode;

    m_jump_marker = new JumpMarker(m_config.ui.colors["jump_marker"]);
    m_gscene->addItem(m_jump_marker);
    m_gview->setBackgroundBrush(QColor(m_config.ui.colors["background"]));
    m_model->setDPI(m_config.rendering.dpi);
    m_model->setAnnotRectColor(
        QColor(m_config.ui.colors["annot_rect"]).toRgb());
    m_model->setSelectionColor(m_config.ui.colors["selection"]);
    m_model->setHighlightColor(m_config.ui.colors["highlight"]);
    m_model->setAntialiasingBits(m_config.rendering.antialiasing_bits);
    m_model->undoStack()->setUndoLimit(m_config.behavior.undo_limit);

    if (m_config.behavior.invert_mode)
        m_model->toggleInvertColor();
    m_cache.setMaxCost(m_config.behavior.cache_pages);

    if (m_config.rendering.icc_color_profile)
        m_model->enableICC();

    if (m_config.ui.initial_fit == "height")
        m_initial_fit = FitMode::Height;
    else if (m_config.ui.initial_fit == "width")
        m_initial_fit = FitMode::Width;
    else if (m_config.ui.initial_fit == "window")
        m_initial_fit = FitMode::Window;
    else
        m_initial_fit = FitMode::None;

    m_layout->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(m_gview);

    m_gview->setContentsMargins(0, 0, 0, 0);
    m_layout->setContentsMargins(0, 0, 0, 0);
    this->setContentsMargins(0, 0, 0, 0);

    this->setLayout(m_layout);

    m_model->setLinkBoundary(m_config.ui.link_boundary);

    if (!m_config.ui.vscrollbar_shown)
        m_gview->setVerticalScrollBarPolicy(
            Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

    if (!m_config.ui.hscrollbar_shown)
        m_gview->setHorizontalScrollBarPolicy(
            Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

    initConnections();
    openFile(fileName);

#ifdef NDEBUG
#else
    getrusage(RUSAGE_SELF, &usage);
    qDebug() << "Mem (MB): " << usage.ru_maxrss / 1024;
#endif
}

DocumentView::~DocumentView()
{
    synctex_scanner_free(m_synctex_scanner);
    fz_drop_pixmap(m_model->context(), m_hit_pixmap);
}

void
DocumentView::setDPR(qreal DPR) noexcept
{
    m_cache.clear();
    m_model->setDPR(DPR);
    m_dpr     = DPR;
    m_inv_dpr = 1 / DPR;
    if (m_pageno != -1)
        renderPage(m_pageno, true);
}

void
DocumentView::initConnections() noexcept
{
    connect(m_gview, &GraphicsView::scrollRequested, this,
            &DocumentView::mouseWheelScrollRequested);
    connect(m_gview, &GraphicsView::populateContextMenuRequested, this,
            &DocumentView::populateContextMenu);
    connect(m_model, &Model::documentSaved, this, [&]() { setDirty(false); });

    connect(m_gview, &GraphicsView::rightClickRequested, this,
            [&](QPointF scenePos)
    { m_hit_pixmap = m_model->hitTestImage(m_pageno, scenePos); });

    connect(
        m_model, &Model::searchResultsReady, this,
        [&](const QMap<int, QList<Model::SearchResult>> &maps, int matchCount)
    {
        if (matchCount == 0)
        {
            QMessageBox::information(
                this, "Search Result",
                QString("No match found for %1").arg(m_last_search_term));
            return;
        }
        m_searchRectMap = maps;

        emit searchCountChanged(matchCount);

        auto page = maps.firstKey();
        jumpToHit(page, 0);
    });

    connect(m_gview, &GraphicsView::highlightDrawn, m_model,
            [&](const QRectF &rect)
    {
        m_model->addRectAnnotation(rect);
        setDirty(true);
        renderPage(m_pageno, true);
    });

    connect(m_model, &Model::horizontalFitRequested, this,
            [&](int pageno, const BrowseLinkItem::Location &location)
    {
        gotoPage(pageno);
        FitWidth();
        scrollToNormalizedTop(location.y);
    });

    connect(m_model, &Model::verticalFitRequested, this,
            [&](int pageno, const BrowseLinkItem::Location &location)
    {
        gotoPage(pageno, false);
        FitHeight();
        scrollToNormalizedTop(location.y);
    });

    connect(m_model, &Model::jumpToPageRequested, this, [&](int pageno)
    {
        gotoPage(pageno, false);
        TopOfThePage();
    });

    connect(m_model, &Model::jumpToLocationRequested, this,
            [&](int pageno, const BrowseLinkItem::Location &loc)
    {
        gotoPage(pageno, false);
        scrollToXY(loc.x, loc.y);
    });

    connect(m_model, &Model::pageRenderRequested, this,
            &DocumentView::renderPage);

    connect(m_gview, &GraphicsView::synctexJumpRequested, this,
            &DocumentView::synctexJumpRequested);

    connect(m_gview, &GraphicsView::textSelectionRequested, m_model,
            [&](const QPointF &start, const QPointF &end)
    {
        if (start != end)
        {
            m_model->highlightTextSelection(start, end);
            m_selection_present = true;
        }
    });

    connect(m_gview, &GraphicsView::textHighlightRequested, m_model,
            [&](const QPointF &start, const QPointF &end)
    {
        m_model->annotHighlightSelection(start, end);
        setDirty(true);
        renderPage(m_pageno, true);
    });

    connect(m_gview, &GraphicsView::textSelectionDeletionRequested, this,
            &DocumentView::ClearTextSelection);

    // QRectF version
    connect(m_gview,
            QOverload<const QRectF &>::of(&GraphicsView::annotSelectRequested),
            m_model, [this](const QRectF &area)
    {
        m_selected_annots = m_model->getAnnotationsInArea(area);
        selectAnnots();
    });

    // QPointF version (if exists)
    connect(m_gview,
            QOverload<const QPointF &>::of(&GraphicsView::annotSelectRequested),
            m_model, [this](const QPointF &p)
    {
        m_selected_annots = QSet<int>({m_model->getAnnotationAtPoint(p)});
        selectAnnots();
    });

    connect(m_gview, &GraphicsView::regionSelectRequested, m_model,
            [&](const QRectF &area)
    {
        // Show a context menu to choose what to do with the selected region
        QMenu menu(this);
        menu.addAction("Copy Text", this,
                       [this, area]() { CopyTextFromRegion(area); });
        menu.addAction("Copy Image", this, [this, area]()
        {
            // Scale the area according to DPR
            const QRectF scaledArea
                = QRectF(area.x() * m_dpr, area.y() * m_dpr,
                         area.width() * m_dpr, area.height() * m_dpr);
            CopyRegionAsImage(scaledArea);
        });
        menu.addAction("Save Image", this, [this, area]()
        {
            // Scale the area according to DPR
            const QRectF scaledArea
                = QRectF(area.x() * m_dpr, area.y() * m_dpr,
                         area.width() * m_dpr, area.height() * m_dpr);
            SaveRegionAsImage(scaledArea);
        });
        menu.addAction("Open image using external viewer", this, [this, area]()
        {
            const QRectF scaledArea
                = QRectF(area.x() * m_dpr, area.y() * m_dpr,
                         area.width() * m_dpr, area.height() * m_dpr);
            OpenRegionInExternalViewer(scaledArea);
        });
        menu.exec(QCursor::pos());
        m_gview->rubberBand()->hide();
    });

    connect(m_gview, &GraphicsView::annotSelectClearRequested, this,
            &DocumentView::clearAnnotSelection);
    connect(m_gview, &GraphicsView::zoomInRequested, this,
            &DocumentView::ZoomIn);
    connect(m_gview, &GraphicsView::zoomOutRequested, this,
            &DocumentView::ZoomOut);
}

inline uint
qHash(const DocumentView::CacheKey &key, uint seed = 0)
{
    seed ^= uint(key.page) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= uint(key.rotation) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= uint(key.scale) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

void
DocumentView::selectAnnots() noexcept
{
    for (const auto &item : m_gscene->items())
    {
        if (item->data(0).toString() == "annot")
        {
            Annotation *gitem = static_cast<Annotation *>(item);
            if (gitem && m_selected_annots.contains(gitem->index()))
                gitem->select(Qt::black);
        }
    }
    m_annot_selection_present = true;
}

void
DocumentView::ClearTextSelection() noexcept
{
    if (!m_selection_present)
        return;

    for (auto &object : m_gscene->items())
    {
        if (object->data(0).toString() == "selection")
            m_gscene->removeItem(object);
    }
    m_selection_present = false;
}

void
DocumentView::scrollToXY(float x, float y) noexcept
{
    if (!m_pix_item || !m_gview)
        return;

    fz_point point{.x = x * m_inv_dpr, .y = y * m_inv_dpr};
    point = fz_transform_point(point, m_model->transform());

    if (m_jump_marker_shown)
        showJumpMarker(point);

    m_gview->centerOn(QPointF(point.x, point.y));
    ClearTextSelection();
}

void
DocumentView::CloseFile(bool skipUnsavedCheck) noexcept
{
    if (!skipUnsavedCheck && m_model->hasUnsavedChanges())
    {
        auto action = QMessageBox::question(
            this, "Unsaved changes detected",
            "There are unsaved changes in this document. Do you really want to "
            "close this file ?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Cancel);

        switch (action)
        {
            case QMessageBox::StandardButton::Save:
                SaveFile();
                break;

            case QMessageBox::StandardButton::Discard:
                break;

            case QMessageBox::Cancel:
            default:
                return;
        }
    }

    if (m_config.behavior.remember_last_visited && !m_filename.isEmpty()
        && m_pageno >= 0)
        emit insertToDBRequested(m_filename, m_pageno + 1);

    m_cache.clear();

    clearLinks();

    emit searchModeChanged(false);

    if (m_highlights_present)
    {
        clearHighlights();
        clearIndexHighlights();
    }

    if (m_selection_present)
        ClearTextSelection();

    if (m_annot_selection_present)
        clearAnnotSelection();

    m_filename.clear();
    emit fileNameChanged(QString());

    if (m_owidget)
    {
        m_owidget->close();
        m_owidget->deleteLater();
        m_owidget = nullptr;
    }

    if (m_propsWidget)
    {
        m_propsWidget->close();
        m_propsWidget->deleteLater();
        m_propsWidget = nullptr;
    }

    if (!m_pix_item->pixmap().isNull())
        m_pix_item->setPixmap(QPixmap());
    m_gview->setSceneRect(m_pix_item->boundingRect());
    m_model->closeFile();
    m_gview->setEnabled(false);
}

void
DocumentView::clearPixmapItems() noexcept
{
    const auto items = m_pix_item->childItems();
    if (items.isEmpty())
        return;

    for (QGraphicsItem *item : items)
        m_gscene->removeItem(item);
}

/* Function for opening the file using the model.
   For internal usage only */
void
DocumentView::openFile(const QString &fileName) noexcept
{
    if (m_model->valid())
        CloseFile();

    m_filename = fileName;

    if (m_filename.contains("~"))
        m_filename.replace(0, 1, getenv("HOME"));

    clearPixmapItems();

    if (!QFile::exists(m_filename))
    {
        qCritical() << "File does not exist: " << m_filename;
        return;
    }

    if (!m_model->openFile(m_filename))
    {
        QMessageBox::critical(this, "Error opening document",
                              "Unable to open document for some reason");
        return;
    }

    if (m_model->passwordRequired())
        if (!askForPassword())
            return;

    m_pageno      = -1;
    m_total_pages = m_model->numPages();
    emit totalPageCountChanged(m_total_pages);
    emit searchModeChanged(false);

    m_basename = QFileInfo(m_filename).fileName();

    if (m_start_page_override >= 0)
        m_pageno = m_start_page_override;

    if (m_pageno != -1)
    {
        gotoPage(m_pageno);
        return;
    }

    int targetPage = 0;

    m_gview->setEnabled(true);
    gotoPage(targetPage);

    // Initialize zoom to be equal to fit to height
    int pixmapHeight = m_pix_item->pixmap().height();
    int viewHeight   = m_gview->viewport()->height() * m_dpr;

    qreal scale    = static_cast<qreal>(viewHeight) / pixmapHeight;
    m_default_zoom = scale;

    // updateUiEnabledState();

    if (m_initial_fit != FitMode::None)
    {
        QTimer::singleShot(10, this, [this]()
        {
            switch (m_initial_fit)
            {
                case FitMode::Height:
                    FitHeight();
                    break;
                case FitMode::Width:
                    FitWidth();
                    break;
                case FitMode::Window:
                    FitWindow();
                    break;
                case FitMode::None:
                default:
                    break;
            }
        });
    }

    initSynctex();

    QString title = m_config.ui.window_title_format;
    title         = title.replace("%1", m_basename);
    setWindowTitle(title);
    m_last_modified_time = QFileInfo(m_filename).lastModified();
    emit fileNameChanged(m_basename);
}

bool
DocumentView::askForPassword() noexcept
{
    bool ok;
    auto pwd
        = QInputDialog::getText(this, "Document is locked", "Enter password",
                                QLineEdit::EchoMode::Password, QString(), &ok);
    if (!ok)
        return false;

    auto correct = m_model->authenticate(pwd);
    if (correct)
        return true;
    else
    {
        QMessageBox::critical(this, "Password", "Password is incorrect");
        return false;
    }
}

void
DocumentView::RotateClock() noexcept
{
    if (!m_pix_item)
        return;

    m_rotation = (m_rotation + 90) % 360;
    m_model->setRotation(m_rotation);

    m_cache.clear();
    renderPage(m_pageno, true);

    QRectF bbox
        = m_pix_item->mapToScene(m_pix_item->boundingRect()).boundingRect();
    m_gview->setSceneRect(bbox);
    m_gview->centerOn(bbox.center()); // center view
}

void
DocumentView::RotateAntiClock() noexcept
{
    if (!m_pix_item)
        return;

    m_rotation = (m_rotation + 270) % 360;
    m_model->setRotation(m_rotation);
    m_cache.clear();
    renderPage(m_pageno, true);
    QRectF bbox
        = m_pix_item->mapToScene(m_pix_item->boundingRect()).boundingRect();
    m_gview->setSceneRect(bbox);
    m_gview->centerOn(bbox.center()); // center view
}

bool
DocumentView::gotoPage(int pageno, bool refresh) noexcept
{
    if (!m_model->valid())
    {
        qWarning("Trying to go to page but no document loaded");
        return false;
    }

    if (pageno < 0 || pageno >= m_total_pages)
        return false;

    // boundary condition
    if (!m_suppressHistory)
    {
        fz_point p = mapToPdf(m_gview->getCursorPos());
        m_page_history_list.push_back((HistoryLocation){
            .x = p.x, .y = p.y, .zoom = 1, .pageno = m_pageno});
        if (m_page_history_list.size() > m_page_history_limit)
            m_page_history_list.removeFirst();
    }

    // TODO: Handle file content change detection

    if (m_highlights_present)
    {
        clearHighlights();
        clearIndexHighlights();
    }

    if (m_link_hints_present)
        clearKBHintsOverlay();

    if (m_annot_selection_present)
        clearAnnotSelection();

    if (m_selection_present)
        ClearTextSelection();

    return gotoPageInternal(pageno, refresh);
}

bool
DocumentView::gotoPageInternal(int pageno, bool refresh) noexcept
{
    m_pageno = pageno;
    return renderPage(pageno, refresh);
}

void
DocumentView::clearLinks() noexcept
{
    for (auto &link : m_gscene->items())
        if (link->data(0).toString() == "link")
            m_gscene->removeItem(link);
}

void
DocumentView::renderAnnotations(const QList<Annotation *> &annots) noexcept
{
    clearAnnotations();
    for (Annotation *annot : annots)
    {
        m_gscene->addItem(annot);
        connect(annot, &Annotation::annotDeleteRequested, m_model,
                [&](int index)
        {
            m_model->annotDeleteRequested(index);
            setDirty(true);
            renderPage(m_pageno, true);
        });

        connect(annot, &Annotation::annotColorChangeRequested, m_model,
                [this, annot](int index)
        {
            auto color = QColorDialog::getColor(
                annot->data(3).value<QColor>(), this, "Highlight Color",
                QColorDialog::ColorDialogOption::ShowAlphaChannel);
            if (color.isValid())
            {
                m_model->annotChangeColorForIndex(index, color);
                setDirty(true);
                renderPage(m_pageno, true);
            }
        });
    }
}

void
DocumentView::clearAnnotations() noexcept
{
    for (auto &link : m_gscene->items())
    {
        if (link->data(0).toString() == "annot")
        {
            m_gscene->removeItem(link);
        }
    }
}

void
DocumentView::renderLinks(const QList<BrowseLinkItem *> &links) noexcept
{
    clearLinks();
    for (auto *link : links)
    {
        connect(link, &BrowseLinkItem::linkCopyRequested, this,
                [&](const QString &link)
        {
            if (link.startsWith("#"))
            {
                auto equal_pos = link.indexOf("=");
                emit clipboardContentChanged(m_filename + "#"
                                             + link.mid(equal_pos + 1));
            }
            else
            {
                emit clipboardContentChanged(link);
            }
        });
        m_gscene->addItem(link);
    }
}

bool
DocumentView::renderPage(int pageno, bool refresh) noexcept
{
    if (!m_model->valid())
        return false;

    // Cache only if m_config.behavior.cache_pages > 0
    CacheKey key{pageno, m_rotation, m_model->zoom()};

    emit pageNumberChanged(pageno + 1);

    if (m_config.behavior.cache_pages && !refresh)
    {
        if (auto *cached = m_cache.object(key))
        {
#ifdef NDEBUG
#else
            qDebug() << "Using cached pixmap";
#endif
            // Update text page and transform for text selection even when
            // using cached pixmap
            m_model->renderPage(pageno, true);

            renderPixmap(cached->pixmap);
            m_link_items = cached->links;
            renderLinks(m_link_items);
            renderAnnotations(cached->annot_highlights);

            return true;
        }
    }

    QPixmap pix                          = m_model->renderPage(pageno);
    m_link_items                         = m_model->getLinks();
    QList<Annotation *> annot_highlights = m_model->getAnnotations();

    if (m_config.behavior.cache_pages > 0)
        m_cache.insert(key,
                       new CacheValue(pix, m_link_items, annot_highlights));

    renderPixmap(pix);
    renderLinks(m_link_items);
    renderAnnotations(annot_highlights);

    return true;
}

void
DocumentView::renderPixmap(const QPixmap &pix) noexcept
{
    m_pix_item->setPixmap(pix);
}

bool
DocumentView::FirstPage() noexcept
{
    auto dd = gotoPage(0, false);
    if (dd)
        TopOfThePage();
    else
        return false;
    return true;
}

bool
DocumentView::LastPage() noexcept
{
    return gotoPage(m_total_pages - 1, false);
}

bool
DocumentView::NextPage() noexcept
{
    return gotoPage(m_pageno + 1, false);
}

bool
DocumentView::PrevPage() noexcept
{
    return gotoPage(m_pageno - 1, false);
}

void
DocumentView::ZoomIn() noexcept
{
    if (!m_model->valid())
        return;
    m_model->setZoom(m_model->zoom() * m_zoom_by);
    zoomHelper();
}

void
DocumentView::ZoomReset() noexcept
{
    if (!m_model->valid())
        return;

    m_model->setZoom(m_default_zoom);
    zoomHelper();
}

void
DocumentView::ZoomOut() noexcept
{
    if (!m_model->valid())
        return;

    float zoom = m_model->zoom();

    if (zoom * 1 / m_zoom_by != 0)
    {
        m_model->setZoom(zoom * 1 / m_zoom_by);
        zoomHelper();
    }
}

void
DocumentView::Zoom(float factor) noexcept
{
    if (!m_model->valid())
        return;

    m_model->setZoom(factor);
    zoomHelper();
}

void
DocumentView::zoomHelper() noexcept
{
    renderPage(m_pageno);
    if (m_highlights_present)
    {
        highlightSingleHit();
        highlightHitsInPage();
    }
    if (m_selection_present)
        ClearTextSelection();
    m_gview->setSceneRect(m_pix_item->boundingRect());
    m_vscrollbar->setValue(m_vscrollbar->value());
    m_cache.clear();
}

void
DocumentView::ScrollDown() noexcept
{
    m_vscrollbar->setValue(m_vscrollbar->value() + 50);
}

void
DocumentView::ScrollUp() noexcept
{
    m_vscrollbar->setValue(m_vscrollbar->value() - 50);
}

void
DocumentView::ScrollLeft() noexcept
{
    auto hscrollbar = m_gview->horizontalScrollBar();
    hscrollbar->setValue(hscrollbar->value() - 50);
}

void
DocumentView::ScrollRight() noexcept
{
    auto hscrollbar = m_gview->horizontalScrollBar();
    hscrollbar->setValue(hscrollbar->value() + 50);
}

void
DocumentView::FitHeight() noexcept
{
    if (!m_pix_item || m_pix_item->pixmap().isNull())
        return;

    int pixmapHeight = m_pix_item->pixmap().height();
    int viewHeight   = m_gview->viewport()->height() * m_dpr;

    qreal scale = static_cast<qreal>(viewHeight) / pixmapHeight;
    m_model->setZoom(m_model->zoom() * scale);
    zoomHelper();
    setFitMode(FitMode::Height);
}

void
DocumentView::FitWidth() noexcept
{
    if (!m_pix_item || m_pix_item->pixmap().isNull())
        return;

    int pixmapWidth = m_pix_item->pixmap().width();
    int viewWidth   = m_gview->viewport()->width() * m_dpr;

    qreal scale = static_cast<qreal>(viewWidth) / pixmapWidth;
    m_model->setZoom(m_model->zoom() * scale);
    zoomHelper();
    setFitMode(FitMode::Width);
}

void
DocumentView::FitWindow() noexcept
{
    const int pixmapWidth  = m_pix_item->pixmap().width();
    const int pixmapHeight = m_pix_item->pixmap().height();
    const int viewWidth    = m_gview->viewport()->width() * m_dpr;
    const int viewHeight   = m_gview->viewport()->height() * m_dpr;

    // Calculate scale factors for both dimensions
    const qreal scaleX = static_cast<qreal>(viewWidth) / pixmapWidth;
    const qreal scaleY = static_cast<qreal>(viewHeight) / pixmapHeight;

    // Use the smaller scale to ensure the entire image fits in the window
    const qreal scale = std::min(scaleX, scaleY);
    m_model->setZoom(m_model->zoom() * scale);
    zoomHelper();
    setFitMode(FitMode::Window);
}

void
DocumentView::FitNone() noexcept
{
    setFitMode(FitMode::None);
}

void
DocumentView::setFitMode(const FitMode &mode) noexcept
{
    m_fit_mode = mode;
    switch (m_fit_mode)
    {
        case FitMode::Height:
            emit fitModeChanged(FitMode::Height);
            break;

        case FitMode::Width:
            emit fitModeChanged(FitMode::Width);
            break;

        case FitMode::Window:
            emit fitModeChanged(FitMode::Window);
            break;

        case FitMode::None:
            emit fitModeChanged(FitMode::None);
            break;

        default:
            break;
    }
}

// Single page search
void
DocumentView::search(const QString &term) noexcept
{
    m_last_search_term = term;
    // m_pdf_backend->search();
}

void
DocumentView::jumpToHit(int page, int index)
{
    if (m_searchRectMap[page].empty() || index < 0
        || index >= m_searchRectMap[page].size())
        return;

    m_search_index    = index;
    m_search_hit_page = page;

    gotoPage(page, false); // Render page
    emit searchIndexChanged(m_searchRectMap[page][index].index + 1);

    highlightSingleHit();
    highlightHitsInPage();
}

void
DocumentView::highlightHitsInPage()
{
    clearHighlights();

    auto results = m_searchRectMap[m_pageno];

    for (const auto &result : results)
    {
        fz_quad quad      = result.quad;
        QRectF scaledRect = fzQuadToQRect(quad);
        auto *highlight   = new QGraphicsRectItem(scaledRect);

        highlight->setBrush(QColor(m_config.ui.colors["search_match"]));
        highlight->setPen(Qt::NoPen);
        highlight->setData(0, "searchHighlight");
        m_gscene->addItem(highlight);
        // m_gview->centerOn(highlight);
    }
    m_highlights_present = true;
}

void
DocumentView::highlightSingleHit() noexcept
{
    if (m_highlights_present)
        clearIndexHighlights();

    fz_quad quad = m_searchRectMap[m_pageno][m_search_index].quad;

    QRectF scaledRect = fzQuadToQRect(quad);
    auto *highlight   = new QGraphicsRectItem(scaledRect);
    highlight->setBrush(QColor(m_config.ui.colors["search_index"]));
    highlight->setPen(Qt::NoPen);
    highlight->setData(0, "searchIndexHighlight");

    m_gscene->addItem(highlight);
    m_gview->centerOn(highlight);
    m_highlights_present = true;
}

void
DocumentView::clearIndexHighlights()
{
    for (auto item : m_gscene->items())
    {
        if (auto rect = qgraphicsitem_cast<QGraphicsRectItem *>(item))
        {
            if (rect->data(0).toString() == "searchIndexHighlight")
            {
                m_gscene->removeItem(rect);
                delete rect;
            }
        }
    }
    m_highlights_present = false;
}

void
DocumentView::clearHighlights()
{
    for (auto item : m_gscene->items())
    {
        if (auto rect = qgraphicsitem_cast<QGraphicsRectItem *>(item))
        {
            if (rect->data(0).toString() == "searchHighlight")
            {
                m_gscene->removeItem(rect);
                delete rect;
            }
        }
    }
    m_highlights_present = false;
}

void
DocumentView::Search(const QString &term) noexcept
{
    if (!m_model->valid())
        return;

    m_searchRectMap.clear();
    m_search_index = -1;
    if (term.isEmpty() || term.isNull())
    {
        emit searchModeChanged(false);
        if (m_highlights_present)
        {
            clearHighlights();
            clearIndexHighlights();
        }
        return;
    }

    m_last_search_term = term;
    emit searchModeChanged(true);
    m_search_index = 0;

    m_model->searchAll(term, hasUpperCase(term));
}

void
DocumentView::NextHit()
{
    if (m_search_hit_page == -1)
        return;

    QList<int> pages   = m_searchRectMap.keys();
    int currentPageIdx = pages.indexOf(m_search_hit_page);

    // Try next hit on current page
    if (m_search_index + 1 < m_searchRectMap[m_search_hit_page].size())
    {
        jumpToHit(m_search_hit_page, m_search_index + 1);
        return;
    }

    // Try next page
    if (currentPageIdx + 1 < pages.size())
    {
        if (!m_searchRectMap[pages[currentPageIdx + 1]].isEmpty())
            jumpToHit(pages[currentPageIdx + 1], 0);
    }
}

void
DocumentView::PrevHit()
{
    if (m_search_hit_page == -1)
        return;

    QList<int> pages   = m_searchRectMap.keys();
    int currentPageIdx = pages.indexOf(m_search_hit_page);

    // Try previous hit on current page
    if (m_search_index - 1 >= 0)
    {
        jumpToHit(m_search_hit_page, m_search_index - 1);
        return;
    }

    // Try previous page
    if (currentPageIdx - 1 >= 0)
    {
        int prevPage     = pages[currentPageIdx - 1];
        int lastHitIndex = m_searchRectMap[prevPage].size() - 1;
        jumpToHit(prevPage, lastHitIndex);
    }
}

void
DocumentView::GoBackHistory() noexcept
{
    if (!m_page_history_list.isEmpty())
    {
        HistoryLocation loc = m_page_history_list.takeLast(); // Pop last page
        m_suppressHistory   = true;
        gotoPage(loc.pageno);
        scrollToXY(loc.x, loc.y);
        m_suppressHistory = false;
    }
}

void
DocumentView::closeEvent(QCloseEvent *e)
{
    // Unsaved changes are handled by:
    // - dodo::closeEvent() when closing the main window
    // - CloseFile() when closing a tab via the tab close button
    // So we just accept the close event here
    e->accept();
}

void
DocumentView::scrollToNormalizedTop(double ntop) noexcept
{
    // 1. Get scene Y position of the top of the page
    qreal pageSceneY = m_pix_item->sceneBoundingRect().top();

    // 2. Get full height of the page
    qreal pageHeight = m_pix_item->boundingRect().height();

    // 3. Compute desired Y position in scene coords
    qreal targetSceneY = pageSceneY + ntop * pageHeight;

    // 4. Convert scene Y to viewport Y
    QPointF targetViewportPoint
        = m_gview->mapFromScene(QPointF(0, targetSceneY));

    // 5. Compute the scroll position required
    int scrollPos = m_vscrollbar->value() + targetViewportPoint.y();

    m_vscrollbar->setValue(scrollPos);
}

// void
// DocumentView::ShowOutline() noexcept
// {
//     if (!m_model->valid())
//         return;
//
//     if (!m_owidget)
//     {
//         m_owidget = new OutlineWidget(m_model->clonedContext(), this);
//         connect(m_owidget, &OutlineWidget::jumpToPageRequested, this,
//                 [&](int pageno) { gotoPage(pageno); });
//     }
//
//     if (!m_owidget->hasOutline())
//     {
//         fz_outline *outline = m_model->getOutline();
//         if (!outline)
//         {
//             QMessageBox::information(
//                 this, "Outline", "Document does not have outline
//                 information");
//             return;
//         }
//         m_owidget->setOutline(outline);
//     }
//     m_owidget->open();
// }

void
DocumentView::ToggleRectAnnotation() noexcept
{
    if (m_gview->mode() == GraphicsView::Mode::AnnotRect)
    {
        m_gview->setMode(m_default_mode);
        emit selectionModeChanged(m_default_mode);
    }
    else
    {
        m_gview->setMode(GraphicsView::Mode::AnnotRect);
        emit highlightColorChanged(m_config.ui.colors["annot_rect"]);
        emit selectionModeChanged(GraphicsView::Mode::AnnotRect);
    }
}

void
DocumentView::ToggleRegionSelect() noexcept
{
    if (m_gview->mode() == GraphicsView::Mode::RegionSelection)
    {
        m_gview->setMode(m_default_mode);
        emit selectionModeChanged(m_default_mode);
    }
    else
    {
        m_gview->setMode(GraphicsView::Mode::RegionSelection);
        emit selectionModeChanged(GraphicsView::Mode::RegionSelection);
    }
}

void
DocumentView::SaveFile() noexcept
{
    if (!m_model->valid())
        return;

    m_model->save();
    auto pix = m_model->renderPage(m_pageno);
    renderPixmap(pix);
}

void
DocumentView::SaveAsFile() noexcept
{
    if (!m_model->valid())
        return;

    auto filename = QFileDialog::getSaveFileName(this, "Save as", QString());

    if (filename.isEmpty())
        return;

    if (!m_model->saveAs(CSTR(filename)))
    {
        QMessageBox::critical(
            this, "Saving as failed",
            "Could not perform save as operation on the file");
    }
}

QMap<int, Model::LinkInfo>
DocumentView::LinkKB() noexcept
{
    if (m_link_items.isEmpty())
        return {};
    int i = 10;
    QRectF viewport
        = m_gview->mapToScene(m_gview->viewport()->rect()).boundingRect();
    QMap<int, Model::LinkInfo> map;

    for (const auto &link : m_link_items)
    {
        auto dest = m_model->resolveLink(link->URI());
        map[i] = (Model::LinkInfo){.uri = QString(link->URI()), .dest = dest};

        float w        = 0.25 * m_model->zoom();
        auto link_rect = link->rect();
        QRectF rect(link_rect.topLeft(), QSizeF(w, w));

        if (rect.intersects(viewport))
        {
            LinkHint *linkHint
                = new LinkHint(rect, m_config.ui.colors["link_hint_bg"],
                               m_config.ui.colors["link_hint_fg"], i,
                               w * m_config.ui.link_hint_size);
            m_link_hints.append(linkHint);
            m_gscene->addItem(linkHint);
        }
        i++;
    }
    m_link_hints_present = true;

    return map;
}

void
DocumentView::GotoPage(int pageno) noexcept
{
    if (pageno < 0 || pageno > m_total_pages)
    {
        pageno = QInputDialog::getInt(
            this, "Goto Page",
            QString("Enter enter valid page number range (1 to %1)")
                .arg(m_total_pages),
            0, 1, m_total_pages);
    }
    this->setFocus();
    gotoPage(pageno - 1, false);
}

bool
DocumentView::hasUpperCase(const QString &text) noexcept
{
    for (const auto &c : text)
        if (c.isUpper())
            return true;

    return false;
}

void
DocumentView::FileProperties() noexcept
{
    if (!m_model->valid())
        return;

    if (!m_propsWidget)
    {
        m_propsWidget = new PropertiesWidget(this);
        auto props    = m_model->extractPDFProperties();
        m_propsWidget->setProperties(props);
    }
    m_propsWidget->exec();
}

// Puts the viewport to the top of the page
void
DocumentView::TopOfThePage() noexcept
{
    m_vscrollbar->setValue(0);
}

void
DocumentView::InvertColor() noexcept
{
    if (!m_model->valid())
        return;

    m_cache.clear();
    m_model->toggleInvertColor();
    renderPage(m_pageno, true);
}

void
DocumentView::ToggleAutoResize() noexcept
{
    m_auto_resize = !m_auto_resize;
    emit autoResizeActionUpdate(m_auto_resize);
}

void
DocumentView::resizeEvent(QResizeEvent *e)
{
    if (m_auto_resize)
    {
        switch (m_fit_mode)
        {
            case FitMode::Height:
                FitHeight();
                break;

            case FitMode::Width:
                FitWidth();
                break;

            case FitMode::Window:
                FitWindow();
                break;

            case FitMode::None:
                break;

            default:
                break;
        }
    }

    e->accept();
}

QRectF
DocumentView::fzQuadToQRect(const fz_quad &q) noexcept
{
    fz_quad tq = fz_transform_quad(q, m_model->transform());

    return QRectF(tq.ul.x * m_inv_dpr, tq.ul.y * m_inv_dpr,
                  (tq.ur.x - tq.ul.x) * m_inv_dpr,
                  (tq.ll.y - tq.ul.y) * m_inv_dpr);
}

void
DocumentView::initSynctex() noexcept
{
    m_synctex_scanner
        = synctex_scanner_new_with_output_file(CSTR(m_filename), nullptr, 1);
    if (!m_synctex_scanner)
        return;
}

void
DocumentView::synctexLocateInFile(const char *texFile, int line) noexcept
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
    args.replaceInStrings("%f", texFile);

    QProcess::startDetached(editor, args);
}

void
DocumentView::synctexLocateInPdf(const QString &texFile, int line,
                                 int column) noexcept
{
    if (synctex_display_query(m_synctex_scanner, CSTR(texFile), line, column,
                              -1)
        > 0)
    {
        synctex_node_p node = nullptr;
        int page;
        float x, y;
        while ((node = synctex_scanner_next_result(m_synctex_scanner)))
        {
            page = synctex_node_page(node);
            x    = synctex_node_visible_h(node);
            y    = synctex_node_visible_v(node);
        }
        gotoPage(page - 1, false);
        scrollToXY(x, y);
        showJumpMarker(QPointF(x, y));
    }
}

fz_point
DocumentView::mapToPdf(QPointF loc) noexcept
{
    double x = loc.x() * m_dpr;
    double y = loc.y() * m_dpr;

    // Apply inverse of rendering transform
    fz_matrix invTransform;
    invTransform = fz_invert_matrix(m_model->transform());

    fz_point pt = {static_cast<float>(x), static_cast<float>(y)};
    pt          = fz_transform_point(pt, invTransform);
    return pt;
}

void
DocumentView::YankSelection() noexcept
{
    if (!m_model->valid())
        return;

    emit clipboardContentChanged(m_model->getSelectionText(
        m_gview->selectionStart(), m_gview->selectionEnd()));
    ClearTextSelection();
}

void
DocumentView::ToggleTextHighlight() noexcept
{
    if (m_gview->mode() == GraphicsView::Mode::TextHighlight)
    {
        m_gview->setMode(m_default_mode);
        emit selectionModeChanged(m_default_mode);
        // m_actionTextSelect->setChecked(true);
    }
    else
    {
        m_gview->setMode(GraphicsView::Mode::TextHighlight);
        emit highlightColorChanged(m_config.ui.colors["highlight"]);
        emit selectionModeChanged(GraphicsView::Mode::TextHighlight);
        ClearTextSelection();
    }
}

void
DocumentView::clearAnnotSelection() noexcept
{
    if (!m_annot_selection_present)
        return;

    for (const auto &item : m_gscene->items())
    {
        if (item->data(0).toString() == "annot")
        {
            Annotation *gitem = static_cast<Annotation *>(item);
            if (gitem && m_selected_annots.contains(gitem->index()))
                gitem->restoreBrushPen();
        }
    }

    m_annot_selection_present = false;
}

void
DocumentView::ToggleAnnotSelect() noexcept
{
    if (m_gview->mode() == GraphicsView::Mode::AnnotSelect)
    {

        m_gview->setMode(m_default_mode);
        emit selectionModeChanged(m_default_mode);
        // m_actionTextSelect->setChecked(true);
    }
    else
    {
        m_gview->setMode(GraphicsView::Mode::AnnotSelect);
        emit selectionModeChanged(GraphicsView::Mode::AnnotSelect);
        // m_actionAnnotEdit->setChecked(true);
    }
}

// Copies all the text in the page to clipboard
void
DocumentView::YankAll() noexcept
{
    auto end = m_pix_item->boundingRect().bottomRight();
    emit clipboardContentChanged(m_model->selectAllText(QPointF(0, 0), end));
}

// Execution while pressing the "delete" action
// (not to be confused with the designated delete key on the keyboard)
void
DocumentView::deleteKeyAction() noexcept
{
    if (!m_selected_annots.empty())
    {
        m_model->deleteAnnots(m_selected_annots);
        setDirty(true);
        renderPage(m_pageno, true);
    }
    m_annot_selection_present = false;
}

void
DocumentView::setDirty(bool state) noexcept
{
    if (m_dirty == state)
        return;

    m_dirty = state;

    QString title     = m_config.ui.window_title_format;
    QString panelName = m_filename;

    if (m_dirty)
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

    title = title.arg(m_basename);
    emit panelNameChanged(panelName);
    this->setWindowTitle(title);
}

void
DocumentView::TextSelectionMode() noexcept
{
    if (m_annot_selection_present)
        clearAnnotSelection();

    if (m_selection_present)
        ClearTextSelection();

    m_gview->setMode(GraphicsView::Mode::TextSelection);
    emit selectionModeChanged(GraphicsView::Mode::TextSelection);
}

void
DocumentView::fadeJumpMarker(JumpMarker *marker) noexcept
{
    // Stop any existing animation
    if (m_jump_marker_animation)
    {
        m_jump_marker_animation->stop();
        m_jump_marker_animation->deleteLater();
        m_jump_marker_animation = nullptr;
    }

    m_jump_marker_animation = new QPropertyAnimation(marker, "opacity");
    m_jump_marker_animation->setDuration(500); // 0.5 seconds fade
    m_jump_marker_animation->setStartValue(1.0);
    m_jump_marker_animation->setEndValue(0.0);
    m_jump_marker_animation->start(QAbstractAnimation::DeleteWhenStopped);

    connect(m_jump_marker_animation, &QPropertyAnimation::finished, [this, marker]()
    {
        marker->hide();
        marker->setOpacity(1.0);
        m_jump_marker_animation = nullptr;
    });
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
    if (m_hit_pixmap)
    {
        addAction("Open Image in External Viewer",
                  &DocumentView::OpenHitPixmapInExternalViewer);
        addAction("Save Image As...", &DocumentView::SaveImageAs);
        addAction("Copy Image", &DocumentView::CopyImageToClipboard);
        return;
    }

    switch (m_gview->mode())
    {
        case GraphicsView::Mode::TextSelection:
        {
            if (m_selection_present)
            {
                addAction("Copy Text", &DocumentView::YankSelection);
                addAction("Highlight Text",
                          &DocumentView::TextHighlightCurrentSelection);
            }
        }
        break;

        case GraphicsView::Mode::AnnotSelect:
        {
            if (!m_annot_selection_present)
                return;
            addAction("Delete Annotations", &DocumentView::deleteKeyAction);
            addAction("Change color", &DocumentView::annotChangeColor);
        }
        break;

        case GraphicsView::Mode::TextHighlight:
            addAction("Change color", &DocumentView::changeHighlighterColor);
            break;

        case GraphicsView::Mode::AnnotRect:
            addAction("Change color", &DocumentView::changeAnnotRectColor);
            break;

        default:
            break;
    }
}

// Change color for annots in the selection area
void
DocumentView::annotChangeColor() noexcept
{
    auto color = QColorDialog::getColor(
        m_model->highlightColor(), this, "Highlight Color",
        QColorDialog::ColorDialogOption::ShowAlphaChannel);
    if (color.isValid())
    {
        m_model->annotChangeColorForIndexes(m_selected_annots, color);
        renderPage(m_pageno, true);
    }
}

void
DocumentView::changeHighlighterColor() noexcept
{
    auto color = QColorDialog::getColor(
        m_config.ui.colors["highlight"], this, "Highlighter Color",
        QColorDialog::ColorDialogOption::ShowAlphaChannel);
    if (color.isValid())
    {
        m_config.ui.colors["highlight"] = color.name().toStdString().c_str();
        m_model->setHighlightColor(color);
        emit highlightColorChanged(color);
    }
}

void
DocumentView::changeAnnotRectColor() noexcept
{
    auto color = QColorDialog::getColor(
        m_model->highlightColor(), this, "Annot Rect Color",
        QColorDialog::ColorDialogOption::ShowAlphaChannel);
    if (color.isValid())
    {
        m_config.ui.colors["annot_rect"] = color.name().toStdString().c_str();
        m_model->setAnnotRectColor(color);
        emit highlightColorChanged(color);
    }
}

void
DocumentView::mouseWheelScrollRequested(int delta) noexcept
{
    if (!m_scroll_cooldown.isValid())
        m_scroll_cooldown.start();

    if (m_scroll_cooldown.elapsed() < kScrollCooldownMs)
        return;

    m_scroll_accumulator += delta;

    bool atTop    = m_vscrollbar->value() == m_vscrollbar->minimum();
    bool atBottom = m_vscrollbar->value() == m_vscrollbar->maximum();

    // Only accumulate if we're at the page edge
    if (atTop || atBottom)
    {
        m_scroll_accumulator += delta;

        if (m_scroll_accumulator >= kPageThreshold && atTop)
        {
            if (PrevPage())
            {
                m_scroll_cooldown.restart();
                m_scroll_accumulator = 0;

                QTimer::singleShot(0, this, [this]()
                { m_vscrollbar->setValue(m_vscrollbar->maximum()); });
            }
        }
        else if (m_scroll_accumulator <= -kPageThreshold && atBottom)
        {
            if (NextPage())
            {
                m_scroll_cooldown.restart();
                m_scroll_accumulator = 0;

                QTimer::singleShot(0, this, [this]()
                { m_vscrollbar->setValue(m_vscrollbar->minimum()); });
            }
        }
    }
    else
    {
        // Reset accumulator if user scrolls mid-page
        m_scroll_accumulator = 0;
    }
}

// Highlight text annotation for the current selection
void
DocumentView::TextHighlightCurrentSelection() noexcept
{
    if (!m_selection_present)
        return;

    m_model->annotHighlightSelection(m_gview->selectionStart(),
                                     m_gview->selectionEnd());
    m_selection_present = false;
    renderPage(m_pageno, true);
}

// Convenience function for setting the fit mode from outside this class
void
DocumentView::Fit(DocumentView::FitMode mode) noexcept
{
    switch (mode)
    {
        case FitMode::None:
            break;
        case FitMode::Width:
            FitWidth();
            break;
        case FitMode::Height:
            FitHeight();
            break;
        case FitMode::Window:
            FitWindow();
            break;
        default:
            break;
    }
}

void
DocumentView::clearKBHintsOverlay() noexcept
{
    for (auto &link : m_link_hints)
    {
        m_gscene->removeItem(link);
    }
    m_link_hints_present = false;
}

void
DocumentView::synctexJumpRequested(const QPointF &loc) noexcept
{
    if (m_synctex_scanner)
    {
        fz_point pt = mapToPdf(loc);
        if (synctex_edit_query(m_synctex_scanner, m_pageno + 1, pt.x, pt.y) > 0)
        {
            synctex_node_p node;
            while ((node = synctex_scanner_next_result(m_synctex_scanner)))
                synctexLocateInFile(synctex_node_get_name(node),
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

void
DocumentView::showJumpMarker(const QPointF &p) noexcept
{
    // Stop any pending fade timer
    if (m_jump_marker_timer)
    {
        m_jump_marker_timer->stop();
        m_jump_marker_timer->deleteLater();
        m_jump_marker_timer = nullptr;
    }

    // Stop any running animation and reset opacity
    if (m_jump_marker_animation)
    {
        m_jump_marker_animation->stop();
        m_jump_marker_animation->deleteLater();
        m_jump_marker_animation = nullptr;
    }

    m_jump_marker->setOpacity(1.0);
    m_jump_marker->setRect(QRectF(p.x(), p.y(), 10, 10));
    m_jump_marker->show();

    // Create a new timer for the fade delay
    m_jump_marker_timer = new QTimer(this);
    m_jump_marker_timer->setSingleShot(true);
    connect(m_jump_marker_timer, &QTimer::timeout, [this]() {
        fadeJumpMarker(m_jump_marker);
        m_jump_marker_timer = nullptr;
    });
    m_jump_marker_timer->start(1000);
}

void
DocumentView::showJumpMarker(const fz_point &p) noexcept
{
    showJumpMarker(QPointF(p.x, p.y));
}

void
DocumentView::nextFitMode() noexcept
{
    FitMode nextMode = static_cast<FitMode>((static_cast<int>(m_fit_mode) + 1)
                                            % static_cast<int>(FitMode::COUNT));
    m_fit_mode       = nextMode;
    switch (m_fit_mode)
    {
        case FitMode::Width:
            FitWidth();
            break;

        case FitMode::Height:
            FitHeight();
            break;

        case FitMode::Window:
            FitWindow();
            break;

        case FitMode::None:
            FitNone();
            break;

        default:
            break;
    }
    fitModeChanged(m_fit_mode);
    // Fit(static_cast<FitMode>(nextMode));
}

void
DocumentView::OpenHitPixmapInExternalViewer() noexcept
{
    if (!m_hit_pixmap)
        return;

    QTemporaryFile tempFile(QDir::tempPath() + "/XXXXXX.png");
    tempFile.setAutoRemove(false);

    if (tempFile.open())
    {
        m_model->SavePixmap(m_hit_pixmap, tempFile.fileName());
        tempFile.close();
        QDesktopServices::openUrl(QUrl::fromLocalFile(tempFile.fileName()));
    }
    else
    {
        QMessageBox::critical(this, "Error",
                              "Could not open in external viewer. [Could not "
                              "create temporary file.]");
    }
}

void
DocumentView::OpenImageInExternalViewer(const QImage &img) noexcept
{
    if (img.isNull())
        return;

    QTemporaryFile tempFile(QDir::tempPath() + "/XXXXXX.png");
    tempFile.setAutoRemove(false);

    if (tempFile.open())
    {
        img.save(tempFile.fileName(), "PNG");
        tempFile.close();
        QDesktopServices::openUrl(QUrl::fromLocalFile(tempFile.fileName()));
    }
    else
    {
        QMessageBox::critical(this, "Error",
                              "Could not open in external viewer. [Could not "
                              "create temporary file.]");
    }
}

void
DocumentView::SaveImageAs() noexcept
{
    if (!m_hit_pixmap)
        return;

    QFileDialog fd(this);
    const QString &fileName
        = fd.getSaveFileName(this, "Save Image", "",
                             "PNG Image (*.png), "
                             "JPEG Image (*.jpg *.jpeg), "
                             "BMP Image (*.bmp);; All Files (*)");
    m_model->SavePixmap(m_hit_pixmap, fileName);
}

void
DocumentView::CopyImageToClipboard() noexcept
{
    m_model->CopyPixmapToClipboard(m_hit_pixmap);
}

void
DocumentView::CopyTextFromRegion(const QRectF &area) noexcept
{
    QClipboard *clip = QApplication::clipboard();
    clip->setText(
        m_model->getSelectionText(area.topLeft(), area.bottomRight()));
}

void
DocumentView::CopyRegionAsImage(const QRectF &area) noexcept
{
    QImage img = m_pix_item->pixmap().copy(area.toRect()).toImage();
    if (!img.isNull())
    {
        QClipboard *clip = QApplication::clipboard();
        clip->setImage(img);
    }
}

void
DocumentView::SaveRegionAsImage(const QRectF &area) noexcept
{
    QImage img = m_pix_item->pixmap().copy(area.toRect()).toImage();
    if (img.isNull())
        return;
    QFileDialog fd(this);
    const QString &fileName
        = fd.getSaveFileName(this, "Save Image", "",
                             "PNG Image (*.png), "
                             "JPEG Image (*.jpg *.jpeg), "
                             "BMP Image (*.bmp);; All Files (*)");
    if (fileName.isEmpty())
        return;
    QString format;
    if (fileName.endsWith(".png", Qt::CaseInsensitive))
        format = "PNG";
    else if (fileName.endsWith(".jpg", Qt::CaseInsensitive)
             || fileName.endsWith(".jpeg", Qt::CaseInsensitive))
        format = "JPEG";
    else if (fileName.endsWith(".bmp", Qt::CaseInsensitive))
        format = "BMP";
    else
        format = "PNG"; // Default to PNG
    img.save(fileName, format.toStdString().c_str());
}

void
DocumentView::OpenRegionInExternalViewer(const QRectF &area) noexcept
{
    QImage img = m_pix_item->pixmap().copy(area.toRect()).toImage();
    OpenImageInExternalViewer(img);
    qDebug() << "Opened region in external viewer";
}
