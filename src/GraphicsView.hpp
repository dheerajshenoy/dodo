#pragma once

#include <QCursor>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QRubberBand>
#include <mupdf/fitz.h>
#include <mupdf/fitz/pixmap.h>
#include <qevent.h>
#include <QGuiApplication>
#include <qgraphicssceneevent.h>

#define CLICK_THRESHOLD 50

class GraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    enum class Mode
    {
        TextSelection = 0,
        TextHighlight,
        AnnotSelect,
        AnnotRect
    };

    explicit GraphicsView(QWidget* parent = nullptr);
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
    inline void setPixmapItem(QGraphicsPixmapItem* item) noexcept
    {
        m_pixmapItem = item;
    }
    inline QGraphicsPixmapItem* pixmapItem() const noexcept
    {
        return m_pixmapItem;
    }
    void setMode(Mode mode) noexcept;

signals:
    void highlightDrawn(const QRectF& pdfRect);
    void textSelectionRequested(const QPointF& a, const QPointF& b);
    void textHighlightRequested(const QPointF& a, const QPointF& b);
    void textSelectionDeletionRequested();
    void synctexJumpRequested(QPointF pos);
    void annotSelectRequested(const QRectF& rect);
    void annotSelectClearRequested();
    void zoomInRequested();
    void zoomOutRequested();
    void populateContextMenuRequested(QMenu *menu);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;

private:
    QRect m_rect;
    QPoint m_start;
    QPoint m_mousePressPos;
    QPointF m_selection_start, m_selection_end;
    bool m_selecting{false};
    Mode m_mode{Mode::TextSelection};
    QGraphicsPixmapItem* m_pixmapItem{nullptr};
    QRubberBand* m_rubberBand{nullptr};
    bool m_has_text_selection{false};
};
