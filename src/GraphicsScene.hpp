#pragma once

#include "BrowseLinkItem.hpp"
#include "HighlightItem.hpp"

#include <QGraphicsScene>
#include <QGraphicsView>

class GraphicsScene : public QGraphicsScene
{
    Q_OBJECT
public:
    GraphicsScene(QObject *parent = nullptr) : QGraphicsScene(parent) {}

signals:
    void populateContextMenuRequested(QMenu *menu, const QPointF &pos);

protected:
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *e) override
    {
        // Check if item under cursor handled the event
        QGraphicsItem *item = itemAt(e->pos(), views().first()->transform());

        // Optional: Skip if it's a link item
        if (item && (dynamic_cast<BrowseLinkItem *>(item) || dynamic_cast<HighlightItem *>(item)))
        {
            qDebug() << "EE";
            e->ignore();
            QGraphicsScene::contextMenuEvent(e);
        }
        else
        {
            QMenu menu;
            emit populateContextMenuRequested(&menu, e->pos());
            if (!menu.isEmpty())
                menu.exec(e->screenPos());
            e->accept();
        }
    }
};
