#pragma once

#include <QBrush>
#include <QFont>
#include <QGraphicsItem>
#include <QPainter>
#include <QPen>
#include <QRectF>

class LinkHint : public QGraphicsItem
{
public:
    LinkHint(const QRectF &rect, const QColor &bg, const QColor &fg, int hint,
             const float &fontSize)
        : m_rect(rect), m_bg(bg), m_fg(fg), m_hint(hint), m_fontSize(fontSize)
    {
        setData(0, "kb_link_overlay");
        setZValue(10);
    }

    QRectF boundingRect() const override
    {
        return m_rect;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *,
               QWidget *) override
    {
        painter->save();
        painter->setPen(Qt::PenStyle::SolidLine);
        painter->setBrush(m_bg);
        painter->drawRect(m_rect);

        QFont font;
        font.setPointSizeF(m_fontSize);
        font.setBold(true);
        painter->setFont(font);
        painter->setPen(m_fg);

        painter->drawText(m_rect, Qt::AlignCenter, QString::number(m_hint));

        painter->restore();
    }

private:
    QRectF m_rect;
    QColor m_bg;
    QColor m_fg;
    int m_hint;
    float m_fontSize;
};
