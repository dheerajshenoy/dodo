#pragma once

#include "Annotation.hpp"

#include <QGraphicsView>
#include <QPainter>
#include <QToolTip>
#include <qgraphicssceneevent.h>
#include <qnamespace.h>

class RectAnnotation : public Annotation
{
public:
    RectAnnotation(const QRectF &rect, int index, const QColor &color,
                   QGraphicsItem *parent = nullptr)
        : Annotation(index, color, parent), m_rect(rect)
    {
    }

    QRectF boundingRect() const override
    {
        return m_rect;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override
    {
        Q_UNUSED(option);
        Q_UNUSED(widget);

        painter->setPen(m_pen);
        painter->setBrush(m_brush);
        painter->drawRect(m_rect);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        Q_UNUSED(event);

        if (event->button() == Qt::LeftButton)
        {
            emit annotSelectRequested();
        }

        event->accept();
    }

private:
    QRectF m_rect;
};
