#include "GraphicsView.hpp"

#include <QGestureEvent>
#include <QGraphicsItem>
#include <QGuiApplication>
#include <QLineF>
#include <QMenu>
#include <QNativeGestureEvent>
// #include <QOpenGLWidget>
#include <QPinchGesture>
#include <QScroller>
#include <QSwipeGesture>
#include <qsurfaceformat.h>

GraphicsView::GraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    setMouseTracking(true);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setAcceptDrops(false);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);
    setOptimizationFlag(QGraphicsView::DontSavePainterState);
    setContentsMargins(0, 0, 0, 0);

    // QSurfaceFormat format;
    // format.setSamples(4);
    // QOpenGLWidget *glWidget = new QOpenGLWidget();
    // glWidget->setFormat(format);
    // setViewport(glWidget);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);

    // Enable gesture events on the viewport (important for QGraphicsView)
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);

    // Qt gesture framework (often touchscreens; sometimes trackpads depending
    // on platform/plugin)
    grabGesture(Qt::PinchGesture);
    grabGesture(Qt::SwipeGesture);

    // Optional: kinetic scrolling for touch (won’t harm trackpad, but mostly
    // for touchscreen) QScroller::grabGesture(viewport(),
    // QScroller::TouchGesture);

    if (!m_rubberBand)
        m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
    m_rubberBand->hide();
}

void
GraphicsView::clearRubberBand() noexcept
{
    if (!m_rubberBand)
        return;
    m_rubberBand->hide();
    m_rect = QRect();
}

