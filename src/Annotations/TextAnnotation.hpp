#pragma once

#include "Annotation.hpp"

#include <QApplication>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolTip>

class TextAnnotation : public Annotation
{
    Q_OBJECT
public:
    TextAnnotation(const QRectF &rect, int index, const QColor &color,
                   const QString &text, QGraphicsItem *parent = nullptr)
        : Annotation(index, color, parent), m_rect(rect), m_text(text)
    {
        setAcceptHoverEvents(true);
        setCursor(Qt::PointingHandCursor);

        // Timer for delayed tooltip show
        m_hoverTimer = new QTimer();
        m_hoverTimer->setSingleShot(true);
        m_hoverTimer->setInterval(300); // 300ms delay before showing

        connect(m_hoverTimer, &QTimer::timeout, this,
                [this]() { showTooltip(); });
    }

    ~TextAnnotation() override
    {
        m_hoverTimer->stop();
        delete m_hoverTimer;
        hideTooltip();
    }

    QRectF boundingRect() const override
    {
        // The note icon is typically 24x24 pixels, centered at the annotation
        // position. Add extra space for hover glow effect.
        constexpr qreal iconSize   = 24.0;
        constexpr qreal glowMargin = 4.0;
        return QRectF(
            m_rect.topLeft() - QPointF(glowMargin, glowMargin),
            QSizeF(iconSize + glowMargin * 2, iconSize + glowMargin * 2));
    }

    QRectF iconRect() const
    {
        constexpr qreal iconSize   = 24.0;
        constexpr qreal glowMargin = 4.0;
        return QRectF(m_rect.topLeft() + QPointF(glowMargin, glowMargin)
                          - QPointF(glowMargin, glowMargin),
                      QSizeF(iconSize, iconSize));
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override
    {
        Q_UNUSED(option);
        Q_UNUSED(widget);

        painter->setRenderHint(QPainter::Antialiasing);

        constexpr qreal iconSize = 24.0;
        QRectF iconR(m_rect.topLeft(), QSizeF(iconSize, iconSize));

        // Draw hover glow effect
        if (m_hovered)
        {
            QColor glowColor(255, 200, 0, 100);
            for (int i = 3; i >= 1; --i)
            {
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor(255, 200, 0, 30 * (4 - i)));
                painter->drawRoundedRect(
                    iconR.adjusted(-i * 2, -i * 2, i * 2, i * 2), 4 + i, 4 + i);
            }
        }

        // Draw note icon background
        QPainterPath notePath;
        constexpr qreal foldSize = 6.0;

        // Main rectangle with folded corner
        notePath.moveTo(iconR.topLeft());
        notePath.lineTo(iconR.topRight() - QPointF(foldSize, 0));
        notePath.lineTo(iconR.topRight() + QPointF(0, foldSize));
        notePath.lineTo(iconR.bottomRight());
        notePath.lineTo(iconR.bottomLeft());
        notePath.closeSubpath();

        // Fill with annotation color
        QColor fillColor = m_brush.color();
        if (!fillColor.isValid() || fillColor.alpha() == 0)
            fillColor = QColor(255, 255, 0, 200); // Default yellow

        // Brighten on hover
        if (m_hovered)
            fillColor = fillColor.lighter(115);

        painter->fillPath(notePath, fillColor);

        // Draw the fold
        QPainterPath foldPath;
        foldPath.moveTo(iconR.topRight() - QPointF(foldSize, 0));
        foldPath.lineTo(iconR.topRight() - QPointF(foldSize, -foldSize));
        foldPath.lineTo(iconR.topRight() + QPointF(0, foldSize));
        foldPath.closeSubpath();

        QColor foldColor = fillColor.darker(120);
        painter->fillPath(foldPath, foldColor);

        // Draw border - thicker and more prominent on hover
        if (m_hovered)
            painter->setPen(QPen(fillColor.darker(180), 2));
        else
            painter->setPen(QPen(fillColor.darker(150), 1));
        painter->drawPath(notePath);

        // Draw selection highlight if selected
        if (m_selected)
        {
            painter->setPen(m_pen);
            painter->drawRect(iconR.adjusted(-2, -2, 2, 2));
        }

        // Draw small lines to indicate text content
        painter->setPen(QPen(fillColor.darker(180), 1));
        qreal lineY     = iconR.top() + 8;
        qreal lineLeft  = iconR.left() + 4;
        qreal lineRight = iconR.right() - 6; // Leave space for folded corner

        for (int i = 0; i < 3 && lineY < iconR.bottom() - 4; ++i)
        {
            painter->drawLine(QPointF(lineLeft, lineY),
                              QPointF(lineRight - (i == 0 ? 2 : 0), lineY));
            lineY += 4;
        }
    }

    inline void setText(const QString &text)
    {
        m_text = text;
    }

    inline const QString &text() const
    {
        return m_text;
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            emit annotSelectRequested();
        }
        event->accept();
    }

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            emit editRequested();
        }
        event->accept();
    }

    void contextMenuEvent(QGraphicsSceneContextMenuEvent *e) override
    {
        QMenu menu;

        QAction *editAction        = new QAction("Edit");
        QAction *deleteAction      = new QAction("Delete");
        QAction *changeColorAction = new QAction("Change Color");

        connect(editAction, &QAction::triggered,
                [this]() { emit editRequested(); });
        connect(deleteAction, &QAction::triggered,
                [this]() { emit annotDeleteRequested(); });
        connect(changeColorAction, &QAction::triggered,
                [this]() { emit annotColorChangeRequested(); });

        menu.addAction(editAction);
        menu.addSeparator();
        menu.addAction(changeColorAction);
        menu.addAction(deleteAction);
        menu.exec(e->screenPos());
        e->accept();
    }

signals:
    void editRequested();

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override
    {
        m_hovered  = true;
        m_hoverPos = event->screenPos();
        m_hoverTimer->start();
        update(); // Trigger repaint for hover effect
        Annotation::hoverEnterEvent(event);
    }

    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override
    {
        m_hoverPos = event->screenPos();
        Annotation::hoverMoveEvent(event);
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override
    {
        m_hovered = false;
        m_hoverTimer->stop();
        hideTooltip();
        update(); // Trigger repaint to remove hover effect
        Annotation::hoverLeaveEvent(event);
    }

private:
    void showTooltip()
    {
        if (!m_text.isEmpty())
        {
            // Get the widget to use as parent for the tooltip
            QWidget *widget = nullptr;
            if (scene() && !scene()->views().isEmpty())
            {
                widget = scene()->views().first()->viewport();
            }

            // Show styled tooltip using QToolTip
            QString styledText
                = QString(
                      "<div style='background-color: #ffffc0; padding: 6px; "
                      "border: 1px solid #c0c080; border-radius: 4px;'>"
                      "<p style='margin: 0; color: #222; white-space: "
                      "pre-wrap;'>%1</p>"
                      "</div>")
                      .arg(m_text.toHtmlEscaped());

            QToolTip::showText(m_hoverPos, styledText, widget);
        }
    }

    inline void hideTooltip() noexcept
    {
        QToolTip::hideText();
    }

    QRectF m_rect;
    QString m_text;
    QTimer *m_hoverTimer{nullptr};
    QPoint m_hoverPos;
    bool m_hovered{false};
};
