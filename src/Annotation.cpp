#include "Annotation.hpp"

#include <QPainter>

Annotation::Annotation(int index, QGraphicsItem *parent) : QGraphicsItem(parent), m_index(index)
{
    setAcceptHoverEvents(true);
    setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
}

void
Annotation::select(const QColor &color) noexcept
{
    m_originalBrush = m_brush;
    m_originalPen   = m_pen;

    // setBrush(color);
    m_pen = QPen(color, 1, Qt::PenStyle::SolidLine);

    update();
}

void
Annotation::restoreBrushPen() noexcept
{
    m_brush = m_originalBrush;
    m_pen   = m_originalPen;

    update(); // Trigger repaint
}

void
Annotation::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    // Default visual — for example, a red rectangle
    painter->setPen(QPen(Qt::red, 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(boundingRect());
}