void
GraphicsView::updateCursorForMode() noexcept
{
#ifndef NDEBUG
    qDebug() << "GraphicsView::updateCursorForMode(): Updating cursor for "
             << "mode:" << static_cast<int>(m_mode);
#endif
    if (m_selecting
        && (m_mode == Mode::TextSelection || m_mode == Mode::TextHighlight))
    {
        setCursor(Qt::IBeamCursor);
    }
    else
    {
        unsetCursor();
    }
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
                m_rubberBand->hide();
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
    updateCursorForMode();
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

    if ((m_mode == Mode::TextSelection || m_mode == Mode::TextHighlight)
        && event->button() == Qt::LeftButton)
    {
        if (QGraphicsItem *item = itemAt(event->pos()))
        {
            if (item->data(0).toString() == "link")
            {
                QGraphicsView::mousePressEvent(event);
                return;
            }
        }
    }

    // Multi-click tracking (avoid QLineF sqrt)
    if (m_mode == Mode::TextSelection && event->button() == Qt::LeftButton)
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
                m_selecting = true;
                updateCursorForMode();
                const QPointF scenePos = mapToScene(event->pos());
                m_mousePressPos        = scenePos;
                m_selection_start      = scenePos;
                m_lastMovePos          = event->pos();

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
        if (m_mode == Mode::TextSelection || m_mode == Mode::TextHighlight)
            emit textSelectionRequested(m_selection_start, scenePos);
        // else
        //     emit textHighlightRequested(m_selection_start, scenePos);

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
        updateCursorForMode();

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
            {
                emit textSelectionRequested(m_selection_start, scenePos);
                emit textHighlightRequested(m_selection_start, scenePos);
            }
        }

        m_dragging = false;
        event->accept();
        return; // handled
    }

    // Rubber band modes
    if (m_mode == Mode::RegionSelection || m_mode == Mode::AnnotRect
        || m_mode == Mode::AnnotSelect)
    {
        const QRectF sceneRect  = mapToScene(m_rect).boundingRect();
        const bool hasSelection = m_dragging && !sceneRect.isEmpty();

        if (m_mode != Mode::RegionSelection || !hasSelection)
            clearRubberBand();

        if (!m_dragging && m_mode == Mode::AnnotSelect)
        {
            emit annotSelectRequested(mapToScene(event->pos()));
        }
        else
        {
            if (hasSelection)
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

    // const int delta = !event->pixelDelta().isNull() ? event->pixelDelta().y()
    //                                                 :
    //                                                 event->angleDelta().y();

    QGraphicsView::wheelEvent(event);
}

void
GraphicsView::contextMenuEvent(QContextMenuEvent *event)
{
    QGraphicsView::contextMenuEvent(event);
    if (!event->isAccepted())
    {
        emit contextMenuRequested(event->globalPos());
        event->accept();
    }
}

// bool
// GraphicsView::viewportEvent(QEvent *event)
// {
//     // 1) High-level Qt gestures (Pinch/Swipe)
//     if (event->type() == QEvent::Gesture)
//     {
//         auto *ge = static_cast<QGestureEvent *>(event);

//         // Pinch zoom (touchscreens; sometimes trackpads)
//         if (QGesture *g = ge->gesture(Qt::PinchGesture))
//         {
//             auto *pinch = static_cast<QPinchGesture *>(g);

//             if (pinch->state() == Qt::GestureStarted)
//             {
//                 m_lastPinchScale = pinch->scaleFactor();
//                 m_zoomAccum      = 0.0;
//             }
//             else
//             {
//                 const qreal scaleNow = pinch->scaleFactor();
//                 const qreal ratio    = (m_lastPinchScale > 0.0)
//                                            ? (scaleNow / m_lastPinchScale)
//                                            : 1.0;
//                 m_lastPinchScale     = scaleNow;

//                 // Convert multiplicative ratio into additive “energy”
//                 // log(ratio) is symmetrical for in/out.
//                 if (ratio > 0.0)
//                     m_zoomAccum += std::log(ratio);

//                 while (m_zoomAccum > ZOOM_STEP_TRIGGER)
//                 {
//                     emit zoomInRequested();
//                     m_zoomAccum -= ZOOM_STEP_TRIGGER;
//                 }
//                 while (m_zoomAccum < -ZOOM_STEP_TRIGGER)
//                 {
//                     emit zoomOutRequested();
//                     m_zoomAccum += ZOOM_STEP_TRIGGER;
//                 }
//             }

//             ge->accept(pinch);
//             return true;
//         }

//         // Swipe gesture (usually touch). Map it to page nav if you like.
//         if (QGesture *g = ge->gesture(Qt::SwipeGesture))
//         {
//             auto *swipe = static_cast<QSwipeGesture *>(g);
//             if (swipe->state() == Qt::GestureFinished)
//             {
//                 // You can choose Up/Down or Left/Right for pages.
//                 // This example uses vertical swipe for pages:
//                 // Left or right swipe could be used for horizontal page nav.
//                 if (swipe->horizontalDirection() == QSwipeGesture::Left)
//                     emit scrollHorizontalRequested(swipe->swipeAngle() > 0 ?
//                     -1
//                                                                            :
//                                                                            1);
//                 else if (swipe->horizontalDirection() ==
//                 QSwipeGesture::Right)
//                     emit scrollHorizontalRequested(
//                         swipe->swipeAngle() > 0 ? 1 : -1);

//                 if (swipe->verticalDirection() == QSwipeGesture::Up)
//                     emit scrollVerticalRequested(swipe->swipeAngle() > 0 ? -1
//                                                                          :
//                                                                          1);
//                 else if (swipe->verticalDirection() == QSwipeGesture::Down)
//                     emit scrollVerticalRequested(swipe->swipeAngle() > 0 ? 1
//                                                                          :
//                                                                          -1);
//             }

//             ge->accept(swipe);
//             return true;
//         }

//         return QGraphicsView::viewportEvent(event);
//     }

//     // 2) Native gestures (trackpads: macOS; sometimes Wayland/X11 depending
//     on
//     // Qt/platform)
//     if (event->type() == QEvent::NativeGesture)
//     {
//         auto *ng = static_cast<QNativeGestureEvent *>(event);

//         switch (ng->gestureType())
//         {
//                 // case QNativeGestureEvent::Type::Ge
//                 // {
//                 //     // Qt provides a value per event; treat it as small
//                 //     deltas and
//                 //     // accumulate. On many systems: positive => zoom in,
//                 //     negative =>
//                 //     // zoom out.
//                 //     m_zoomAccum += ng->value();

//                 //     while (m_zoomAccum > ZOOM_STEP_TRIGGER)
//                 //     {
//                 //         emit zoomInRequested();
//                 //         m_zoomAccum -= ZOOM_STEP_TRIGGER;
//                 //     }
//                 //     while (m_zoomAccum < -ZOOM_STEP_TRIGGER)
//                 //     {
//                 //         emit zoomOutRequested();
//                 //         m_zoomAccum += ZOOM_STEP_TRIGGER;
//                 //     }

//                 //     event->accept();
//                 //     return true;
//                 // }

//                 // case QNativeGestureEvent::Swipe:
//                 // {
//                 //     // Some platforms provide swipe as native gesture.
//                 //     // ng->value() is typically +/-1-ish; direction
//                 mapping
//                 //     varies. const qreal v = ng->value(); if (v > 0)
//                 //         emit nextPageRequested();
//                 //     else if (v < 0)
//                 //         emit prevPageRequested();

//                 //     event->accept();
//                 //     return true;
//                 // }

//             default:
//                 break;
//         }

//         return QGraphicsView::viewportEvent(event);
//     }

//     return QGraphicsView::viewportEvent(event);
// }
