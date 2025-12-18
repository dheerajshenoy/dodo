#include "GraphicsView.hpp"

#include <QGraphicsProxyWidget>
#include <QLineF>
#include <numbers>
#include <qgraphicssceneevent.h>
#include <qgraphicsview.h>
#include <qmenu.h>
#include <qnamespace.h>

GraphicsView::GraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    setMouseTracking(true);
    setResizeAnchor(AnchorViewCenter);
    setTransformationAnchor(AnchorUnderMouse);
    setAcceptDrops(false);
}

void
GraphicsView::setMode(Mode mode) noexcept
{
    m_selecting = false;
    switch (m_mode)
    {
        case Mode::RegionSelection:
        {
            if (m_rubberBand)
            {
                m_rubberBand->hide();
                delete m_rubberBand;
                m_rubberBand = nullptr;
            }
        }
        break;

        case Mode::TextSelection:
        case Mode::TextHighlight:
            emit textSelectionDeletionRequested();
            break;

        case Mode::AnnotRect:
            emit annotSelectClearRequested();
            break;

        case Mode::AnnotSelect:
            emit annotSelectClearRequested();
            break;

        default:
            break;
    }

    m_mode = mode;
}

void
GraphicsView::mousePressEvent(QMouseEvent *event)
{
    const QPoint viewPos   = event->pos();
    const QPointF scenePos = mapToScene(viewPos);

    /* --------------------------------------------------
     * SyncTeX (ABSOLUTE PRIORITY)
     * -------------------------------------------------- */
    if (m_mode == Mode::TextSelection && event->button() == Qt::LeftButton
        && (event->modifiers() & Qt::ShiftModifier))
    {
        emit synctexJumpRequested(scenePos);
        m_ignore_next_release = true;
        return; // 🔴 do NOT fall through
    }

    /* --------------------------------------------------
     * Right click handling
     * -------------------------------------------------- */
    if (event->button() == Qt::RightButton)
    {
        emit rightClickRequested(scenePos);
        QGraphicsView::mousePressEvent(event);
        return;
    }

    /* --------------------------------------------------
     * Proxy widget handling
     * -------------------------------------------------- */
    if (QGraphicsItem *item = itemAt(viewPos))
    {
        if (item->type() == QGraphicsProxyWidget::Type)
        {
            emit rightClickRequested(scenePos);
            QGraphicsView::mousePressEvent(event);
            return;
        }
    }

    if (event->button() == Qt::LeftButton)
    {
        const bool isMultiClick
            = m_clickTimer.isValid()
              && m_clickTimer.elapsed() < MULTI_CLICK_INTERVAL
              && QLineF(viewPos, m_lastClickPos).length()
                     < CLICK_DISTANCE_THRESHOLD;

        m_clickCount = isMultiClick ? m_clickCount + 1 : 1;
        if (m_clickCount > 4)
            m_clickCount = 1;

        m_lastClickPos = viewPos;
        m_clickTimer.restart();

        emit textSelectionDeletionRequested();

        switch (m_clickCount)
        {
            case 2:
                emit doubleClickRequested(scenePos);
                return;
            case 3:
                emit tripleClickRequested(scenePos);
                return;
            case 4:
                emit quadrupleClickRequested(scenePos);
                return;
            default:
                break;
        }
    }

    /* --------------------------------------------------
     * Mode-specific handling
     * -------------------------------------------------- */
    switch (m_mode)
    {
        case Mode::RegionSelection:
        case Mode::AnnotRect:
        {
            if (event->button() == Qt::LeftButton)
            {
                m_start = viewPos;
                m_rect  = QRect();

                if (!m_rubberBand)
                    m_rubberBand
                        = new QRubberBand(QRubberBand::Rectangle, this);

                m_rubberBand->setGeometry(QRect(m_start, QSize()));
                m_rubberBand->show();
                m_selecting = true;
            }
            break;
        }

        case Mode::AnnotSelect:
        {
            if (event->button() == Qt::LeftButton)
            {
                m_start     = viewPos;
                m_dragging  = false;
                m_selecting = true;
            }
            break;
        }

        case Mode::TextSelection:
        {
            if (event->button() == Qt::LeftButton)
            {
                QGuiApplication::setOverrideCursor(Qt::IBeamCursor);

                if (m_pixmapItem
                    && m_pixmapItem->sceneBoundingRect().contains(scenePos))
                {
                    m_mousePressPos   = scenePos;
                    m_selection_start = scenePos;
                    m_selecting       = true;
                }
            }
            break;
        }

        case Mode::TextHighlight:
        {
            if (event->button() == Qt::LeftButton)
            {
                QGuiApplication::setOverrideCursor(Qt::IBeamCursor);

                if (m_pixmapItem
                    && m_pixmapItem->sceneBoundingRect().contains(scenePos))
                {
                    m_mousePressPos   = scenePos;
                    m_selection_start = scenePos;
                    m_selecting       = true;
                }
            }
            break;
        }

        default:
            break;
    }

    QGraphicsView::mousePressEvent(event);
}

