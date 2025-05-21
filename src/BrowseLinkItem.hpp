#pragma once

#include <QGraphicsRectItem>
#include <QDesktopServices>
#include <QCursor>
#include <QUrl>
#include <qgraphicsitem.h>
#include <qnamespace.h>
#include <QPen>
#include <QBrush>
#include <QGraphicsSceneHoverEvent>
#include <QApplication>
#include <QCursor>

class BrowseLinkItem : public QObject, public QGraphicsRectItem {
    Q_OBJECT
    public:

    struct Location {
        float x, y, zoom;
    };

    enum class LinkType
        {
            Internal_Page = 0,
            Internal_Section,
            External
        };

    BrowseLinkItem(const QRectF& rect,
                   const QString &link,
                   LinkType type) : QGraphicsRectItem(rect), _link(link), _type(type)
    {
        setPen(Qt::NoPen);
        setBrush(Qt::transparent);
        setAcceptedMouseButtons(Qt::MouseButton::LeftButton);
        setAcceptHoverEvents(true);
        setToolTip(link);
        // setCursor(Qt::PointingHandCursor);
        setFlag(QGraphicsItem::ItemIsSelectable);
    }

    inline void setGotoPageNo(int pageno) noexcept { _pageno = pageno; }
    inline int gotoPageNo() noexcept { return _pageno; }
    inline void setXYZ(const Location &loc) noexcept { _loc = loc; }
    inline Location XYZ() noexcept { return _loc; }

    signals:
    void jumpToPageRequested(int pageno);
    void jumpToLocationRequested(int pageno, const Location &loc);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        switch (_type)
        {
            case LinkType::Internal_Page:
                if (_pageno)
                    emit jumpToPageRequested(_pageno);
                else
                    qWarning() << "No page number found!";
                break;

            case LinkType::Internal_Section:
                    emit jumpToLocationRequested(_pageno, _loc);
                break;

            case LinkType::External:
                QDesktopServices::openUrl(QUrl(_link));
                break;
        }
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent *e) override {
        QApplication::setOverrideCursor(Qt::PointingHandCursor);
        setBrush(QBrush(QColor(1.0, 1.0, 0.0, 125)));
        QGraphicsRectItem::hoverEnterEvent(e);
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *e) override {
        QApplication::restoreOverrideCursor();
        setBrush(Qt::transparent);
        QGraphicsRectItem::hoverLeaveEvent(e);
    }

private:
    Location _loc;
    int _pageno;
    QString _link;
    LinkType _type;

};
