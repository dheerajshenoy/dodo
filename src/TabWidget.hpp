#pragma once

#include <QContextMenuEvent>
#include <QMenu>
#include <QTabBar>
#include <QTabWidget>

class TabWidget : public QTabWidget
{
    Q_OBJECT
public:
    TabWidget(QWidget *parent = nullptr) : QTabWidget(parent)
    {
        setElideMode(Qt::TextElideMode::ElideRight);
        setDocumentMode(true);
        setMovable(true);
        setTabsClosable(true);
        setAcceptDrops(false);
        setStyleSheet(R"(
QTabWidget::pane {
    border: 0;
    margin: 0;
}
        )");
    }

    int addTab(QWidget *page, const QString &title)
    {
        int result = QTabWidget::addTab(page, title);
        emit tabAdded(result);
        return result;
    }

    int insertTab(int index, QWidget *page, const QString &title)
    {
        int result = QTabWidget::insertTab(index, page, title);
        emit tabAdded(result);
        return result;
    }

signals:
    void tabAdded(int index);
    void openInExplorerRequested(int index);
    void filePropertiesRequested(int index);
};
