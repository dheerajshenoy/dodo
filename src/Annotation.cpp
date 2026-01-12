#include "Annotation.hpp"

#include <QPainter>
Annotation::Annotation(int index, const QColor &color, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_index(index), m_brush(color)
{
    m_originalBrush = m_brush;
    m_originalPen   = m_pen;
    setData(0, "annot");
    setData(3, color);
    setAcceptHoverEvents(true);
    setFlags(QGraphicsItem::ItemIsSelectable);
}

void
Annotation::select(const QColor &color) noexcept
{
    if (m_selected)
        return;
    m_originalBrush = m_brush;
    m_originalPen   = m_pen;

    // setBrush(color);
    // m_brush = QBrush(color, Qt::BrushStyle::NoBrush);
    m_pen      = QPen(color, 3, Qt::PenStyle::SolidLine);
    m_selected = true;

    update();
}

void
Annotation::restoreBrushPen() noexcept
{
    m_brush = m_originalBrush;
    m_pen   = m_originalPen;

    m_selected = false;

    update(); // Trigger repaint
}

void
Annotation::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                  QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setPen(m_pen);
    painter->setBrush(m_brush);
    painter->drawRect(boundingRect());
}
