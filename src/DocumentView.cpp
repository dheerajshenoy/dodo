#include "DocumentView.hpp"

#include <QVBoxLayout>
#include <algorithm>
#include <qnamespace.h>

DocumentView::DocumentView(const QString &filepath, const Config &config,
                           QWidget *parent) noexcept
    : QWidget(parent), m_config(config)
{
    m_model = new Model(filepath);
    m_model->open();

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
            [this](int) { renderVisiblePages(); });
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
    m_target_zoom  = factor;
    m_current_zoom = factor;
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

void
DocumentView::ZoomIn() noexcept
{
    m_current_zoom *= 1.2;
    m_model->setZoom(m_current_zoom);
    cachePageStride();
    renderVisiblePages();
}

void
DocumentView::ZoomOut() noexcept
{
    m_current_zoom /= 1.2;
    m_model->setZoom(m_current_zoom);
    cachePageStride();
    renderVisiblePages();
}

void
DocumentView::ZoomReset() noexcept
{
    m_current_zoom = 1.0;
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
DocumentView::renderPage(int pageno) noexcept
{
    removePageItem(pageno);
    const QPixmap pix = m_model->renderPageToPixmap(pageno);
    auto *item        = new GraphicsPixmapItem();
    item->setPixmap(pix);
    double pageWidthLogical = pix.width() / pix.devicePixelRatio();
    double xOffset = (m_gview->sceneRect().width() - pageWidthLogical) / 2.0;
    item->setPos(xOffset, pageno * m_page_stride);
    m_gscene->addItem(item);
    m_page_items_hash[pageno] = item;
}

void
DocumentView::renderVisiblePages() noexcept
{
    std::vector<int> visiblePages = getVisiblePages();
    qDebug() << "Visible pages:" << visiblePages;
    QSet<int> visibleSet;
    for (int pageno : visiblePages)
    {
        visibleSet.insert(pageno);
        if (!m_page_items_hash.contains(pageno))
            renderPage(pageno);
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
