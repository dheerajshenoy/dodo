#include "GraphicsView.hpp"

#include <QGraphicsProxyWidget>
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
            if (m_rubberBand)
            {
                m_rubberBand->hide();
                delete m_rubberBand;
                m_rubberBand = nullptr;
            }
            break;

        case Mode::TextSelection:
        case Mode::TextHighlight:
            emit textSelectionDeletionRequested();
            break;

        case Mode::AnnotRect:
            break;

        case Mode::AnnotSelect:
        {
            emit annotSelectClearRequested();
        }
        break;

        default:
            break;
    }

    m_mode = mode;
}

void
GraphicsView::mousePressEvent(QMouseEvent *event)
{
    // Right click on image to open context menu
    if (event->button() == Qt::RightButton)
    {
        QPointF scenePos = mapToScene(event->pos());
        emit rightClickRequested(scenePos);
        event->accept();
        return;
    }

    QGraphicsItem *item = itemAt(event->pos());
    if (item && item->type() == QGraphicsProxyWidget::Type)
    {
        // Let the proxy widget handle the event
        QGraphicsView::mousePressEvent(event);
        return;
    }

    switch (m_mode)
    {
        case Mode::RegionSelection:
        {
            if (event->button() == Qt::LeftButton)
            {
                m_start = event->pos();
                m_rect  = QRect();

                if (!m_rubberBand)
                    m_rubberBand
                        = new QRubberBand(QRubberBand::Rectangle, this);

                m_rubberBand->setGeometry(QRect(m_start, QSize()));
                m_rubberBand->show();
                m_selecting = true;
                event->accept();
                return;
            }
        }
        break;

        case Mode::AnnotSelect:
        {
            if (event->button() == Qt::LeftButton)
            {
                // emit annotSelectClearRequested();
                // Select annotation under cursor if any
                QPointF scenePos = mapToScene(event->pos());
                emit annotSelectClearRequested();
                emit annotSelectRequested(scenePos);
                event->accept();
            }
        }
        case Mode::AnnotRect:
        {
            if (event->button() == Qt::LeftButton)
            {
                m_start = event->pos();
                m_rect  = QRect();

                if (!m_rubberBand)
                    m_rubberBand
                        = new QRubberBand(QRubberBand::Rectangle, this);

                m_rubberBand->setGeometry(QRect(m_start, QSize()));
                m_rubberBand->show();
                event->accept();
                return;
            }
        }
        break;

        case Mode::TextSelection:
        {
            if (event->button() == Qt::LeftButton)
            {
                // Synctex stuff
                if (event->modifiers() & Qt::ShiftModifier)
                {
                    emit synctexJumpRequested(mapToScene(event->pos()));
                    return;
                }

                m_mousePressPos = mapToScene(event->pos());
                QGuiApplication::setOverrideCursor(
                    Qt::CursorShape::IBeamCursor);
                m_selecting = true;
                if (m_pixmapItem
                    && m_pixmapItem->sceneBoundingRect().contains(
                        m_mousePressPos))
                {
                    m_selection_start = m_mousePressPos;
                    emit textSelectionDeletionRequested();
                }
                event->accept();
            }
        }
        break;

        case Mode::TextHighlight:
        {
            if (event->button() == Qt::LeftButton)
            {
                QGuiApplication::setOverrideCursor(
                    Qt::CursorShape::IBeamCursor);
                m_mousePressPos = event->pos();
                m_selecting     = true;
                auto pos        = mapToScene(event->pos());
                if (m_pixmapItem
                    && m_pixmapItem->sceneBoundingRect().contains(pos))
                {
                    m_selection_start = pos;
                    emit textSelectionDeletionRequested();
                }
                event->accept();
                return;
            }
        }
        break;

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

        case Mode::RegionSelection:
        case Mode::AnnotSelect:
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
    // Check if we're releasing over a proxy widget and we didn't start a
    // selection
    QGraphicsItem *item = itemAt(event->pos());
    if (item && item->type() == QGraphicsProxyWidget::Type && !m_selecting)
    {
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    // Check if it was a drag or a click
    int dist = (event->pos() - m_mousePressPos).manhattanLength();

    switch (m_mode)
    {
        case Mode::RegionSelection:
        {
            if (event->button() == Qt::LeftButton && dist > m_drag_threshold)
            {
                QRectF sceneRect = mapToScene(m_rect).boundingRect();

                if (!m_pixmapItem)
                    return;

                QRectF clippedRect
                    = sceneRect.intersected(m_pixmapItem->boundingRect());
                if (!clippedRect.isEmpty())
                    emit regionSelectRequested(clippedRect);
                m_selecting = false;
            }
        }
        break;

        case Mode::TextSelection:
        {
            if (event->button() == Qt::LeftButton && dist > m_drag_threshold)
            {
                m_selection_end = mapToScene(event->pos());
                emit textSelectionRequested(m_selection_start, m_selection_end);
            }
            QGuiApplication::restoreOverrideCursor();
            m_selecting = false;
        }
        break;

        case Mode::TextHighlight:
        {
            if (dist > m_drag_threshold)
            {
                m_selection_end = mapToScene(event->pos());
                emit textHighlightRequested(m_selection_start, m_selection_end);
            }
            QGuiApplication::restoreOverrideCursor();
            m_selecting = false;
            emit textSelectionDeletionRequested();
        }
        break;

        case Mode::AnnotSelect:
        {
            if (event->button() == Qt::LeftButton)
            {
                if (m_rubberBand)
                    m_rubberBand->hide();

                QRectF sceneRect = mapToScene(m_rect).boundingRect();

                if (!m_pixmapItem)
                    return;

                QRectF clippedRect
                    = sceneRect.intersected(m_pixmapItem->boundingRect());
                if (!clippedRect.isEmpty())
                    emit annotSelectRequested(clippedRect);
            }
        }
        break;

        case Mode::AnnotRect:
        {
            if (event->button() == Qt::LeftButton)
            {
                if (m_rubberBand)
                    m_rubberBand->hide();

                QRectF sceneRect = mapToScene(m_rect).boundingRect();
                if (!m_pixmapItem)
                    return;

                QRectF clippedRect
                    = sceneRect.intersected(m_pixmapItem->boundingRect());
                if (!clippedRect.isEmpty())
                    emit highlightDrawn(clippedRect);
            }
        }
        break;

        default:
            break;
    }

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

    if (m_mode == Mode::AnnotSelect)
    {
        QGraphicsItem *item
            = scene()->itemAt(mapToScene(e->pos()), transform());

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
