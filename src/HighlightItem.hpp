#pragma once

#include <QBrush>
#include <QGraphicsRectItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QObject>
#include <QPen>
#include <qgraphicsitem.h>
#include <qnamespace.h>

class HighlightItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
public:
    HighlightItem(const QRectF &rect, int index, QGraphicsItem *parent = nullptr)
        : QGraphicsRectItem(rect, parent), m_index(index)
    {
        setPen(Qt::NoPen);
        setBrush(Qt::transparent);
        setAcceptHoverEvents(true);
    }

    void select(const QColor &color) noexcept
    {
        m_originalBrush = brush();
        m_originalPen   = pen();

        // setBrush(color);
        setPen(QPen(color, 1, Qt::PenStyle::SolidLine));
    }

    void restoreBrushPen() noexcept
    {
        setBrush(m_originalBrush);
        setPen(m_originalPen);
    }

signals:
    void annotDeleteRequested(int index);
    void annotColorChangeRequested(int index);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override
    {
        QGraphicsRectItem::mousePressEvent(e);
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

        connect(deleteAction, &QAction::triggered, this, [this]() { emit annotDeleteRequested(m_index); });

        connect(changeColorAction, &QAction::triggered, this,
                [this]() { emit annotColorChangeRequested(m_index); });

        menu.addAction(deleteAction);
        menu.addAction(changeColorAction);
        menu.exec(e->screenPos());
        e->accept();
    }

private:
    int m_index{-1};
    QBrush m_originalBrush;
    QPen m_originalPen;
};
