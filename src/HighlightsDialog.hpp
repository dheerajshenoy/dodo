#pragma once

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QTableView;
class QStandardItemModel;
class QSortFilterProxyModel;

#include "DocumentView.hpp"

class HighlightsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HighlightsDialog(const QList<DocumentView::TextHighlight> &highlights, QWidget *parent = nullptr);

signals:
    void highlightActivated(int index); // Emitted when a highlight is selected

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    QLineEdit *searchBar;
    QTableView *tableView;
    QStandardItemModel *model;
    QSortFilterProxyModel *proxy;

    void setupUI();
    void connectSignals();
};
