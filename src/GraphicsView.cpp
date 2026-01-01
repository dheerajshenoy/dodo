#include "GraphicsView.hpp"

#include <QGraphicsProxyWidget>
#include <QGuiApplication>
#include <QLineF>
#include <QMenu>
#include <qgraphicsview.h>

GraphicsView::GraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    setMouseTracking(true);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setAcceptDrops(false);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);
    setOptimizationFlag(QGraphicsView::DontSavePainterState);
}

void
GraphicsView::setMode(Mode mode) noexcept
{
    m_selecting = false;

    switch (m_mode)
    {
        case Mode::RegionSelection:
        case Mode::AnnotRect:
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
    const QPointF scenePos = mapToScene(event->pos());

    // SyncTeX priority
#ifdef HAS_SYNCTEX
    if (m_mode == Mode::TextSelection && event->button() == Qt::LeftButton
        && (event->modifiers() & Qt::ShiftModifier))
    {
        emit synctexJumpRequested(scenePos);
        m_ignore_next_release = true;
        return;
    }
#endif

    // Right click passthrough (for image context menu)
    // if (event->button() == Qt::RightButton)
    // {
    //     emit rightClickRequested(scenePos);
    //     QGraphicsView::mousePressEvent(event);
    //     return;
    // }

    // Multi-click tracking
    if (event->button() == Qt::LeftButton)
    {
        bool isMultiClick = m_clickTimer.isValid()
                            && m_clickTimer.elapsed() < MULTI_CLICK_INTERVAL
                            && QLineF(event->pos(), m_lastClickPos).length()
                                   < CLICK_DISTANCE_THRESHOLD;

        m_clickCount = isMultiClick ? m_clickCount + 1 : 1;
        if (m_clickCount > 4)
            m_clickCount = 1;

        m_lastClickPos = event->pos();
        m_clickTimer.restart();

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

            case 1:
            default:
                emit textSelectionDeletionRequested();
                break;
        }
    }

    // Mode-specific handling
    switch (m_mode)
    {
        case Mode::RegionSelection:
        case Mode::AnnotRect:
        case Mode::AnnotSelect:
            m_start     = event->pos();
            m_rect      = QRect();
            m_dragging  = false;
            m_selecting = true;

            if (!m_rubberBand)
                m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);

            m_rubberBand->setGeometry(QRect(m_start, QSize()));
            m_rubberBand->show();
            break;

        case Mode::TextSelection:
        case Mode::TextHighlight:
            if (event->button() == Qt::LeftButton)
            {
                QGuiApplication::setOverrideCursor(Qt::IBeamCursor);
                m_mousePressPos   = scenePos;
                m_selection_start = scenePos;
                m_selecting       = true;
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
    const QPointF scenePos = mapToScene(event->pos());

    switch (m_mode)
    {
        case Mode::TextSelection:
        case Mode::TextHighlight:
            if (m_selecting)
            {
                if (m_mode == Mode::TextSelection)
                    emit textSelectionRequested(m_selection_start, scenePos);
                else
                    emit textHighlightRequested(m_selection_start, scenePos);
            }
            break;

        case Mode::AnnotSelect:
        case Mode::RegionSelection:
        case Mode::AnnotRect:
            if (event->buttons() & Qt::LeftButton && m_selecting)
            {
                if (!m_dragging
                    && (event->pos() - m_start).manhattanLength()
                           > m_drag_threshold)
                    m_dragging = true;

                if (m_dragging && m_rubberBand)
                {
                    m_rect = QRect(m_start, event->pos()).normalized();
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
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    const QPointF scenePos = mapToScene(event->pos());
    const int dist
        = (event->pos() - m_mousePressPos.toPoint()).manhattanLength();
    const bool isDrag = dist > m_drag_threshold;

    switch (m_mode)
    {
        case Mode::TextSelection:
            if (m_selection_start == scenePos && !isDrag)
            {
                // No selection, just a click
                emit textSelectionDeletionRequested();
            }
            else if (event->button() == Qt::LeftButton && isDrag)
                emit textSelectionRequested(m_selection_start, scenePos);
            break;

        case Mode::TextHighlight:
            if (isDrag)
                emit textHighlightRequested(m_selection_start, scenePos);
            // emit textSelectionDeletionRequested(); Should we clear selection
            // here?
            break;

        case Mode::RegionSelection:
        case Mode::AnnotRect:
        case Mode::AnnotSelect:
        {
            if (m_rubberBand)
                m_rubberBand->hide();

            const QRectF sceneRect = mapToScene(m_rect).boundingRect();
            if (!sceneRect.isEmpty())
            {
                if (m_mode == Mode::RegionSelection)
                    emit regionSelectRequested(sceneRect);
                else if (m_mode == Mode::AnnotRect)
                    emit highlightDrawn(sceneRect);
                else if (m_mode == Mode::AnnotSelect)
                {
                    if (!m_dragging)
                        emit annotSelectRequested(scenePos);
                    else
                        emit annotSelectRequested(sceneRect);
                }
            }
        }
        break;

        default:
            break;
    }

    m_selecting = false;
    m_dragging  = false;
    QGuiApplication::restoreOverrideCursor();

    QGraphicsView::mouseReleaseEvent(event);
}

void
GraphicsView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() == Qt::ControlModifier)
    {
        if (event->angleDelta().y() > 0)
            emit zoomInRequested();
        else
            emit zoomOutRequested();
        return;
    }

    if (m_page_nav_with_mouse)
    {
        int delta = !event->pixelDelta().isNull() ? event->pixelDelta().y()
                                                  : event->angleDelta().y();
        emit scrollRequested(delta);
    }

    QGraphicsView::wheelEvent(event);
}

void
GraphicsView::contextMenuEvent(QContextMenuEvent *event)
{
    emit contextMenuRequested(event->pos());
    event->accept();
}
