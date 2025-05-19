#pragma once

#include <QGraphicsRectItem>
#include <QDesktopServices>
#include <QUrl>
#include <QBrush>
#include <QPen>
#include <QCursor>

class LinkItem : public QGraphicsRectItem {
public:
    LinkItem(const QRectF& rect, const QString& url) : QGraphicsRectItem(rect), m_url(url) {
        // setBrush(QColor(255, 0, 0, 50));
        setBrush(Qt::transparent);
        setPen(QPen(Qt::red, 1));
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::PointingHandCursor);
    }

    inline QString Url() { return m_url; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        QDesktopServices::openUrl(QUrl(m_url));  // Open in browser
    }

private:
    QString m_url;
};

