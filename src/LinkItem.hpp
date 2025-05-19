#pragma once

#include <QGraphicsRectItem>
#include <QDesktopServices>
#include <QUrl>
#include <QBrush>
#include <QPen>
#include <QCursor>

class LinkItem : public QGraphicsRectItem {
public:
    LinkItem(const QRectF& rect, const QString& link) :
        QGraphicsRectItem(rect), m_link(link) {
        setPen(Qt::NoPen);
        setBrush(Qt::transparent);
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::PointingHandCursor);
        setToolTip(link);
    }

    inline QString link() { return m_link; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent*) = 0;

private:
    QString m_link;
};

