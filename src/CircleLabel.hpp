#pragma once

#include <QLabel>
#include <QPainter>

class CircleLabel : public QLabel
{
public:
    CircleLabel(QWidget *parent = nullptr) : QLabel(parent)
    {
        setFixedSize(24, 24); // or any square size
    }

    void setColor(const QColor &color)
    {
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(m_color);
        p.drawEllipse(rect().adjusted(1, 1, -1, -1)); // Avoid clipping
    }

private:
    QColor m_color;
};
