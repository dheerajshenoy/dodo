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

        enum class Mode {
            None = 0,
            TextSelection,
            TextHighlight,
            AnnotSelect,
            AnnotRect,
        };

    explicit GraphicsView(QWidget *parent = nullptr)
    : QGraphicsView(parent)
    {
    setMouseTracking(true);
    setResizeAnchor(AnchorViewCenter);
    setTransformationAnchor(AnchorUnderMouse);
}


void setMode(Mode mode) noexcept
{
    switch(m_mode)
    {
        case Mode::None:
            break;

        case Mode::TextSelection:
            emit textSelectionDeletionRequested();
            break;

        case Mode::TextHighlight:
            emit textSelectionDeletionRequested();
            break;

        case Mode::AnnotRect:
        case Mode::AnnotSelect:
            break;
    }

    m_mode = mode;
}

inline QPointF selectionStart() noexcept { return m_selection_start; }
inline QPointF selectionEnd() noexcept { return m_selection_end; }

inline Mode mode() const noexcept { return m_mode; }

inline void setPixmapItem(QGraphicsPixmapItem *item) noexcept { m_pixmapItem = item; }
inline QGraphicsPixmapItem* pixmapItem() const noexcept { return m_pixmapItem; }

signals:
void highlightDrawn(const QRectF &pdfRect);
void textSelectionRequested(const QPointF &a, const QPointF &b);
void textHighlightRequested(const QPointF &a, const QPointF &b);
void textSelectionDeletionRequested();
void synctexJumpRequested(QPointF pos);
void annotSelectRequested(const QRectF &rect);

protected:
void mousePressEvent(QMouseEvent *event) override
{
    switch(m_mode)
    {
        case Mode::None:
            if (event->modifiers() & Qt::ShiftModifier)
            {
                emit synctexJumpRequested(mapToScene(event->pos()));
                return;
            }
            QGraphicsView::mousePressEvent(event);  // Let QGraphicsItems (like links) respond
            break;

        case Mode::AnnotRect:
        case Mode::AnnotSelect:
            if (event->button() == Qt::LeftButton)
            {
                m_start = event->pos();
                m_rect = QRect();

                if (!m_rubberBand)
                    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);

                m_rubberBand->setGeometry(QRect(m_start, QSize()));
                m_rubberBand->show();
                return;
            }
            break;

        case Mode::TextSelection:
            if (event->button() == Qt::LeftButton)
            {
                m_mousePressPos = event->pos();
                m_selecting = true;
                auto pos = mapToScene(event->pos());
                if (m_pixmapItem && m_pixmapItem->sceneBoundingRect().contains(pos))
                {
                    m_selection_start = pos;
                    emit textSelectionDeletionRequested();
                }
            }
            break;

        case Mode::TextHighlight:
            emit textSelectionDeletionRequested();
            break;
    }
}

void mouseMoveEvent(QMouseEvent *event) override
{
    switch(m_mode)
    {
        case Mode::None: break;

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
            return;

        case Mode::AnnotRect:
        case Mode::AnnotSelect:
            {
                m_rect = QRect(m_start, event->pos()).normalized();
                if (m_rubberBand)
                    m_rubberBand->setGeometry(m_rect);
            }
            break;
    }

    QGraphicsView::mouseMoveEvent(event);  // Allow hover/cursor events
}

void mouseReleaseEvent(QMouseEvent *event) override
{
    int dist = (event->pos() - m_mousePressPos).manhattanLength();

    switch(m_mode)
    {
        case Mode::TextSelection:
            {
                if (dist > CLICK_THRESHOLD) {
                    m_selection_end = mapToScene(event->pos());
                    emit textSelectionRequested(m_selection_start, m_selection_end);
                }
                m_selecting = false;
            }
            break;


        case Mode::TextHighlight:
            {
                if (dist > CLICK_THRESHOLD) {
                    m_selection_end = mapToScene(event->pos());
                    emit textHighlightRequested(m_selection_start, m_selection_end);
                }
                m_selecting = false;
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

        case Mode::None: break;
    }

    QGraphicsView::mouseReleaseEvent(event);  // Let items handle clicks
}

private:
QRect m_rect;
QPoint m_start;
QPoint m_mousePressPos;
QPointF m_selection_start, m_selection_end;
bool m_selecting { false };
Mode m_mode { Mode::None };
QGraphicsPixmapItem *m_pixmapItem { nullptr };
QRubberBand *m_rubberBand { nullptr };
};

