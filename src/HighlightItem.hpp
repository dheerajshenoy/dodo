#pragma once

#include "Annotation.hpp"

#include <QBrush>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QObject>
#include <QPainter>
#include <QPen>

class HighlightAnnotation : public Annotation
{
    Q_OBJECT
public:
    HighlightAnnotation(const QRectF &rect, int index, const QColor &color,
                        QGraphicsItem *parent = nullptr)
        : Annotation(index, color, parent), m_rect(rect)
    {
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton)
        {
            setSelected(true);
            setFocus();

            QGraphicsItem::mousePressEvent(e);
        }
    }

    QRectF boundingRect() const override
    {
        return m_rect;
    }

    // void hoverEnterEvent(QGraphicsSceneHoverEvent *e) override {
    //     m_originalBrush = brush();
    //     m_originalPen = pen();
    //     QGraphicsRectItem::hoverEnterEvent(e);
    //     setPen(QPen(Qt::red, 2, Qt::SolidLine));
    // }
    //
    // void hoverLeaveEvent(QGraphicsSceneHoverEvent *e) override {
    //     QGraphicsRectItem::hoverLeaveEvent(e);
    //     restoreBrushPen();
    //     QGuiApplication::restoreOverrideCursor();
    // }

    void contextMenuEvent(QGraphicsSceneContextMenuEvent *e) override
    {
        QMenu menu;

        QAction *deleteAction      = new QAction("Delete");
        QAction *changeColorAction = new QAction("Change Color");

        connect(deleteAction, &QAction::triggered, this,
                [this]() { emit annotDeleteRequested(m_index); });
        connect(changeColorAction, &QAction::triggered, this,
                [this]() { emit annotColorChangeRequested(m_index); });

        menu.addAction(deleteAction);
        menu.addAction(changeColorAction);
        menu.exec(e->screenPos());
        e->accept();
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override
    {
        Q_UNUSED(option);
        Q_UNUSED(widget);
        Q_UNUSED(painter);
    }

private:
    QRectF m_rect;
};
