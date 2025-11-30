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

class PopupWidget : public QWidget
{
public:
    PopupWidget(QWidget *parent = nullptr)
        : QWidget(parent), m_textEdit(new QTextEdit(this)),
          m_layout(new QVBoxLayout(this))
    {
        // Style
        setAttribute(Qt::WA_ShowWithoutActivating);
        setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        setStyleSheet(R"(
            QWidget {
                background-color: #fefefe;
                border: 1px solid #c5c5c5;
                border-radius: 8px;
            }
            QTextEdit {
                background: transparent;
                border: none;
                padding: 6px;
                font-size: 13px;
            }
        )");

        m_textEdit->setReadOnly(true); // makes it clean
        m_textEdit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

        m_layout->setContentsMargins(0, 0, 0, 0);
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
          m_popupProxy(nullptr), m_clicked(false)
    {
        setAcceptHoverEvents(true);
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
        textWidget->setStyleSheet("background-color: #fefefe; "
                                  "border-radius: 8px; padding: 6px; ");
        textWidget->setTextInteractionFlags(Qt::TextSelectableByMouse
                                            | Qt::LinksAccessibleByMouse);

        // Wrap in proxy and parent to annotation
        m_popupProxy = scene()->addWidget(textWidget);
        m_popupProxy->setParentItem(this);
        m_popupProxy->setZValue(9999);
        m_popupProxy->setFlag(QGraphicsItem::ItemIsFocusable);
        m_popupProxy->hide();
    }

private:
    QRectF m_rect;
    QString m_text;
    QGraphicsProxyWidget *m_popupProxy;
    bool m_clicked;
};
