#include "EditLastPagesWidget.hpp"

#include <QFile>

EditLastPagesWidget::EditLastPagesWidget(const QSqlDatabase &db,
                                         QWidget *parent)
    : QDialog(parent), m_db(db)
{
    m_model = new QSqlTableModel(this, db);
    m_model->setTable("last_visited");
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_model->select();
    m_model->setHeaderData(0, Qt::Horizontal, tr("File Path"));
    m_model->setHeaderData(1, Qt::Horizontal, tr("Page"));
    m_model->setHeaderData(2, Qt::Horizontal, tr("Last Visited"));

    m_autoremove_btn     = new QPushButton("Remove unfound files");
    m_revert_changes_btn = new QPushButton("Revert Changes");
    m_delete_row_btn     = new QPushButton("Delete row");
    m_apply_btn          = new QPushButton("Apply");
    m_close_btn          = new QPushButton("Close");

    initConnections();

    m_tableView->setModel(m_model);
    m_tableView->horizontalHeader()->setSectionResizeMode(0,
                                                          QHeaderView::Stretch);
    m_tableView->setSelectionBehavior(
        QAbstractItemView::SelectionBehavior::SelectRows);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_tableView);

    QHBoxLayout *btnLayout = new QHBoxLayout();

    btnLayout->addStretch(1);
    btnLayout->addWidget(m_autoremove_btn);
    btnLayout->addWidget(m_revert_changes_btn);
    btnLayout->addWidget(m_delete_row_btn);
    btnLayout->addWidget(m_apply_btn);
    btnLayout->addWidget(m_close_btn);
    layout->addLayout(btnLayout);
}

void
EditLastPagesWidget::initConnections() noexcept
{

    connect(m_autoremove_btn, &QPushButton::clicked, this,
            &EditLastPagesWidget::autoRemoveFiles);

    connect(m_revert_changes_btn, &QPushButton::clicked, this,
            &EditLastPagesWidget::revertChanges);

    connect(m_delete_row_btn, &QPushButton::clicked, this,
            &EditLastPagesWidget::deleteRows);

    connect(m_apply_btn, &QPushButton::clicked, this, [&]()
    {
        auto confirm = QMessageBox::question(
            this, "Apply Changes", "Do you want to apply the changes ?");
        if (confirm == QMessageBox::Yes)
            m_model->submitAll();
    });

    connect(m_close_btn, &QPushButton::clicked, this,
            &EditLastPagesWidget::close);
}

void
EditLastPagesWidget::autoRemoveFiles() noexcept
{
    int nrows = m_model->rowCount();
    QList<int> removableFileIndexes;

    if (nrows == 0)
        return;

    for (int i = 0; i < nrows; i++)
    {
        QModelIndex index = m_model->index(i, 0);
        QString filePath  = m_model->data(index).toString();
        if (!QFile::exists(filePath))
            removableFileIndexes.append(i);
    }

    // Remove rows in reverse order to avoid index shifting
    std::sort(removableFileIndexes.begin(), removableFileIndexes.end(),
              std::greater<int>());
    for (int row : removableFileIndexes)
    {
        m_model->removeRow(row);
    }
    // Remove removableFileIndexes
}

void
EditLastPagesWidget::revertChanges() noexcept
{
    if (!m_model->isDirty())
    {
        QMessageBox::information(this, "Revert Changes",
                                 "There are no changes to revert to");
        return;
    }
    auto confirm = QMessageBox::question(
        this, "Revert Changes", "Do you really want to revert the changes ?");
    if (confirm == QMessageBox::Yes)
        m_model->revertAll();
}

void
EditLastPagesWidget::deleteRows() noexcept
{

    if (!m_tableView->selectionModel())
        return;

    auto rows = m_tableView->selectionModel()->selectedRows();

    std::sort(rows.begin(), rows.end(),
              [](const QModelIndex &a, const QModelIndex &b)
    { return a.row() > b.row(); });

    for (const auto &index : rows)
    {
        m_model->removeRow(index.row());
    }
}
