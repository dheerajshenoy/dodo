#pragma once

#include "OutlineModel.hpp"

#include <QDialog>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

class OutlineWidget : public QWidget
{
    Q_OBJECT
public:
    OutlineWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        QVBoxLayout *layout = new QVBoxLayout(this);
        searchEdit->setPlaceholderText("Search TOC...");
        layout->addWidget(searchEdit);
        setContentsMargins(0, 0, 0, 0);

        m_view = new QTableView(this);
        m_view->setSelectionBehavior(
            QAbstractItemView::SelectionBehavior::SelectRows);
        m_view->setSelectionMode(QAbstractItemView::SingleSelection);

        m_view->verticalHeader()->setVisible(false);

        connect(m_view, &QTableView::doubleClicked, this,
                [&](const QModelIndex &index)
        {
            int pageno = index.siblingAtColumn(1).data().toInt() - 1;
            emit jumpToPageRequested(pageno);
        });

        layout->addWidget(m_view);

        auto *proxy = new QSortFilterProxyModel(this);
        proxy->setSourceModel(m_model);
        proxy->setFilterKeyColumn(-1); // Filter by title
        proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

        m_view->setEditTriggers(QTableView::EditTrigger::NoEditTriggers);
        m_view->setModel(proxy);
        m_view->setSelectionBehavior(QAbstractItemView::SelectRows);

        connect(searchEdit, &QLineEdit::textChanged, this,
                [proxy](const QString &text)
        {
            proxy->setFilterRegularExpression(QRegularExpression(
                text, QRegularExpression::CaseInsensitiveOption));
        });
    }

    ~OutlineWidget() {}

    void setOutline(fz_outline *outline)
    {
        if (outline)
        {
            m_model->loadFromOutline(outline);
            m_view->horizontalHeader()->setSectionResizeMode(
                0, QHeaderView::Stretch);
        }
        else
        {
            m_model->clear();
        }
    }

signals:
    void jumpToPageRequested(int pageno);

private:
    QTableView *m_view{nullptr};
    QLineEdit *searchEdit{new QLineEdit(this)};
    OutlineModel *m_model{new OutlineModel(this)};

protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape)
            e->ignore();
    }
};
