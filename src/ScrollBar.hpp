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
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setContextMenuPolicy(Qt::NoContextMenu);
        applyStyle();
    }

    void setSize(int size) noexcept
    {
        if (m_size != size)
        {
            m_size = size;
            applyStyle();
        }
    }

    void setSearchMarkers(std::vector<double> markers) noexcept
    {
        if (markers.empty())
        {
            if (m_markers.empty())
                return;
            m_markers.clear();
        }
        else
        {
            if (markers == m_markers)
                return;
            m_markers = std::move(markers);
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QScrollBar::paintEvent(event);

        if (m_markers.empty())
            return;

        QStyleOptionSlider opt;
        initStyleOption(&opt);

        const QRect groove = style()->subControlRect(
            QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarGroove, this);
        if (!groove.isValid())
            return;

        const int range = maximum() - minimum();
        if (range <= 0)
            return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(QColor(255, 200, 0, 200), 2));

        const int minv = minimum();
        const int maxv = maximum();

        if (orientation() == Qt::Vertical)
        {
            const int top = groove.top();
            const int h   = groove.height() - 1;
            for (const double mv : m_markers)
            {
                if (mv < minv || mv > maxv)
                    continue;
                const int y = top + static_cast<int>((mv - minv) / range * h);
                p.drawLine(groove.left(), y, groove.right(), y);
            }
        }
        else
        {
            const int left = groove.left();
            const int w    = groove.width() - 1;
            for (const double mv : m_markers)
            {
                if (mv < minv || mv > maxv)
                    continue;
                const int x = left + static_cast<int>((mv - minv) / range * w);
                p.drawLine(x, groove.top(), x, groove.bottom());
            }
        }
    }

private:
    void applyStyle() noexcept
    {
        const int radius = m_size / 2 - 1;
        if (orientation() == Qt::Vertical)
        {
            setStyleSheet(
                QStringLiteral(
                    "QScrollBar:vertical {"
                    "  background: transparent; width: %1px; margin: 0; "
                    "border: none;"
                    "}"
                    "QScrollBar::handle:vertical {"
                    "  background: rgba(128,128,128,160); border-radius: %2px;"
                    "  min-height: 30px; margin: 2px;"
                    "}"
                    "QScrollBar::handle:vertical:hover { background: "
                    "rgba(160,160,160,200); }"
                    "QScrollBar::handle:vertical:pressed { background: "
                    "rgba(100,100,100,220); }"
                    "QScrollBar::add-line:vertical, "
                    "QScrollBar::sub-line:vertical {"
                    "  height: 0; background: transparent;"
                    "}"
                    "QScrollBar::add-page:vertical, "
                    "QScrollBar::sub-page:vertical {"
                    "  background: transparent;"
                    "}")
                    .arg(m_size)
                    .arg(radius));
        }
        else
        {
            setStyleSheet(
                QStringLiteral(
                    "QScrollBar:horizontal {"
                    "  background: transparent; height: %1px; margin: 0; "
                    "border: none;"
                    "}"
                    "QScrollBar::handle:horizontal {"
                    "  background: rgba(128,128,128,160); border-radius: %2px;"
                    "  min-width: 30px; margin: 2px;"
                    "}"
                    "QScrollBar::handle:horizontal:hover { background: "
                    "rgba(160,160,160,200); }"
                    "QScrollBar::handle:horizontal:pressed { background: "
                    "rgba(100,100,100,220); }"
                    "QScrollBar::add-line:horizontal, "
                    "QScrollBar::sub-line:horizontal {"
                    "  width: 0; background: transparent;"
                    "}"
                    "QScrollBar::add-page:horizontal, "
                    "QScrollBar::sub-page:horizontal {"
                    "  background: transparent;"
                    "}")
                    .arg(m_size)
                    .arg(radius));
        }
    }

    int m_size{12};
    std::vector<double> m_markers;
};
