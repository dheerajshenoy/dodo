#pragma once

#include "OutlineModel.hpp"

#include <QDialog>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QVBoxLayout>

class RightAlignDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void initStyleOption(QStyleOptionViewItem *opt,
                         const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(opt, index);
        opt->displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
    }
};

class OutlineFilterProxy : public QSortFilterProxyModel
{
public:
    OutlineFilterProxy(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setRecursiveFilteringEnabled(true);
    }

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override
    {
        QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);

        // 1. If this row matches, accept
        if (sourceModel()->data(index0).toString().contains(
                filterRegularExpression()))
            return true;

        // 2. If any child matches, accept parent
        if (hasAcceptedChildren(index0))
            return true;

        // 3. If parent matches, accept child (optional but nice for outlines)
        if (parentMatches(sourceParent))
            return true;

        return false;
    }

private:
    bool hasAcceptedChildren(const QModelIndex &parent) const
    {
        int rows = sourceModel()->rowCount(parent);
        for (int r = 0; r < rows; ++r)
        {
            QModelIndex child = sourceModel()->index(r, 0, parent);
            if (filterAcceptsRow(r, parent))
                return true;

            if (hasAcceptedChildren(child))
                return true;
        }
        return false;
    }

    bool parentMatches(const QModelIndex &parent) const
    {
        QModelIndex p = parent;
        while (p.isValid())
        {
            if (sourceModel()->data(p).toString().contains(
                    filterRegularExpression()))
                return true;
            p = p.parent();
        }
        return false;
    }
};

class OutlineWidget : public QWidget
{
    Q_OBJECT
public:
    OutlineWidget(QWidget *parent = nullptr, bool is_side_panel = true) noexcept
        : QWidget(parent), m_is_side_panel(is_side_panel)
    {
        QVBoxLayout *layout = new QVBoxLayout(this);
        searchEdit->setPlaceholderText("Search Outline");
        searchEdit->setFocusPolicy(Qt::ClickFocus);
        layout->addWidget(searchEdit);
        setContentsMargins(0, 0, 0, 0);
        layout->setContentsMargins(0, 0, 0, 0);

        m_tree = new QTreeView(this);
        m_tree->setSelectionBehavior(
            QAbstractItemView::SelectionBehavior::SelectRows);
        m_tree->setSelectionMode(QAbstractItemView::SingleSelection);

        // m_tree->verticalHeader()->setVisible(false);

        connect(m_tree, &QTreeView::doubleClicked, this,
                [&](const QModelIndex &index)
        { emit jumpToPageRequested(index.siblingAtColumn(1).data().toInt()); });

        layout->addWidget(m_tree);

        OutlineFilterProxy *proxy = new OutlineFilterProxy(this);
        proxy->setSourceModel(m_model);
        proxy->setFilterKeyColumn(-1); // Filter by title
        proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

        // m_tree->setEditTriggers(QTableView::EditTrigger::NoEditTriggers);
        m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);

        connect(searchEdit, &QLineEdit::textChanged, this,
                [proxy](const QString &text)
        {
            proxy->setFilterRegularExpression(QRegularExpression(
                text, QRegularExpression::CaseInsensitiveOption));
        });

        // m_tree->setItemDelegateForColumn(1, new RightAlignDelegate(this));
        m_tree->setAnimated(true);
        m_tree->setModel(proxy);
        m_tree->setFrameShape(QFrame::NoFrame);
    }

    bool isSidePanel() const noexcept
    {
        return m_is_side_panel;
    }

    void setOutline(fz_outline *outline) noexcept
    {
        if (outline)
        {
            m_model->loadFromOutline(outline);
            m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
            m_tree->header()->setSectionResizeMode(
                1, QHeaderView::ResizeToContents);
            m_tree->header()->setSectionResizeMode(
                QHeaderView::ResizeToContents);
        }
        else
        {
            m_model->clear();
        }
    }

signals:
    void jumpToPageRequested(int pageno);

private:
    QTreeView *m_tree{nullptr};
    QLineEdit *searchEdit{new QLineEdit(this)};
    OutlineModel *m_model{new OutlineModel(this)};
    bool m_is_side_panel{true};

protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape)
            e->ignore();
    }
};
