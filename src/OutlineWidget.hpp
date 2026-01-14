#pragma once

#include "OutlineModel.hpp"

#include <QFocusEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>
#include <algorithm>

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
        setMinimumSize(500, 400);

        QVBoxLayout *layout = new QVBoxLayout(this);
        searchEdit->setPlaceholderText("Search Outline");
        searchEdit->setFocusPolicy(Qt::StrongFocus);
        layout->addWidget(searchEdit);
        setContentsMargins(0, 0, 0, 0);
        layout->setContentsMargins(0, 0, 0, 0);

        m_tree = new QTreeView(this);
        m_tree->setSelectionBehavior(
            QAbstractItemView::SelectionBehavior::SelectRows);
        m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
        m_tree->setRootIsDecorated(false);
        m_tree->setItemsExpandable(false);
        m_tree->setIndentation(0);
        m_tree->setUniformRowHeights(true);
        m_tree->setStyleSheet("QTreeView::item { padding: 4px 2px; }"
                              "QTreeView { outline: none; }");

        // m_tree->verticalHeader()->setVisible(false);

        connect(m_tree, &QTreeView::doubleClicked, this,
                [&](const QModelIndex &index)
        {
            int page               = index.siblingAtColumn(1).data().toInt();
            const QPointF location = index.siblingAtColumn(0)
                                         .data(OutlineModel::TargetLocationRole)
                                         .toPointF();
            emit jumpToLocationRequested(page, location);
        });

        layout->addWidget(m_tree);

        OutlineFilterProxy *proxy = new OutlineFilterProxy(this);
        proxy->setSourceModel(m_model);
        proxy->setFilterKeyColumn(-1); // Filter by title
        proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

        // m_tree->setEditTriggers(QTableView::EditTrigger::NoEditTriggers);
        m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);

        connect(searchEdit, &QLineEdit::textChanged, this,
                [this, proxy](const QString &text)
        {
            proxy->setFilterRegularExpression(QRegularExpression(
                text, QRegularExpression::CaseInsensitiveOption));
            selectFirstItem();
        });

        auto *activateShortcut
            = new QShortcut(QKeySequence(Qt::Key_Return), this);
        activateShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(activateShortcut, &QShortcut::activated, this,
                [this]() { activateCurrentSelection(); });

        auto *enterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), this);
        enterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(enterShortcut, &QShortcut::activated, this,
                [this]() { activateCurrentSelection(); });

        auto *nextShortcut
            = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_N), this);
        nextShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(nextShortcut, &QShortcut::activated, this,
                [this]() { moveSelection(1); });

        auto *prevShortcut
            = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_P), this);
        prevShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(prevShortcut, &QShortcut::activated, this,
                [this]() { moveSelection(-1); });

        // m_tree->setItemDelegateForColumn(1, new RightAlignDelegate(this));
        m_tree->setAnimated(true);
        m_tree->setModel(proxy);
        m_tree->setFrameShape(QFrame::NoFrame);

        QWidget::setTabOrder(searchEdit, m_tree);
    }

    bool isSidePanel() const noexcept
    {
        return m_is_side_panel;
    }

    void setOutline(fz_outline *outline) noexcept
    {
        if (outline)
        {
            m_has_outline = true;
            m_model->loadFromOutline(outline);
            m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
            m_tree->header()->setSectionResizeMode(
                1, QHeaderView::ResizeToContents);
            m_tree->header()->setSectionResizeMode(
                QHeaderView::ResizeToContents);
            // Store the location of target outline
            selectFirstItem();
        }
        else
            clearOutline();
    }

    inline bool hasOutline() const noexcept
    {
        return m_has_outline;
    }

    void clearOutline() noexcept
    {
        m_tree->selectionModel()->clearSelection();
        m_tree->setCurrentIndex(QModelIndex());
        m_has_outline = false;
        m_model->clear();
    }

    void selectFirstItem() noexcept
    {
        auto *proxy = qobject_cast<QSortFilterProxyModel *>(m_tree->model());
        if (!proxy || proxy->rowCount() == 0)
            return;

        const QModelIndex first = proxy->index(0, 0);
        if (!first.isValid())
            return;

        m_tree->setCurrentIndex(first);
        m_tree->selectionModel()->select(first,
                                         QItemSelectionModel::ClearAndSelect
                                             | QItemSelectionModel::Rows);
        m_tree->scrollTo(first);
    }

    void moveSelection(int delta) noexcept
    {
        auto *proxy = qobject_cast<QSortFilterProxyModel *>(m_tree->model());
        if (!proxy || proxy->rowCount() == 0)
            return;

        const QModelIndex current = m_tree->currentIndex();
        int row                   = current.isValid() ? current.row() : 0;
        row = std::clamp(row + delta, 0, proxy->rowCount() - 1);

        const QModelIndex next = proxy->index(row, 0);
        if (!next.isValid())
            return;

        m_tree->setCurrentIndex(next);
        m_tree->selectionModel()->select(next,
                                         QItemSelectionModel::ClearAndSelect
                                             | QItemSelectionModel::Rows);
        m_tree->scrollTo(next);
    }

    void activateCurrentSelection() noexcept
    {
        const QModelIndex index = m_tree->currentIndex();
        if (!index.isValid())
            return;
        const int page         = index.siblingAtColumn(1).data().toInt();
        const QPointF location = index.siblingAtColumn(0)
                                     .data(OutlineModel::TargetLocationRole)
                                     .toPointF();
        emit jumpToLocationRequested(page, location);
    }
signals:
    void jumpToLocationRequested(int pageno, const QPointF &pos);

private:
    QTreeView *m_tree{nullptr};
    QLineEdit *searchEdit{new QLineEdit(this)};
    OutlineModel *m_model{new OutlineModel(this)};
    bool m_is_side_panel{true};
    bool m_has_outline{false};

protected:
    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        searchEdit->setFocus();
        searchEdit->selectAll();
    }

    void keyReleaseEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape)
            e->ignore();
    }
};
