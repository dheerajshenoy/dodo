#pragma once
#include <QElapsedTimer>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QRubberBand>

class GraphicsPixmapItem;

class GraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    enum class Mode
    {
        RegionSelection,
        TextSelection,
        TextHighlight,
        AnnotSelect,
        AnnotRect,
        AnnotPopup,
        AnnotPen,
        None,
        COUNT
    };

    explicit GraphicsView(QWidget *parent = nullptr);

    QPointF selectionStart() const noexcept
    {
        return m_selection_start;
    }

    void setMode(Mode mode) noexcept;
    Mode mode() const noexcept
    {
        return m_mode;
    }

    void setSelectionDragThreshold(int value) noexcept
    {
        m_drag_threshold = value;
    }

    QPointF getCursorPos() const noexcept
    {
        return mapToScene(mapFromGlobal(QCursor::pos()));
    }

    inline Mode getNextMode() noexcept
    {
        return static_cast<Mode>((static_cast<int>(m_mode) + 1)
                                 % static_cast<int>(Mode::COUNT));
    }

    inline void setDefaultMode(Mode mode) noexcept
    {
        m_default_mode = mode;
    }

    inline Mode getDefaultMode() const noexcept
    {
        return m_default_mode;
    }

signals:
    void highlightDrawn(const QRectF &sceneRect);
    void textSelectionRequested(const QPointF &a, const QPointF &b);
    void textHighlightRequested(const QPointF &a, const QPointF &b);
    void textSelectionDeletionRequested();
#ifdef HAS_SYNCTEX
    void synctexJumpRequested(QPointF scenePos);
#endif
    void regionSelectRequested(const QRectF &sceneRect);
    void annotSelectRequested(const QRectF &sceneRect);
    void annotSelectRequested(const QPointF &scenePos);
    void annotSelectClearRequested();
    void zoomInRequested();
    void zoomOutRequested();
    void contextMenuRequested(QPoint globalPos);
    void rightClickRequested(QPointF scenePos);
    void doubleClickRequested(QPointF scenePos);
    void tripleClickRequested(QPointF scenePos);
    void quadrupleClickRequested(QPointF scenePos);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    // bool viewportEvent(QEvent *event) override;

private:
    QRect m_rect;
    QPoint m_start;
    QPointF m_mousePressPos;
    QPointF m_selection_start;

    bool m_selecting{false};
    bool m_dragging{false};
    bool m_ignore_next_release{false};
    Mode m_mode{Mode::TextSelection};
    Mode m_default_mode{Mode::None};

    QRubberBand *m_rubberBand{nullptr};
    int m_drag_threshold{50};

    // Multi-click tracking
    int m_clickCount{0};
    QElapsedTimer m_clickTimer;
    QPointF m_lastClickPos;
    static constexpr int MULTI_CLICK_INTERVAL        = 400;
    static constexpr double CLICK_DISTANCE_THRESHOLD = 5.0;
    QPoint m_lastMovePos;
    static constexpr int MOVE_EMIT_THRESHOLD_PX = 2;

    // Gesture state
    qreal m_lastPinchScale = 1.0;
    qreal m_zoomAccum
        = 0.0; // accumulate pinch/native zoom deltas to trigger steps
    qreal m_scrollAccumY
        = 0.0; // accumulate trackpad scroll to trigger page flips

    static constexpr qreal ZOOM_STEP_TRIGGER
        = 0.12; // ~12% “gesture energy” per step (tweak)
    static constexpr qreal PAGE_SCROLL_TRIGGER
        = 900.0; // pixels of trackpad scroll per page (tweak)

    // Utility
    QPointF scenePosFromEvent(QMouseEvent *event) const
    {
        return mapToScene(event->pos());
    }
};
