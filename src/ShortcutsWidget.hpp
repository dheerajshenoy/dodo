#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QHash>

class ShortcutsWidget : public QDialog
{

public:
    ShortcutsWidget(const QHash<QString, QString> &shortcutsMap, QWidget *parent = nullptr) : QDialog(parent)
    {
        auto *layout      = new QVBoxLayout(this);
        auto *tableWidget = new QTableWidget();

        tableWidget->setColumnCount(2);
        tableWidget->setRowCount(shortcutsMap.size());

        tableWidget->setHorizontalHeaderLabels({"Action", "Shortcut"});

        int i = 0;
        for (auto it = shortcutsMap.constBegin(); it != shortcutsMap.constEnd(); ++it, ++i)
        {
            tableWidget->setItem(i, 0, new QTableWidgetItem(it.key()));
            tableWidget->setItem(i, 1, new QTableWidgetItem(it.value()));
        }

        tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeMode::Stretch);
        tableWidget->verticalHeader()->setVisible(false);
        tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tableWidget->setSelectionMode(QAbstractItemView::NoSelection);

        layout->addWidget(tableWidget);
    }

private:
};
