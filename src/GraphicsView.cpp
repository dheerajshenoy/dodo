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

    if (!m_rubberBand)
        m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
    m_rubberBand->hide();
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
#ifdef HAS_SYNCTEX
    if (m_mode == Mode::TextSelection && event->button() == Qt::LeftButton
        && (event->modifiers() & Qt::ShiftModifier))
    {
        emit synctexJumpRequested(mapToScene(event->pos()));
        m_ignore_next_release = true;
        event->accept();
        return; // don't forward to QGraphicsView
    }
#endif

    // Multi-click tracking (avoid QLineF sqrt)
    if (event->button() == Qt::LeftButton)
    {
        const bool timerOk = m_clickTimer.isValid()
                             && m_clickTimer.elapsed() < MULTI_CLICK_INTERVAL;

        const QPointF d    = event->pos() - m_lastClickPos;
        const double dist2 = d.x() * d.x() + d.y() * d.y();
        const double thresh2
            = CLICK_DISTANCE_THRESHOLD * CLICK_DISTANCE_THRESHOLD;

        const bool isMultiClick = timerOk && (dist2 < thresh2);

        m_clickCount = isMultiClick ? (m_clickCount + 1) : 1;
        if (m_clickCount > 4)
            m_clickCount = 1;

        m_lastClickPos = event->pos();
        m_clickTimer.restart();

        const QPointF scenePos = mapToScene(event->pos());

        if (m_clickCount == 2)
        {
            emit doubleClickRequested(scenePos);
            event->accept();
            return;
        }
        if (m_clickCount == 3)
        {
            emit tripleClickRequested(scenePos);
            event->accept();
            return;
        }
        if (m_clickCount == 4)
        {
            emit quadrupleClickRequested(scenePos);
            event->accept();
            return;
        }

        // single click
        emit textSelectionDeletionRequested();
    }

    switch (m_mode)
    {
        case Mode::RegionSelection:
        case Mode::AnnotRect:
        case Mode::AnnotSelect:
        {
            m_start     = event->pos();
            m_rect      = QRect();
            m_dragging  = false;
            m_selecting = true;

            m_rubberBand->setGeometry(QRect(m_start, QSize()));
            m_rubberBand->show();

            event->accept();
            return; // handled
        }

        case Mode::TextSelection:
        case Mode::TextHighlight:
        {
            if (event->button() == Qt::LeftButton)
            {
                setCursor(
                    Qt::IBeamCursor); // cheaper than global override cursor
                const QPointF scenePos = mapToScene(event->pos());
                m_mousePressPos        = scenePos;
                m_selection_start      = scenePos;
                m_selecting            = true;

                event->accept();
                return; // handled
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
    // If we are selecting text/highlight, throttle signals
    if ((m_mode == Mode::TextSelection || m_mode == Mode::TextHighlight)
        && m_selecting)
    {
        if ((event->pos() - m_lastMovePos).manhattanLength()
            < MOVE_EMIT_THRESHOLD_PX)
        {
            event->accept();
            return;
        }
        m_lastMovePos = event->pos();

        const QPointF scenePos = mapToScene(event->pos());
        if (m_mode == Mode::TextSelection)
            emit textSelectionRequested(m_selection_start, scenePos);
        else
            emit textHighlightRequested(m_selection_start, scenePos);

        event->accept();
        return; // handled
    }

    // Rubber band modes: no mapToScene needed during drag
    if ((m_mode == Mode::AnnotSelect || m_mode == Mode::RegionSelection
         || m_mode == Mode::AnnotRect)
        && (event->buttons() & Qt::LeftButton) && m_selecting)
    {
        if (!m_dragging
            && (event->pos() - m_start).manhattanLength() > m_drag_threshold)
        {
            m_dragging = true;
        }

        if (m_dragging)
        {
            m_rect = QRect(m_start, event->pos()).normalized();
            m_rubberBand->setGeometry(m_rect);
        }

        event->accept();
        return; // handled
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

    const bool wasSelecting = m_selecting;
    m_selecting             = false;

    // If we weren't doing any interaction, let base handle it
    if (!wasSelecting)
    {
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    // Text modes
    if (m_mode == Mode::TextSelection || m_mode == Mode::TextHighlight)
    {
        unsetCursor();

        const QPointF scenePos = mapToScene(event->pos());
        const int dist = (scenePos.toPoint() - m_mousePressPos.toPoint())
                             .manhattanLength();
        const bool isDrag = dist > m_drag_threshold;

        if (m_mode == Mode::TextSelection)
        {
            if (!isDrag || m_selection_start == scenePos)
                emit textSelectionDeletionRequested();
            else if (event->button() == Qt::LeftButton)
                emit textSelectionRequested(m_selection_start, scenePos);
        }
        else
        {
            if (isDrag)
                emit textHighlightRequested(m_selection_start, scenePos);
        }

        m_dragging = false;
        event->accept();
        return; // handled
    }

    // Rubber band modes
    if (m_mode == Mode::RegionSelection || m_mode == Mode::AnnotRect
        || m_mode == Mode::AnnotSelect)
    {
        m_rubberBand->hide();

        if (!m_dragging && m_mode == Mode::AnnotSelect)
        {
            emit annotSelectRequested(mapToScene(event->pos()));
        }
        else
        {
            const QRectF sceneRect = mapToScene(m_rect).boundingRect();
            if (!sceneRect.isEmpty())
            {
                if (m_mode == Mode::RegionSelection)
                    emit regionSelectRequested(sceneRect);
                else if (m_mode == Mode::AnnotRect)
                    emit highlightDrawn(sceneRect);
                else
                    emit annotSelectRequested(sceneRect);
            }
        }

        m_dragging = false;
        event->accept();
        return; // handled
    }

    m_dragging = false;
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
        event->accept();
        return; // do NOT call base
    }

    if (m_page_nav_with_mouse)
    {
        const int delta = !event->pixelDelta().isNull()
                              ? event->pixelDelta().y()
                              : event->angleDelta().y();
        emit scrollRequested(delta);
        event->accept();
        return; // do NOT call base (prevents double scroll + extra work)
    }

    QGraphicsView::wheelEvent(event);
}

void
GraphicsView::contextMenuEvent(QContextMenuEvent *event)
{
    emit contextMenuRequested(mapToScene(event->pos()));
    event->accept();
}
