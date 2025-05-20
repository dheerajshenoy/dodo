#pragma once

#include "LinkItem.hpp"

class BrowseLinkItem : public LinkItem {
public:
    enum class LinkType
    {
        Internal,
        External
    };

    BrowseLinkItem(const QRectF& rect,
                   const QString& url,
                   LinkType type) : LinkItem(rect, url), _type(type)
    {}

    BrowseLinkItem(const QRectF& rect,
                   char* &url,
                   LinkType type) : LinkItem(rect, url), _type(type)
    {}

    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        if (_type == LinkType::External)
            QDesktopServices::openUrl(QUrl(link()));  // Open in browser
        else {
            // TODO: Handle internal link navigation
            qDebug() << "Not yet implemented: Internal link navigation";
        }
    }

private:
    LinkType _type;

};

