#include "StartupWidget.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QSortFilterProxyModel>

StartupWidget::StartupWidget(const QSqlDatabase &db, QWidget *parent) : QWidget(parent), m_db(db)
{
    QVBoxLayout *layout  = new QVBoxLayout(this);
    QLabel *recent_label = new QLabel("Recent Files");
    QLineEdit *line_edit = new QLineEdit(this);
    line_edit->setPlaceholderText("Search for file");

    QFont font = recent_label->font();

    font.setPixelSize(20);

    recent_label->setFont(font);

    layout->addWidget(recent_label);
    layout->addWidget(line_edit);
    layout->addWidget(m_table_view);

    setProperty("tabRole", "startup");

    QSqlQuery query(m_db);

    if (!query.exec("SELECT file_path, page_number, last_accessed FROM last_visited ORDER BY last_accessed DESC"))
    {
        qWarning() << "Query failed:" << query.lastError().text();
        return;
    }

    m_model = new HomePathModel(this, db);
    m_model->setTable("last_visited");
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_model->select();
    m_table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_model->setHeaderData(m_model->fieldIndex("file_path"), Qt::Horizontal, "File");
    // m_model->setHeaderData(m_model->fieldIndex("last_accessed"), Qt::Horizontal, tr("Last Visited"));

    // Optional: sort by last_accessed DESC
    m_model->setSort(m_model->fieldIndex("last_accessed"), Qt::DescendingOrder);

    QSortFilterProxyModel *proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(m_model);
    proxy->setFilterKeyColumn(0); // Filter by title
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    proxy->setFilterRegularExpression(QRegularExpression(".*", QRegularExpression::CaseInsensitiveOption));

    connect(line_edit, &QLineEdit::textChanged, this, [proxy](const QString &text)
    {
    proxy->setFilterRegularExpression(
            text.isEmpty() ? QRegularExpression(".*", QRegularExpression::CaseInsensitiveOption)
            : QRegularExpression(text, QRegularExpression::CaseInsensitiveOption)
            );
    });

    m_table_view->setModel(proxy);
    m_table_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table_view->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);

    m_table_view->horizontalHeader()->setVisible(false);

    m_table_view->setColumnHidden(1, true);
    m_table_view->setColumnHidden(2, true);

    connect(m_table_view, &QTableView::doubleClicked, this, [this](const QModelIndex &index)
    {
        if (!index.isValid())
            return;

        QString file_path = m_model->data(m_model->index(index.row(), m_model->fieldIndex("file_path"))).toString();
        int page_number   = m_model->data(m_model->index(index.row(), m_model->fieldIndex("page_number"))).toInt();
        emit openFileRequested(file_path, page_number);
    });
}
