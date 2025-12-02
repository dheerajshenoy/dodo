#pragma once

// This class represents a clickable link area in the document view

#include <QApplication>
#include <QBrush>
#include <QCursor>
#include <QDesktopServices>
#include <QGraphicsRectItem>
#include <QGraphicsSceneHoverEvent>
#include <QMenu>
#include <QPen>
#include <QUrl>
#include <qbytearrayview.h>
#include <qevent.h>
#include <qgraphicsitem.h>
#include <qnamespace.h>

class BrowseLinkItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
public:
    struct Location
    {
        float x, y, zoom;
    };

    enum class LinkType
    {
        Page = 0,
        Section,
        FitV,
        FitH,
        External
    };

    BrowseLinkItem(const QRectF &rect, const QLatin1StringView &link, LinkType type, bool boundary = false,
                   QGraphicsItem *parent = nullptr)
        : QGraphicsRectItem(rect, parent), _link(link), _type(type)
    {
        if (!boundary)
            setPen(Qt::NoPen);
        // setBrush(Qt::transparent);
        setZValue(2);
        setAcceptHoverEvents(true);
        setToolTip(link);
        setCursor(Qt::PointingHandCursor);
        setAcceptedMouseButtons(Qt::AllButtons);
        setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
    }

    inline void setGotoPageNo(int pageno) noexcept
    {
        _pageno = pageno;
    }
    inline int gotoPageNo() noexcept
    {
        return _pageno;
    }
    inline void setXYZ(const Location &loc) noexcept
    {
        _loc = loc;
    }
    inline Location XYZ() noexcept
    {
        return _loc;
    }

    inline void setURI(char *uri) noexcept
    {
        _uri = uri;
    }

    inline const char* URI() noexcept { return _uri; }

signals:
    void jumpToPageRequested(int pageno);
    void jumpToLocationRequested(int pageno, const Location &loc);
    void verticalFitRequested(int pageno, const Location &loc);
    void horizontalFitRequested(int pageno, const Location &loc);
    void linkCopyRequested(const QString &link);

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton)
        {
            switch (_type)
            {
                case LinkType::Page:
                    if (_pageno)
                        emit jumpToPageRequested(_pageno);
                    break;

                case LinkType::Section:
                    emit jumpToLocationRequested(_pageno, _loc);
                    break;

                case LinkType::FitV:
                    emit verticalFitRequested(_pageno, _loc);
                    break;

                case LinkType::FitH:
                    emit horizontalFitRequested(_pageno, _loc);
                    break;

                case LinkType::External:
                    QDesktopServices::openUrl(QUrl(_link));
                    break;
            }
            setBrush(Qt::transparent);
        }

        QGraphicsRectItem::mouseReleaseEvent(e);
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent *e) override
    {
        setBrush(QBrush(QColor(1.0, 1.0, 0.0, 125)));
        QGraphicsRectItem::hoverEnterEvent(e);
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *e) override
    {
        setBrush(Qt::transparent);
        QGraphicsRectItem::hoverLeaveEvent(e);
    }

    void contextMenuEvent(QGraphicsSceneContextMenuEvent *e) override
    {
        QMenu menu;
        QAction *copyLinkLocationAction = menu.addAction("Copy Link Location");

        connect(copyLinkLocationAction, &QAction::triggered, this, [this]() { emit linkCopyRequested(_link); });

        menu.exec(e->screenPos());
        e->accept();
    }

private:
    Location _loc{0, 0, 0};
    int _pageno{-1};
    QString _link;
    LinkType _type;
    char *_uri{nullptr};
};