void
GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    // Check if we're dragging over a proxy widget
    QGraphicsItem *item = itemAt(event->pos());
    if (item && item->type() == QGraphicsProxyWidget::Type && !m_selecting)
    {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    switch (m_mode)
    {
        case Mode::TextSelection:
        case Mode::TextHighlight:
        {
            if (m_selecting)
            {
                auto pos = mapToScene(event->pos());
                if (m_pixmapItem
                    && m_pixmapItem->sceneBoundingRect().contains(pos))
                {
                    m_selection_end = pos;
                    emit textSelectionRequested(m_selection_start,
                                                m_selection_end);
                }
            }
        }
        break;

        case Mode::AnnotSelect:
        {
            if (event->buttons() & Qt::LeftButton)
            {
                if (!m_dragging)
                {
                    // Check if drag should start
                    if ((event->pos() - m_selection_start).manhattanLength()
                        > m_drag_threshold)
                    {
                        m_dragging  = true;
                        m_selecting = true;

                        // Initialize rubberband *now*, not earlier
                        if (!m_rubberBand)
                            m_rubberBand
                                = new QRubberBand(QRubberBand::Rectangle, this);

                        m_rubberBand->setGeometry(QRect(m_start, QSize()));
                        m_rubberBand->show();
                    }
                }

                if (m_dragging)
                {
                    m_rect = QRect(m_start, event->pos()).normalized();
                    if (m_rubberBand)
                        m_rubberBand->setGeometry(m_rect);
                }
            }
        }

        break;

        case Mode::RegionSelection:
        case Mode::AnnotRect:
        {
            if (m_selecting)
            {
                m_rect = QRect(m_start, event->pos()).normalized();
                if (m_rubberBand)
                    m_rubberBand->setGeometry(m_rect);
            }
        }
        break;

        default:
            break;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void
GraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_ignore_next_release)
    {
        m_ignore_next_release = false;
        return;
    }

    const QPoint viewPos   = event->pos();
    const QPointF scenePos = mapToScene(viewPos);

    /* --------------------------------------------------
     * Proxy widget passthrough
     * -------------------------------------------------- */
    if (!m_selecting)
    {
        if (QGraphicsItem *item = itemAt(viewPos))
        {
            if (item->type() == QGraphicsProxyWidget::Type)
            {
                QGraphicsView::mouseReleaseEvent(event);
                return;
            }
        }
    }

    /* --------------------------------------------------
     * Drag distance
     * -------------------------------------------------- */
    const int dist    = (viewPos - m_mousePressPos.toPoint()).manhattanLength();
    const bool isDrag = dist > m_drag_threshold;

    /* --------------------------------------------------
     * Mode-specific handling
     * -------------------------------------------------- */
    switch (m_mode)
    {
        case Mode::RegionSelection:
        {
            if (event->button() == Qt::LeftButton && isDrag)
            {
                if (!m_pixmapItem)
                    break;

                QRectF sceneRect = mapToScene(m_rect).boundingRect();
                QRectF clipped
                    = sceneRect.intersected(m_pixmapItem->boundingRect());

                if (!clipped.isEmpty())
                    emit regionSelectRequested(clipped);
            }
            break;
        }

        case Mode::TextSelection:
        {
            if (event->button() == Qt::LeftButton && isDrag)
            {
                m_selection_end = scenePos;
                emit textSelectionRequested(m_selection_start, m_selection_end);
            }
            break;
        }

        case Mode::TextHighlight:
        {
            if (isDrag)
            {
                m_selection_end = scenePos;
                emit textHighlightRequested(m_selection_start, m_selection_end);
            }

            emit textSelectionDeletionRequested();
            break;
        }

        case Mode::AnnotSelect:
        {
            if (event->button() != Qt::LeftButton)
                break;

            if (!m_dragging)
            {
                // Single click
                emit annotSelectClearRequested();
                emit annotSelectRequested(scenePos);
            }
            else
            {
                // Drag select
                if (m_rubberBand)
                    m_rubberBand->hide();

                if (!m_pixmapItem)
                    break;

                QRectF sceneRect = mapToScene(m_rect).boundingRect();
                QRectF clipped
                    = sceneRect.intersected(m_pixmapItem->boundingRect());

                if (!clipped.isEmpty())
                    emit annotSelectRequested(clipped);
            }
            break;
        }

        case Mode::AnnotRect:
        {
            if (event->button() == Qt::LeftButton)
            {
                if (m_rubberBand)
                    m_rubberBand->hide();

                if (!m_pixmapItem)
                    break;

                QRectF sceneRect = mapToScene(m_rect).boundingRect();
                QRectF clipped
                    = sceneRect.intersected(m_pixmapItem->boundingRect());

                if (!clipped.isEmpty())
                    emit highlightDrawn(clipped);
            }
            break;
        }

        default:
            break;
    }

    /* --------------------------------------------------
     * Cleanup (ONCE)
     * -------------------------------------------------- */
    m_selecting = false;
    m_dragging  = false;
    QGuiApplication::restoreOverrideCursor();

    QGraphicsView::mouseReleaseEvent(event);
}

