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
    setObjectName("startupRoot");
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(16);

    QLineEdit *line_edit = new QLineEdit(this);
    line_edit->setObjectName("startupSearch");
    line_edit->setPlaceholderText("Search recent files...");
    line_edit->setClearButtonEnabled(true);

    m_table_view = new QTableView(this);
    m_table_view->setObjectName("recentTable");

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
    proxy->setSortRole(Qt::UserRole);
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
    m_table_view->horizontalHeader()->setSectionResizeMode(
        RecentFilesModel::LastAccessed, QHeaderView::ResizeToContents);
    m_table_view->setSelectionBehavior(
        QAbstractItemView::SelectionBehavior::SelectRows);
    m_table_view->setSelectionMode(
        QAbstractItemView::SelectionMode::SingleSelection);
    m_table_view->setSortingEnabled(true);
    m_table_view->setAlternatingRowColors(true);
    m_table_view->setShowGrid(false);
    m_table_view->verticalHeader()->setVisible(false);
    m_table_view->verticalHeader()->setDefaultSectionSize(32);

    m_table_view->horizontalHeader()->setVisible(true);

    connect(m_table_view, &QTableView::doubleClicked, this,
            [this, proxy](const QModelIndex &index)
    {
        if (!index.isValid())
            return;

        const QModelIndex sourceIndex = proxy->mapToSource(index);
        const RecentFileEntry entry   = m_model->entryAt(sourceIndex.row());
        const QString file_path       = entry.file_path;
        emit openFileRequested(file_path);
    });

    QWidget *empty_state      = new QWidget(this);
    QVBoxLayout *empty_layout = new QVBoxLayout(empty_state);
    empty_layout->setContentsMargins(16, 24, 16, 24);
    empty_layout->setSpacing(8);
    QLabel *empty_title = new QLabel("No recent files yet", empty_state);
    empty_title->setObjectName("emptyTitle");
    QLabel *empty_subtitle
        = new QLabel("Open a PDF to see it listed here.", empty_state);
    empty_subtitle->setObjectName("emptySubtitle");
    empty_subtitle->setWordWrap(true);
    empty_layout->addStretch(1);
    empty_layout->addWidget(empty_title, 0, Qt::AlignHCenter);
    empty_layout->addWidget(empty_subtitle, 0, Qt::AlignHCenter);
    empty_layout->addStretch(1);

    layout->addWidget(line_edit);
    layout->addWidget(m_table_view);

    setStyleSheet("QLineEdit#startupSearch {"
                  "  padding: 8px 10px;"
                  "  border-radius: 8px;"
                  "  border: 1px solid palette(midlight);"
                  "  background: palette(base);"
                  "}"
                  "QTableView#recentTable {"
                  "  background: palette(base);"
                  "  border: 1px solid palette(midlight);"
                  "  border-radius: 10px;"
                  "}"
                  "QTableView#recentTable::item {"
                  "  padding: 6px;"
                  "}"
                  "QHeaderView::section {"
                  "  background: palette(window);"
                  "  padding: 6px 8px;"
                  "  border: none;"
                  "  font-weight: 600;"
                  "}");
}
