#pragma once

#include <QGraphicsRectItem>
#include <QBrush>
#include <QPen>
#include <QObject>
#include <qgraphicsitem.h>

class HighlightItem : public QObject, public QGraphicsRectItem {
    Q_OBJECT
    public:
        HighlightItem(const QRectF &rect,
                int index,
                QGraphicsItem *parent = nullptr)
            : QGraphicsRectItem(rect, parent), m_index(index)
        {
            setPen(Qt::NoPen);
            setBrush(Qt::transparent);
        }

signals:
        void annotDeleteRequested(int index);

    protected:

        void mousePressEvent(QGraphicsSceneMouseEvent* e) override {
            // setBrush(Qt::red);
            // emit annotDeleteRequested(m_index);
            QGraphicsRectItem::mousePressEvent(e);
        }

        // void hoverEnterEvent(QGraphicsSceneHoverEvent *e) override {
        //     QGraphicsRectItem::hoverEnterEvent(e);
        //     setBrush(QBrush(QColor(1.0, 1.0, 0.0, 125)));
        // }
        //
        // void hoverLeaveEvent(QGraphicsSceneHoverEvent *e) override {
        //     QGraphicsRectItem::hoverLeaveEvent(e);
        //     setBrush(Qt::transparent);
        //     QApplication::restoreOverrideCursor();
        // }

    private:
        int m_index { -1 };
};
