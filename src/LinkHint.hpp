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
    LinkHint(const QRectF &rect, const QColor &bg, const QColor &fg, const QString &hint, const float &fontSize)
        : m_rect(rect), m_bg(bg), m_fg(fg), m_hint(hint), m_fontSize(fontSize)
    {
        setData(0, "kb_link_overlay");
        setZValue(10);
    }

    QRectF boundingRect() const override
    {
        return m_rect;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_bg);
        painter->drawRect(m_rect);

        QFont font;
        font.setPointSizeF(m_fontSize);
        font.setBold(true);
        painter->setFont(font);
        painter->setPen(m_fg);

        QFontMetricsF metrics(font);
        QRectF textRect = metrics.boundingRect(m_hint);

        // Center text inside m_rect
        QPointF center = m_rect.center();
        QPointF textTopLeft(center.x() - textRect.width() / 2.0, center.y() + textRect.height() / 2.0);
        painter->drawText(textTopLeft, m_hint);
        painter->restore();
    }

private:
    QRectF m_rect;
    QColor m_bg;
    QColor m_fg;
    QString m_hint;
    float m_fontSize;
};
