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

        setStyleSheet(R"(
QTabWidget::pane {
    border: 0;
    margin: -13px -9px -13px -9px;
}
        )");
    }

signals:
    void openInExplorerRequested(int index);
    void filePropertiesRequested(int index);

protected:
};
