#pragma once

#include <QGraphicsScene>
#include <qevent.h>
#include <qgraphicssceneevent.h>

class GraphicsScene : public QGraphicsScene
{
    Q_OBJECT
public:
    GraphicsScene(QObject *parent = nullptr) : QGraphicsScene(parent) {}

};
