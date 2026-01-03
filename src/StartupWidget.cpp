#include "StartupWidget.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QRegularExpression>
#include <QSortFilterProxyModel>

StartupWidget::StartupWidget(RecentFilesStore *store, QWidget *parent)
    : QWidget(parent), m_store(store)
{
    setAcceptDrops(false);
    QVBoxLayout *layout  = new QVBoxLayout(this);
    QLabel *recent_label = new QLabel("Recent Files");
    QLineEdit *line_edit = new QLineEdit(this);
    line_edit->setPlaceholderText("Search for file");
    m_table_view = new QTableView(this);

    QFont font = recent_label->font();

    font.setPixelSize(20);

    recent_label->setFont(font);

    layout->addWidget(recent_label);
    layout->addWidget(line_edit);
    layout->addWidget(m_table_view);
    // layout->addStretch(1);

    setProperty("tabRole", "startup");

    m_model = new RecentFilesModel(this);
    m_model->setDisplayHomePath(true);
    if (m_store)
        m_model->setEntries(m_store->entries());
    else
        m_model->setEntries({});
    m_table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QSortFilterProxyModel *proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(m_model);
    proxy->setFilterKeyColumn(0); // Filter by title
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxy->sort(RecentFilesModel::LastAccessed, Qt::DescendingOrder);

    proxy->setFilterRegularExpression(
        QRegularExpression(".*", QRegularExpression::CaseInsensitiveOption));

    connect(line_edit, &QLineEdit::textChanged, this,
            [proxy](const QString &text)
    {
        proxy->setFilterRegularExpression(
            text.isEmpty()
                ? QRegularExpression(".*",
                                     QRegularExpression::CaseInsensitiveOption)
                : QRegularExpression(
                      text, QRegularExpression::CaseInsensitiveOption));
    });

    m_table_view->setModel(proxy);
    m_table_view->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    m_table_view->setSelectionBehavior(
        QAbstractItemView::SelectionBehavior::SelectRows);

    m_table_view->horizontalHeader()->setVisible(false);

    m_table_view->setColumnHidden(1, true);
    m_table_view->setColumnHidden(2, true);

    connect(m_table_view, &QTableView::doubleClicked, this,
            [this, proxy](const QModelIndex &index)
    {
        if (!index.isValid())
            return;

        const QModelIndex sourceIndex = proxy->mapToSource(index);
        const RecentFileEntry entry   = m_model->entryAt(sourceIndex.row());
        const QString file_path       = entry.file_path;
        const int page_number         = entry.page_number;
        emit openFileRequested(file_path, page_number);
    });
}
