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
    EditLastPagesWidget(const QSqlDatabase& db, QWidget* parent = nullptr)
        : QDialog(parent), m_db(db)
    {
        m_model = new QSqlTableModel(this, db);
        m_model->setTable("last_visited");
        m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
        m_model->select();
        m_model->setHeaderData(0, Qt::Horizontal, tr("File Path"));
        m_model->setHeaderData(1, Qt::Horizontal, tr("Page"));
        m_model->setHeaderData(2, Qt::Horizontal, tr("Last Visited"));

        m_tableView->setModel(m_model);
        m_tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_tableView->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->addWidget(m_tableView);

        QHBoxLayout* btnLayout = new QHBoxLayout();

        QPushButton* revert_changes_btn = new QPushButton("Revert Changes");
        QPushButton* delete_row_btn     = new QPushButton("Delete row");
        QPushButton* apply_btn          = new QPushButton("Apply");
        QPushButton* close_btn          = new QPushButton("Close");

        connect(revert_changes_btn, &QPushButton::clicked, this, [&]()
        {
            if (!m_model->isDirty())
            {
                QMessageBox::information(this, "Revert Changes",
                                         "There are no changes to revert to");
                return;
            }
            auto confirm = QMessageBox::question(this, "Revert Changes",
                                                 "Do you really want to revert the changes ?");
            if (confirm == QMessageBox::Yes)
                m_model->revertAll();
        });

        connect(delete_row_btn, &QPushButton::clicked, this, [&]()
        {
            if (!m_tableView->selectionModel())
                return;

            auto rows = m_tableView->selectionModel()->selectedRows();

            std::sort(rows.begin(), rows.end(),
                      [](const QModelIndex& a, const QModelIndex& b) { return a.row() > b.row(); });

            for (const auto& index : rows)
            {
                m_model->removeRow(index.row());
            }
        });

        connect(apply_btn, &QPushButton::clicked, this, [&]()
        {
            auto confirm =
                QMessageBox::question(this, "Apply Changes", "Do you want to apply the changes ?");
            if (confirm == QMessageBox::Yes)
                m_model->submitAll();
        });
        connect(close_btn, &QPushButton::clicked, this, &EditLastPagesWidget::close);

        btnLayout->addStretch(1);
        btnLayout->addWidget(revert_changes_btn);
        btnLayout->addWidget(delete_row_btn);
        btnLayout->addWidget(apply_btn);
        btnLayout->addWidget(close_btn);
        layout->addLayout(btnLayout);
    }

private:
    QSqlTableModel* m_model{nullptr};
    QTableView* m_tableView{new QTableView()};
    QSqlDatabase m_db;
};
