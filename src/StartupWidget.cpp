#include "StartupWidget.hpp"

StartupWidget::StartupWidget(const QSqlDatabase &db, QWidget *parent) : QWidget(parent), m_db(db)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_table_view);

    setProperty("tabRole", "startup");

    QSqlQuery query(m_db);

    if (!query.exec("SELECT file_path, page_number, last_accessed FROM last_visited ORDER BY last_accessed DESC"))
    {
        qWarning() << "Query failed:" << query.lastError().text();
        return;
    }

    m_model = new QSqlTableModel(this, db);
    m_model->setTable("last_visited");
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_model->select();
    m_table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_model->setHeaderData(m_model->fieldIndex("file_path"), Qt::Horizontal, tr("File Path"));
    m_model->setHeaderData(m_model->fieldIndex("last_accessed"), Qt::Horizontal, tr("Last Visited"));

    // Optional: sort by last_accessed DESC
    m_model->setSort(m_model->fieldIndex("last_accessed"), Qt::DescendingOrder);

    m_table_view->setModel(m_model);
    m_table_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table_view->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table_view->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);

    m_table_view->setColumnHidden(1, true);

    connect(m_table_view, &QTableView::doubleClicked, this, [this](const QModelIndex &index)
    {
        if (!index.isValid())
            return;

        QString file_path = m_model->data(m_model->index(index.row(), m_model->fieldIndex("file_path"))).toString();
        int page_number   = m_model->data(m_model->index(index.row(), m_model->fieldIndex("page_number"))).toInt();
        emit openFileRequested(file_path, page_number);
    });
}
