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
        COUNT
    };

    explicit GraphicsView(QWidget *parent = nullptr);

    QPointF selectionStart() const noexcept
    {
        return m_selection_start;
    }
    QPointF selectionEnd() const noexcept
    {
        return m_selection_end;
    }

    void setMode(Mode mode) noexcept;
    Mode mode() const noexcept
    {
        return m_mode;
    }

    void setPageNavWithMouse(bool state) noexcept
    {
        m_page_nav_with_mouse = state;
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

signals:
    void highlightDrawn(const QRectF &sceneRect);
    void textSelectionRequested(const QPointF &a, const QPointF &b);
    void textHighlightRequested(const QPointF &a, const QPointF &b);
    void textSelectionDeletionRequested();
    void synctexJumpRequested(QPointF scenePos);
    void regionSelectRequested(const QRectF &sceneRect);
    void annotSelectRequested(const QRectF &sceneRect);
    void annotSelectRequested(const QPointF &scenePos);
    void annotSelectClearRequested();
    void zoomInRequested();
    void zoomOutRequested();
    void contextMenuRequested(QPointF scenePos);
    void scrollRequested(int delta);
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

private:
    QRect m_rect;
    QPoint m_start;
    QPointF m_mousePressPos;
    QPointF m_selection_start, m_selection_end;

    bool m_selecting{false};
    bool m_dragging{false};
    bool m_page_nav_with_mouse{true};
    bool m_ignore_next_release{false};
    Mode m_mode{Mode::TextSelection};

    QRubberBand *m_rubberBand{nullptr};
    int m_drag_threshold{50};

    // Multi-click tracking
    int m_clickCount{0};
    QElapsedTimer m_clickTimer;
    QPointF m_lastClickPos;
    static constexpr int MULTI_CLICK_INTERVAL        = 400;
    static constexpr double CLICK_DISTANCE_THRESHOLD = 5.0;

    // Utility
    QPointF scenePosFromEvent(QMouseEvent *event) const
    {
        return mapToScene(event->pos());
    }
};
