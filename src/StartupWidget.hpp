#pragma once

#include <QDateTime>
#include <QHeaderView>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>
#include <QSqlTableModel>

class StartupWidget : public QWidget
{
    Q_OBJECT
public:
    StartupWidget(const QSqlDatabase &db, QWidget *parent = nullptr);

signals:
    void openFileRequested(const QString &filename, int pageno);

private:
    QTableView *m_table_view{new QTableView()};
    QSqlDatabase m_db;
    QSqlTableModel *m_model{nullptr};
};
