#pragma once

#include "LinkItem.hpp"

class BrowseLinkItem : public LinkItem {
public:
    BrowseLinkItem(const QRectF& rect, const QString& url) : LinkItem(rect, url)
    {}

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        QDesktopServices::openUrl(QUrl(link()));  // Open in browser
    }
};

