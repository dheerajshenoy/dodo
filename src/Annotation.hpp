#pragma once

#include <QBrush>
#include <QGraphicsRectItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QObject>
#include <QPen>
#include <qstyleoption.h>

class Annotation : public QObject, public QGraphicsItem
{
    Q_OBJECT
public:
    Annotation(int index, const QColor &color, QGraphicsItem *parent = nullptr);

    inline int index() noexcept
    {
        return m_index;
    }

    void select(const QColor &color) noexcept;
    void restoreBrushPen() noexcept;
    virtual void paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option, QWidget *widget);

signals:
    void annotDeleteRequested(int index);
    void annotColorChangeRequested(int index);
    void annotSelectRequested(int index);

protected:
    int m_index{-1};
    QBrush m_brush{Qt::transparent};
    QPen m_pen{Qt::NoPen};
    bool m_selected{false};

    QBrush m_originalBrush;
    QPen m_originalPen;
};