void
GraphicsView::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() == Qt::ControlModifier)
    {
        if (e->angleDelta().y() > 0)
            emit zoomInRequested();
        else
            emit zoomOutRequested();
        return;
    }
    else
    {
        if (m_page_nav_with_mouse)
        {
            int delta = !e->pixelDelta().isNull() ? e->pixelDelta().y()
                                                  : e->angleDelta().y();
            emit scrollRequested(delta);
        }
    }
    QGraphicsView::wheelEvent(e);
}

void
GraphicsView::contextMenuEvent(QContextMenuEvent *e)
{
    QGraphicsItem *item = scene()->itemAt(mapToScene(e->pos()), transform());

    // Check if item is a selection highlight (should not handle context
    // menu)
    bool isSelectionItem = item && item->data(0).toString() == "selection";

    // Let BrowseLinkItem (and other interactive items) handle their own
    // context menu Skip selection items, pixmap, and proxy widgets -
    // they don't have context menus
    if (item && item != m_pixmapItem && !isSelectionItem
        && item->type() != QGraphicsProxyWidget::Type)
    {
        QGraphicsView::contextMenuEvent(e);
        return;
    }

    if (m_mode == Mode::AnnotSelect)
    {
        if (!item || item == m_pixmapItem)
        {
            QMenu menu(this);
            emit populateContextMenuRequested(&menu);
            if (!menu.isEmpty())
                menu.exec(e->globalPos());
            e->accept();
            return;
        }
        QGraphicsView::contextMenuEvent(e);
    }
    else
    {
        QMenu menu(this);
        emit populateContextMenuRequested(&menu);
        if (!menu.isEmpty())
            menu.exec(e->globalPos());
        e->accept();
    }
}

QPointF
GraphicsView::getCursorPos() noexcept
{
    return mapToScene(mapFromGlobal(QCursor::pos()));
}
