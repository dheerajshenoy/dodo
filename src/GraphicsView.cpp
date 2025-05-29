#include "GraphicsView.hpp"

#include "BrowseLinkItem.hpp"
#include "HighlightItem.hpp"

#include <qgraphicssceneevent.h>
#include <qgraphicsview.h>
#include <qnamespace.h>

GraphicsView::GraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    setMouseTracking(true);
    setResizeAnchor(AnchorViewCenter);
    setTransformationAnchor(AnchorUnderMouse);
}

void
GraphicsView::setMode(Mode mode) noexcept
{
    switch (m_mode)
    {
        case Mode::TextSelection:
        case Mode::TextHighlight:
            emit textSelectionDeletionRequested();
            m_has_text_selection = false;
            break;

        case Mode::AnnotRect:
            break;

        case Mode::AnnotSelect:
            emit annotSelectClearRequested();
            break;
    }

    m_mode = mode;
}

void
GraphicsView::mousePressEvent(QMouseEvent *event)
{
    switch (m_mode)
    {
        case Mode::AnnotSelect:
        {
            if (event->button() == Qt::LeftButton)
                emit annotSelectClearRequested();
        }
        case Mode::AnnotRect:
        {
            if (event->button() == Qt::LeftButton)
            {
                m_start = event->pos();
                m_rect  = QRect();

                if (!m_rubberBand)
                    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);

                m_rubberBand->setGeometry(QRect(m_start, QSize()));
                m_rubberBand->show();
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

                QGuiApplication::setOverrideCursor(Qt::CursorShape::IBeamCursor);
                m_mousePressPos = event->pos();
                m_selecting     = true;
                auto pos        = mapToScene(event->pos());
                if (m_pixmapItem && m_pixmapItem->sceneBoundingRect().contains(pos))
                {
                    m_selection_start = pos;
                    emit textSelectionDeletionRequested();
                    m_has_text_selection = false;
                }
            }
        }
        break;

        case Mode::TextHighlight:
        {
            if (event->button() == Qt::LeftButton)
            {
                QGuiApplication::setOverrideCursor(Qt::CursorShape::IBeamCursor);
                m_mousePressPos = event->pos();
                m_selecting     = true;
                auto pos        = mapToScene(event->pos());
                if (m_pixmapItem && m_pixmapItem->sceneBoundingRect().contains(pos))
                {
                    m_selection_start = pos;
                    emit textSelectionDeletionRequested();
                }
                return;
            }
        }
        break;
    }
    QGraphicsView::mousePressEvent(event); // Let QGraphicsItems (like links) respond
}

void
GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    switch (m_mode)
    {
        case Mode::TextSelection:
        case Mode::TextHighlight:
        {
            if (m_selecting)
            {
                auto pos = mapToScene(event->pos());
                if (m_pixmapItem && m_pixmapItem->sceneBoundingRect().contains(pos))
                {
                    m_selection_end = pos;
                    emit textSelectionRequested(m_selection_start, m_selection_end);
                }
            }
        }
        break;

        case Mode::AnnotSelect:
        case Mode::AnnotRect:
        {
            m_rect = QRect(m_start, event->pos()).normalized();
            if (m_rubberBand)
                m_rubberBand->setGeometry(m_rect);
        }
        break;
    }

    QGraphicsView::mouseMoveEvent(event); // Allow hover/cursor events
}

void
GraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    int dist = (event->pos() - m_mousePressPos).manhattanLength();

    switch (m_mode)
    {
        case Mode::TextSelection:
        {
            if (event->button() == Qt::LeftButton && dist > CLICK_THRESHOLD)
            {
                m_selection_end = mapToScene(event->pos());
                emit textSelectionRequested(m_selection_start, m_selection_end);
                m_has_text_selection = true;
            }
            QGuiApplication::restoreOverrideCursor();
            m_selecting = false;
        }
        break;

        case Mode::TextHighlight:
        {
            if (dist > CLICK_THRESHOLD)
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

                QRectF clippedRect = sceneRect.intersected(m_pixmapItem->boundingRect());
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

                QRectF clippedRect = sceneRect.intersected(m_pixmapItem->boundingRect());
                if (!clippedRect.isEmpty())
                    emit highlightDrawn(clippedRect);
            }
        }
        break;
    }

    QGuiApplication::restoreOverrideCursor();
    QGraphicsView::mouseReleaseEvent(event); // Let items handle clicks
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
    QGraphicsView::wheelEvent(e);
}

void
GraphicsView::contextMenuEvent(QContextMenuEvent *e)
{
    if (m_has_text_selection)
    {
        QMenu menu(this);
        emit populateContextMenuRequested(&menu);
        if (!menu.isEmpty())
            menu.exec(e->globalPos());
        e->accept();
        return;
    }
    else
    {
        // Check if item under cursor handled the event
        QGraphicsItem *item = scene()->itemAt(mapToScene(e->pos()), transform());

        // Optional: Skip if it's a link item
        if (item)
        {
            e->ignore();
            QGraphicsView::contextMenuEvent(e);
        }
        else
        {
        }
    }
}
