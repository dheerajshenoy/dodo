#pragma once

#include <QApplication>
#include <QDrag>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QStyleOptionTab>
#include <QTabBar>

class DraggableTabBar : public QTabBar
{
    Q_OBJECT

public:
    static constexpr const char *MIME_TYPE = "application/x-dodo-tab";

    explicit DraggableTabBar(QWidget *parent = nullptr) : QTabBar(parent)
    {
        setAcceptDrops(true);
        setElideMode(Qt::TextElideMode::ElideRight);
        setDrawBase(false);
    }

    struct TabData
    {
        QString filePath;
        int currentPage{1};
        double zoom{1.0};
        bool invertColor{false};
        int rotation{0};
        int fitMode{0};

        QByteArray serialize() const
        {
            QJsonObject obj;
            obj["file_path"]    = filePath;
            obj["current_page"] = currentPage;
            obj["zoom"]         = zoom;
            obj["invert_color"] = invertColor;
            obj["rotation"]     = rotation;
            obj["fit_mode"]     = fitMode;
            return QJsonDocument(obj).toJson(QJsonDocument::Compact);
        }

        static TabData deserialize(const QByteArray &data)
        {
            TabData result;
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isObject())
                return result;

            QJsonObject obj     = doc.object();
            result.filePath     = obj["file_path"].toString();
            result.currentPage  = obj["current_page"].toInt(1);
            result.zoom         = obj["zoom"].toDouble(1.0);
            result.invertColor  = obj["invert_color"].toBool(false);
            result.rotation     = obj["rotation"].toInt(0);
            result.fitMode      = obj["fit_mode"].toInt(0);
            return result;
        }
    };

signals:
    void tabDataRequested(int index, TabData *outData);
    void tabDropReceived(const TabData &data);
    void tabDetached(int index, const QPoint &globalPos);
    void tabDetachedToNewWindow(int index, const TabData &data);

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            m_drag_start_pos = event->pos();
            m_drag_tab_index = tabAt(event->pos());
        }
        QTabBar::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton))
        {
            QTabBar::mouseMoveEvent(event);
            return;
        }

        if (m_drag_tab_index < 0)
        {
            QTabBar::mouseMoveEvent(event);
            return;
        }

        // Check if we've moved enough to start a drag
        if ((event->pos() - m_drag_start_pos).manhattanLength()
            < QApplication::startDragDistance())
        {
            QTabBar::mouseMoveEvent(event);
            return;
        }

        // Request tab data from the parent widget
        TabData tabData;
        emit tabDataRequested(m_drag_tab_index, &tabData);

        if (tabData.filePath.isEmpty())
        {
            QTabBar::mouseMoveEvent(event);
            return;
        }

        // Create drag object
        QDrag *drag     = new QDrag(this);
        QMimeData *mime = new QMimeData();
        mime->setData(MIME_TYPE, tabData.serialize());

        // Also set URL for dropping into file managers
        mime->setUrls({QUrl::fromLocalFile(tabData.filePath)});

        drag->setMimeData(mime);

        // Create a pixmap of the tab for visual feedback
        QRect tabRect = this->tabRect(m_drag_tab_index);
        QPixmap tabPixmap(tabRect.size());
        tabPixmap.fill(Qt::transparent);
        QPainter painter(&tabPixmap);
        painter.setOpacity(0.8);

        // Render the tab
        QStyleOptionTab opt;
        initStyleOption(&opt, m_drag_tab_index);
        opt.rect = QRect(QPoint(0, 0), tabRect.size());
        style()->drawControl(QStyle::CE_TabBarTab, &opt, &painter, this);
        painter.end();

        drag->setPixmap(tabPixmap);
        drag->setHotSpot(event->pos() - tabRect.topLeft());

        int draggedIndex      = m_drag_tab_index;
        m_drag_tab_index      = -1;
        Qt::DropAction result = drag->exec(Qt::MoveAction | Qt::CopyAction);

        // If the drag was accepted by another window, close this tab
        if (result == Qt::MoveAction)
        {
            emit tabDetached(draggedIndex, QCursor::pos());
        }
        else if (result == Qt::IgnoreAction)
        {
            // Tab was dropped outside any accepting window - detach to new window
            emit tabDetachedToNewWindow(draggedIndex, tabData);
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_drag_tab_index = -1;
        QTabBar::mouseReleaseEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasFormat(MIME_TYPE))
        {
            // Check if this drag originated from this tab bar
            if (event->source() == this)
            {
                event->ignore();
                return;
            }
            event->setDropAction(Qt::MoveAction);
            event->accept();
        }
        else if (event->mimeData()->hasUrls())
        {
            // Accept file drops
            for (const QUrl &url : event->mimeData()->urls())
            {
                if (url.isLocalFile()
                    && url.toLocalFile().endsWith(".pdf", Qt::CaseInsensitive))
                {
                    event->acceptProposedAction();
                    return;
                }
            }
        }
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->mimeData()->hasFormat(MIME_TYPE))
        {
            if (event->source() == this)
            {
                event->ignore();
                return;
            }
            event->setDropAction(Qt::MoveAction);
            event->accept();
        }
        else if (event->mimeData()->hasUrls())
        {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent *event) override
    {
        if (event->mimeData()->hasFormat(MIME_TYPE))
        {
            if (event->source() == this)
            {
                event->ignore();
                return;
            }

            QByteArray data = event->mimeData()->data(MIME_TYPE);
            TabData tabData = TabData::deserialize(data);

            if (!tabData.filePath.isEmpty())
            {
                event->setDropAction(Qt::MoveAction);
                event->accept();
                emit tabDropReceived(tabData);
            }
        }
    }

private:
    QPoint m_drag_start_pos;
    int m_drag_tab_index{-1};
};
