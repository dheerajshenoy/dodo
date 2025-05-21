#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QTableView>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QKeyEvent>
#include "OutlineModel.hpp"

class OutlineWidget : public QDialog {
    Q_OBJECT
    public:
    OutlineWidget(fz_context *ctx, fz_document *doc, QWidget *parent = nullptr)
    : QDialog(parent) {
        auto *layout = new QVBoxLayout(this);
        searchEdit->setPlaceholderText("Search TOC...");
        layout->addWidget(searchEdit);

        auto *view = new QTableView(this);
        view->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        view->setSelectionMode(QAbstractItemView::SingleSelection);

        connect(view, &QTableView::doubleClicked, this, [&](const QModelIndex &index) {
            int pageno = index.siblingAtColumn(1).data().toInt() - 1;
            emit jumpToPageRequested(pageno);
        });
        layout->addWidget(view);

        auto *model = new OutlineModel(this);
        fz_outline *outline = fz_load_outline(ctx, doc);
        if (outline)
            model->loadFromOutline(ctx, outline);

        auto *proxy = new QSortFilterProxyModel(this);
        proxy->setSourceModel(model);
        proxy->setFilterKeyColumn(-1); // Filter by title
        proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);


        view->setEditTriggers(QTableView::EditTrigger::NoEditTriggers);
        view->setModel(proxy);
        view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);

        connect(searchEdit, &QLineEdit::textChanged, this, [proxy](const QString &text) {
            proxy->setFilterRegularExpression(QRegularExpression(text,
                                                                 QRegularExpression::CaseInsensitiveOption));
        });

    }

    void open() override {
        qDebug() << "DD";
        searchEdit->clear();
        QDialog::open();
    }

signals:
    void jumpToPageRequested(int pageno);

private:
        QLineEdit *searchEdit = new QLineEdit(this);

protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Escape)
            e->ignore();
    }

};

