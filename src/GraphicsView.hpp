#pragma once

#include <QGraphicsView>
#include <QMouseEvent>
#include <QGraphicsRectItem>
#include <QRubberBand>
#include <mupdf/fitz.h>
#include <mupdf/fitz/pixmap.h>

#define CLICK_THRESHOLD 10

class GraphicsView : public QGraphicsView {
    Q_OBJECT

    public:
    explicit GraphicsView(QWidget *parent = nullptr)
    : QGraphicsView(parent)
    {
    setMouseTracking(true);
    setResizeAnchor(AnchorViewCenter);
    setTransformationAnchor(AnchorUnderMouse);
}

inline QPointF selectionStart() noexcept { return m_selection_start; }
inline QPointF selectionEnd() noexcept { return m_selection_end; }

inline void setDrawingMode(bool state) noexcept { m_drawing_mode = state; }
inline bool drawingMode() const noexcept { return m_drawing_mode; }
inline void setPixmapItem(QGraphicsPixmapItem *item) noexcept { m_pixmapItem = item; }
inline QGraphicsPixmapItem* pixmapItem() const noexcept { return m_pixmapItem; }

signals:
void highlightDrawn(const QRectF &pdfRect);
void textSelectionRequested(const QPointF &a, const QPointF &b);
void textSelectionDeletionRequested();
void synctexJumpRequested(QPointF pos);

protected:
void mousePressEvent(QMouseEvent *event) override
    {
        m_mousePressPos = event->pos();

if (event->modifiers() & Qt::ShiftModifier)
{
    emit synctexJumpRequested(mapToScene(event->pos()));
    return;
}

if (m_drawing_mode && event->button() == Qt::LeftButton)
{
    m_start = event->pos();
    m_rect = QRect();

    if (!m_rubberBand)
        m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);

    m_rubberBand->setGeometry(QRect(m_start, QSize()));
    m_rubberBand->show();
}
else if (event->button() == Qt::LeftButton)
{
    auto pos = mapToScene(event->pos());
    if (m_pixmapItem && m_pixmapItem->sceneBoundingRect().contains(pos))
    {
        m_selecting = true;
        m_selection_start = pos;
        emit textSelectionDeletionRequested();
    }
    QGraphicsView::mousePressEvent(event);  // Let QGraphicsItems (like links) respond
}
else
            {
    QGraphicsView::mousePressEvent(event);
}
    }

void mouseMoveEvent(QMouseEvent *event) override
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
    else if (m_drawing_mode)
    {
        m_rect = QRect(m_start, event->pos()).normalized();
        if (m_rubberBand)
            m_rubberBand->setGeometry(m_rect);
    }

    QGraphicsView::mouseMoveEvent(event);  // Allow hover/cursor events
}

void mouseReleaseEvent(QMouseEvent *event) override
{
    int dist = (event->pos() - m_mousePressPos).manhattanLength();

    if (m_selecting)
    {
        m_selecting = false;
        if (dist > CLICK_THRESHOLD) {
            m_selection_end = mapToScene(event->pos());
            emit textSelectionRequested(m_selection_start, m_selection_end);
        }
    }
    else if (m_drawing_mode && event->button() == Qt::LeftButton)
    {
        if (m_rubberBand)
            m_rubberBand->hide();

        QRectF sceneRect = mapToScene(m_rect).boundingRect();
        if (!m_pixmapItem)
            return;

        QRectF clippedRect = sceneRect.intersected(m_pixmapItem->boundingRect());
        if (!clippedRect.isEmpty())
        {
            auto *highlightItem = scene()->addRect(
                clippedRect,
                QPen(Qt::yellow, 1.0, Qt::SolidLine),
                QBrush(QColor(255, 255, 0, 80))  // Transparent yellow
            );
            highlightItem->setFlag(QGraphicsItem::ItemIsSelectable);
            highlightItem->setFlag(QGraphicsItem::ItemIsMovable);

            emit highlightDrawn(clippedRect);
        }
    }

    QGraphicsView::mouseReleaseEvent(event);  // Let items handle clicks
}

private:
QRect m_rect;
QPoint m_start;
QPoint m_mousePressPos;
QPointF m_selection_start, m_selection_end;
bool m_drawing_mode { false };
bool m_selecting { false };
QGraphicsPixmapItem *m_pixmapItem { nullptr };
QRubberBand *m_rubberBand { nullptr };
};

