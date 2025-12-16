#pragma once

#include <QCursor>
#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QRubberBand>

extern "C"
{
#include <mupdf/fitz.h>
#include <mupdf/fitz/pixmap.h>
}

#include <qevent.h>
#include <qgraphicssceneevent.h>

class GraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    enum class Mode
    {
        RegionSelection = 0,
        TextSelection,
        TextHighlight,
        AnnotSelect,
        AnnotRect,
        COUNT
    };

    QPointF getCursorPos() noexcept;
    explicit GraphicsView(QWidget *parent = nullptr);

    inline QPointF selectionStart() noexcept
    {
        return m_selection_start;
    }

    inline QPointF selectionEnd() noexcept
    {
        return m_selection_end;
    }

    inline Mode mode() const noexcept
    {
        return m_mode;
    }

    inline QRubberBand *rubberBand() const noexcept
    {
        return m_rubberBand;
    }

    inline void setPixmapItem(QGraphicsPixmapItem *item) noexcept
    {
        m_pixmapItem = item;
    }

    inline QGraphicsPixmapItem *pixmapItem() const noexcept
    {
        return m_pixmapItem;
    }

    void setMode(Mode mode) noexcept;

    inline Mode getNextMode() noexcept
    {
        return static_cast<Mode>((static_cast<int>(m_mode) + 1)
                                 % static_cast<int>(Mode::COUNT));
    }

    inline Mode getPrevMode() noexcept
    {
        return static_cast<Mode>((static_cast<int>(m_mode) - 1)
                                 % static_cast<int>(Mode::COUNT));
    }

    void setPageNavWithMouse(bool state) noexcept
    {
        m_page_nav_with_mouse = state;
    }
    void setSelectionDragThreshold(int value) noexcept
    {
        m_drag_threshold = value;
    }

signals:
    void highlightDrawn(const QRectF &pdfRect);
    void textSelectionRequested(const QPointF &a, const QPointF &b);
    void textHighlightRequested(const QPointF &a, const QPointF &b);
    void textSelectionDeletionRequested();
    void synctexJumpRequested(QPointF pos);
    void regionSelectRequested(const QRectF &rect);
    void annotSelectRequested(const QRectF &rect);
    void annotSelectRequested(const QPointF &);
    void annotSelectClearRequested();
    void zoomInRequested();
    void zoomOutRequested();
    void populateContextMenuRequested(QMenu *menu);
    void scrollRequested(int direction);
    void rightClickRequested(QPointF scenePos);
    void doubleClickRequested(QPointF scenePos);
    void tripleClickRequested(QPointF scenePos);
    void quadrupleClickRequested(QPointF scenePos);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;

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
    QGraphicsPixmapItem *m_pixmapItem{nullptr};
    QRubberBand *m_rubberBand{nullptr};
    int m_drag_threshold{50};

    // Multi-click tracking
    int m_clickCount{0};
    QElapsedTimer m_clickTimer;
    QPointF m_lastClickPos;
    static constexpr int MULTI_CLICK_INTERVAL        = 400; // ms
    static constexpr double CLICK_DISTANCE_THRESHOLD = 5.0; // pixels
};
