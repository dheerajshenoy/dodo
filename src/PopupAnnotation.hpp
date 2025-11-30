#pragma once

#include "Annotation.hpp"

#include <QGraphicsView>
#include <QPainter>
#include <QTextEdit>
#include <QToolTip>
#include <QVBoxLayout>
#include <qgraphicssceneevent.h>

class PopupWidget : public QWidget
{
public:
    PopupWidget(QWidget *parent = nullptr)
        : QWidget(parent), m_textEdit(new QTextEdit(this)),
          m_layout(new QVBoxLayout(this))
    {
        setWindowFlags(Qt::ToolTip); // Makes it a floating tooltip-like window
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setStyleSheet("QTextEdit { background-color: #ffffcc; border: 1px "
                      "solid black; }");

        m_layout->addWidget(m_textEdit);
    }

    void setText(const QString &text) noexcept
    {
        m_textEdit->setText(text);
    }

private:
    QTextEdit *m_textEdit;
    QVBoxLayout *m_layout;
};

class PopupAnnotation : public Annotation
{
public:
    PopupAnnotation(const QRectF &rect, const QString &text, int index,
                    const QColor &color, QGraphicsItem *parent = nullptr)
        : Annotation(index, color, parent), m_rect(rect), m_text(text),
          m_popupWidget(nullptr)
    {
        setAcceptHoverEvents(true);
    }

    QRectF boundingRect() const override
    {
        return m_rect;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override
    {
        Q_UNUSED(option);
        Q_UNUSED(widget);

        // Draw a simple speech-bubble or note-like icon
        painter->setPen(m_pen);
        painter->setBrush(m_brush);
        painter->drawRect(m_rect);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (!m_popupWidget)
        {
            QGraphicsView *view = scene()->views().value(0, nullptr);
            if (!view)
                return;

            m_popupWidget = new PopupWidget(view->viewport());
            m_popupWidget->setText(m_text);
        }

        if (m_popupWidget->isVisible())
        {
            m_popupWidget->setHidden(true);
        }
        else
        {
            QPoint screenPos = event->screenPos();
            m_popupWidget->move(screenPos + QPoint(10, 10));
            m_popupWidget->show();
        }

        QGraphicsItem::mousePressEvent(event);
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override
    {
        Q_UNUSED(event);
        m_pen = QPen(Qt::red);
        update();
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override
    {
        Q_UNUSED(event);
        m_pen = m_originalPen;
        update();
    }

private:
    QRectF m_rect;
    QString m_text;
    PopupWidget *m_popupWidget;
};
