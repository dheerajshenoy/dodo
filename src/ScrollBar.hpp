
#pragma once
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionSlider>
#include <vector>

class ScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    explicit ScrollBar(Qt::Orientation o, QWidget *parent = nullptr) noexcept
        : QScrollBar(o, parent)
    {
        // Semi-transparent handle (thumb)
        setStyleSheet(
            "QScrollBar::handle:vertical { background: rgba(128,128,128,140); "
            "border-radius: 4px; min-height: 24px; }"
            "QScrollBar::handle:vertical:hover { background: "
            "rgba(160,160,160,170); }"
            "QScrollBar::handle:horizontal { background: "
            "rgba(128,128,128,140); border-radius: 4px; min-width: 24px; }"
            "QScrollBar::handle:horizontal:hover { background: "
            "rgba(160,160,160,170); }");
    }

    void setSearchMarkers(std::vector<double> markers) noexcept
    {
        if (markers.empty())
        {
            if (m_markers.empty())
                return;
            m_markers.clear();
            update();
            return;
        }
        if (markers == m_markers)
            return;

        m_markers = std::move(markers);
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        // 1) Paint the normal scrollbar first.
        QScrollBar::paintEvent(event);

        if (m_markers.empty())
            return;

        QStyleOptionSlider opt;
        initStyleOption(&opt);

        // Groove = track. Slider = thumb/handle.
        const QRect groove = style()->subControlRect(
            QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarGroove, this);

        if (!groove.isValid())
            return;

        const int minv  = minimum();
        const int maxv  = maximum();
        const int range = maxv - minv;
        if (range <= 0)
            return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);

        QPen pen(QColor(255, 200, 0, 200));
        pen.setWidth(2);
        p.setPen(pen);

        if (orientation() == Qt::Vertical)
        {
            const int top = groove.top();
            const int h   = groove.height();
            for (double mv : m_markers)
            {
                if (mv < minv || mv > maxv)
                    continue;
                const double t = (mv - minv) / double(range);
                const int y    = top + int(t * (h - 1));
                p.drawLine(groove.left(), y, groove.right(), y);
            }
        }
        else
        {
            const int left = groove.left();
            const int w    = groove.width();
            for (double mv : m_markers)
            {
                if (mv < minv || mv > maxv)
                    continue;
                const double t = (mv - minv) / double(range);
                const int x    = left + int(t * (w - 1));
                p.drawLine(x, groove.top(), x, groove.bottom());
            }
        }
    }

private:
    std::vector<double> m_markers;
};
