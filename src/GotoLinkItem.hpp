#pragma once

#include "LinkItem.hpp"
#include <poppler/qt6/poppler-link.h>

class GotoLinkItem : public QObject, public LinkItem {
    Q_OBJECT
public:
    GotoLinkItem(const QRectF& rect, const Poppler::LinkDestination &dest) : LinkItem(rect,
                                                                                      dest.toString()),
        m_dest(dest)
    {

    }


    signals:
    void jumpToPageRequested(int pageNumber, double top = 0.0);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        emit jumpToPageRequested(m_dest.pageNumber(), m_dest.top());
    }

private:
    Poppler::LinkDestination m_dest;
};

