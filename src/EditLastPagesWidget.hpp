#pragma once

#include <QDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QTableView>
#include <QVBoxLayout>
#include <qboxlayout.h>
#include <qt6/QtWidgets/qpushbutton.h>

class EditLastPagesWidget : public QDialog
{
public:
    EditLastPagesWidget(const QSqlDatabase &db, QWidget *parent = nullptr);

private:
    void autoRemoveFiles() noexcept;
    void initConnections() noexcept;
    void revertChanges() noexcept;
    void deleteRows() noexcept;

    QSqlTableModel *m_model{nullptr};
    QTableView *m_tableView{new QTableView()};
    QSqlDatabase m_db;

    QPushButton *m_autoremove_btn;
    QPushButton *m_revert_changes_btn;
    QPushButton *m_delete_row_btn;
    QPushButton *m_apply_btn;
    QPushButton *m_close_btn;
};
