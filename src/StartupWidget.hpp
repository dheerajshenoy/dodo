#pragma once

#include <QDateTime>
#include <QHeaderView>
#include <QMetaType>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlTableModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

class HomePathModel : public QSqlTableModel
{
public:
    using QSqlTableModel::QSqlTableModel; // inherit constructors

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override
    {
        QVariant original = QSqlTableModel::data(index, role);

        if (role == Qt::DisplayRole
            && original.typeId() == QMetaType::Type::QString)
        {
            QString str = original.toString();
            if (str.startsWith(homePath))
            {
                str.replace(homePath, "~");
                return str;
            }
        }

        return original;
    }

private:
    QString homePath{QString(getenv("HOME"))};
};

class StartupWidget : public QWidget
{
    Q_OBJECT
public:
    StartupWidget(const QSqlDatabase &db, QWidget *parent = nullptr);

signals:
    void openFileRequested(const QString &filename, int pageno);

private:
    QTableView *m_table_view{nullptr};
    QSqlDatabase m_db;
    HomePathModel *m_model{nullptr};
};
