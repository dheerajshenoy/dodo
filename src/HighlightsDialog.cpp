#include "HighlightsDialog.hpp"

#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

HighlightsDialog::HighlightsDialog(const QList<DocumentView::TextHighlight> &highlights, QWidget *parent)
    : QDialog(parent), searchBar(new QLineEdit(this)), tableView(new QTableView(this)),
      model(new QStandardItemModel(this)), proxy(new QSortFilterProxyModel(this))
{
    setWindowTitle("Highlights");
    resize(600, 400);

    // Fill model
    model->setColumnCount(2);
    model->setHorizontalHeaderLabels({"Text", "Page Number"});
    for (const DocumentView::TextHighlight &highlight : highlights)
    {
        QList<QStandardItem *> rowItems;
        rowItems << new QStandardItem(highlight.content) << new QStandardItem(QString::number(highlight.pageno + 1));
        model->appendRow(rowItems);
    }

    // Setup proxy model
    proxy->setSourceModel(model);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(0);

    setupUI();
    connectSignals();
}

void
HighlightsDialog::setupUI()
{
    searchBar->setPlaceholderText("Search...");

    tableView->setModel(proxy);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QHeaderView *hHeader = tableView->horizontalHeader();
    hHeader->setSectionResizeMode(0, QHeaderView::Stretch); // Column 1 stretches
    tableView->verticalHeader()->hide();

    auto layout = new QVBoxLayout(this);
    layout->addWidget(searchBar);
    layout->addWidget(tableView);
    setLayout(layout);
}

void
HighlightsDialog::connectSignals()
{
    connect(searchBar, &QLineEdit::textChanged, proxy, &QSortFilterProxyModel::setFilterFixedString);

    connect(tableView, &QTableView::doubleClicked, this, [this](const QModelIndex &index)
    {
        if (index.isValid())
        {
            bool ok    = false;
            int pageno = model->data(model->index(index.row(), 1)).toInt(&ok) - 1;
            if (ok)
                emit highlightActivated(pageno);
            accept();
        }
    });

    tableView->installEventFilter(this);
}

bool
HighlightsDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == tableView && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
        {
            QModelIndex index = tableView->currentIndex();
            if (index.isValid())
            {

                bool ok    = false;
                int pageno = model->data(model->index(index.row(), 1)).toInt(&ok) - 1;
                if (ok)
                    emit highlightActivated(pageno);
                accept();
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, event);
}
