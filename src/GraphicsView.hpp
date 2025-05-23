#pragma once

#include <QGraphicsView>
#include <QMouseEvent>
#include <QGraphicsRectItem>
#include <mupdf/fitz.h>
#include <mupdf/fitz/pixmap.h>
#include <QRubberBand>
#include <qrubberband.h>

class GraphicsView : public QGraphicsView {
    Q_OBJECT

public:
    GraphicsView(QWidget *parent = nullptr) : QGraphicsView(parent)
    {
        setMouseTracking(true);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    }

    inline void setDrawingMode(bool state) noexcept { m_drawing_mode = state; }
    inline bool drawingMode() noexcept { return m_drawing_mode; }
    inline void setPixmapItem(QGraphicsPixmapItem *item) noexcept {
        m_pixmapItem = item;
    }
    inline QGraphicsPixmapItem* pixmapItem() noexcept { return m_pixmapItem; }

    signals:
    void highlightDrawn(const QRectF &pdfRect);

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (!m_drawing_mode)
        {
            QGraphicsView::mousePressEvent(event);
            return;
        }

        m_start = event->pos();
        m_rect = QRect();

        if (!m_rubberBand)
            m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        m_rubberBand->setGeometry(QRect(m_start, QSize()));
        m_rubberBand->show();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!m_drawing_mode)
        {
            QGraphicsView::mousePressEvent(event);
            return;
        }

        m_rect = QRect(m_start, event->pos()).normalized();
        if (m_rubberBand)
            m_rubberBand->setGeometry(QRect(m_start, event->pos()).normalized());
        update();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (!m_drawing_mode)
        {
            QGraphicsView::mouseReleaseEvent(event);
            return;
        }

        m_rect = QRect(m_start, event->pos()).normalized();
        update();

        if (!m_rect.isEmpty())
        {
            if (m_rubberBand)
                m_rubberBand->hide();

            // Convert widget coordinates to scene coordinates
            QRectF sceneRect = mapToScene(m_rect).boundingRect();

            // Clamp to pixmap bounding rect
            QRectF pixmapRect = m_pixmapItem->boundingRect();

            QRectF clippedRect = sceneRect.intersected(pixmapRect);

            if (clippedRect.isEmpty())
                return;  // No highlight inside pixmap bounds

            // Create a semi-transparent yellow rectangle (highlight style)
            auto *highlightItem = scene()->addRect(
                clippedRect,
                QPen(Qt::yellow, 1.0, Qt::SolidLine),
                QBrush(QColor(255, 255, 0, 80)) // Transparent yellow
            );

            highlightItem->setFlag(QGraphicsItem::ItemIsSelectable);
            highlightItem->setFlag(QGraphicsItem::ItemIsMovable);

            emit highlightDrawn(clippedRect);

        }
    }

private:
    QRect m_rect;
    QPoint m_start;
    bool m_drawing_mode { false };
    fz_matrix m_transform;
    float m_dpiScale { 1.0f };
    QGraphicsPixmapItem *m_pixmapItem;
    QRubberBand *m_rubberBand;
};
