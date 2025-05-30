#pragma once

#include <QGraphicsRectItem>
#include <QObject>
#include <QPainter>

class JumpMarker : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    JumpMarker(const QRectF &rect, const QColor &color, QGraphicsItem *parent = nullptr)
        : QGraphicsRectItem(rect, parent)
    {
        setOpacity(1.0);
        setBrush(color);
        setPen(Qt::NoPen);
    }

    JumpMarker(const QColor &color, QGraphicsItem *parent = nullptr) : QGraphicsRectItem(parent)
    {
        setOpacity(1.0);
        setBrush(color);
        setPen(Qt::NoPen);
    }
};
