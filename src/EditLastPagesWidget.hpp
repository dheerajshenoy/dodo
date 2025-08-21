#pragma once

#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QTableView>
#include <QVBoxLayout>

class MySqlTableModel : public QSqlTableModel
{
public:
    using QSqlTableModel::QSqlTableModel; // inherit constructors

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        // First column (column 0) non-editable
        if (index.column() == 0)
        {
            return QSqlTableModel::flags(index) & ~Qt::ItemIsEditable;
        }
        return QSqlTableModel::flags(index);
    }
};

class EditLastPagesWidget : public QDialog
{
public:
    EditLastPagesWidget(const QSqlDatabase &db, QWidget *parent = nullptr);

private:
    void autoRemoveFiles() noexcept;
    void initConnections() noexcept;
    void revertChanges() noexcept;
    void deleteRows() noexcept;

    MySqlTableModel *m_model{nullptr};
    QTableView *m_tableView{new QTableView()};
    QSqlDatabase m_db;

    QPushButton *m_autoremove_btn;
    QPushButton *m_revert_changes_btn;
    QPushButton *m_delete_row_btn;
    QPushButton *m_apply_btn;
    QPushButton *m_close_btn;
};
