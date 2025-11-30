#pragma once

#include "Annotation.hpp"

#include <QGraphicsOpacityEffect>
#include <QGraphicsProxyWidget>
#include <QGraphicsView>
#include <QPainter>
#include <QTextEdit>
#include <QToolTip>
#include <QVBoxLayout>
#include <qgraphicssceneevent.h>

class PopupAnnotation : public Annotation
{
public:
    PopupAnnotation(const QRectF &rect, const QString &text, int index,
                    const QColor &color, QGraphicsItem *parent = nullptr)
        : Annotation(index, color, parent), m_rect(rect), m_text(text),
          m_popupProxy(nullptr), m_clicked(false)
    {
        setData(0, "annot");
    }

    ~PopupAnnotation() override = default; // proxy and widget are auto-deleted

    QRectF boundingRect() const override
    {
        return m_rect;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override
    {
        Q_UNUSED(option)
        Q_UNUSED(widget)
        painter->setPen(m_pen);
        painter->setBrush(m_brush);
        painter->drawRect(m_rect);
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override
    {
        if (!m_popupProxy)
            createPopup();

        // Position popup relative to annotation
        m_popupProxy->setPos(m_rect.topRight() + QPointF(12, 12));
        m_popupProxy->show();
        m_popupProxy->setFocus();

        m_pen = QPen(Qt::red);
        QGraphicsItem::hoverEnterEvent(event);
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override
    {
        if (m_clicked)
            return;

        if (m_popupProxy)
            m_popupProxy->hide();

        m_pen = m_originalPen;
        update();
        QGraphicsItem::hoverLeaveEvent(event);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (!m_popupProxy)
            createPopup();

        m_popupProxy->setPos(m_rect.topRight() + QPointF(12, 12));
        m_popupProxy->show();
        m_popupProxy->setFocus();

        m_pen     = QPen(Qt::red);
        m_clicked = !m_clicked;

        QGraphicsItem::mousePressEvent(event);
    }

private:
    void createPopup()
    {
        if (!scene())
            return;

        // Use QTextBrowser for selectable text
        QTextEdit *textWidget = new QTextEdit();
        textWidget->setPlainText(m_text);
        textWidget->setReadOnly(true);
        textWidget->setFrameStyle(QFrame::Box | QFrame::Plain);
        textWidget->setStyleSheet("border-radius: 8px; padding: 6px; ");
        textWidget->setTextInteractionFlags(Qt::TextSelectableByMouse
                                            | Qt::LinksAccessibleByMouse);
        textWidget->setContentsMargins(0, 0, 0, 0);

        // Wrap in proxy and parent to annotation
        m_popupProxy = scene()->addWidget(textWidget, Qt::FramelessWindowHint);
        m_popupProxy->setContentsMargins(0, 0, 0, 0);
        m_popupProxy->setParentItem(this);
        m_popupProxy->setZValue(9999);

        m_popupProxy->setFlag(QGraphicsItem::ItemIsFocusable);
        m_popupProxy->setAcceptedMouseButtons(Qt::AllButtons);
        m_popupProxy->hide();
    }

private:
    QRectF m_rect;
    QString m_text;
    QGraphicsProxyWidget *m_popupProxy;
    bool m_clicked;
};
