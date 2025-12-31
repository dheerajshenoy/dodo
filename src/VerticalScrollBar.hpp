#pragma once

#include <QGraphicsRectItem>
#include <QMap>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionSlider>
#include <QWidget>
#include <vector>

class VerticalScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    VerticalScrollBar(Qt::Orientation orientation,
                      QWidget *parent = nullptr) noexcept
        : QScrollBar(orientation, parent)
    {
    }

    void setSearchMarkers(const std::vector<double> &markers) noexcept
    {
        if (markers.empty())
        {
            m_search_markers.clear();
            update();
            return;
        }

        m_search_markers = markers;
        update();
    }

    // void paintEvent(QPaintEvent *event) override
    // {

    //     QPainter painter(this);
    //     painter.setPen(Qt::NoPen);
    //     painter.setBrush(QBrush(Qt::red));
    //     painter.setOpacity(0.6);

    //     const int h     = height();
    //     const int w     = width();
    //     const int range = maximum() - minimum() + pageStep();

    //     for (const double &marker : m_search_markers)
    //     {
    //         int y = static_cast<int>((marker - minimum()) / range * h);
    //         painter.drawRect(0, y, w, 2); // Draw a small rectangle as marker
    //     }

    //     QScrollBar::paintEvent(event);
    // }

    void paintEvent(QPaintEvent *event) override
    {
        QScrollBar::paintEvent(event); // Draw the standard scrollbar
        if (!m_search_markers.empty())
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);

            // Use the accent color for search hits
            painter.setPen(QColor(150, 150, 0, 180));

            // Get the groove rectangle (where the handle moves)
            QStyleOptionSlider opt;
            initStyleOption(&opt);
            QRect groove = style()->subControlRect(
                QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarGroove, this);

            // Map scene positions to groove height
            double sceneHeight = maximum() + pageStep();
            if (sceneHeight <= 0)
                return;

            for (double posY : m_search_markers)
            {
                float ratio = static_cast<float>(posY) / sceneHeight;
                int y       = groove.top() + (ratio * groove.height());
                painter.drawLine(0, y, width(), y);
            }
            painter.end();
        }
    }

private:
    std::vector<double> m_search_markers;
};
