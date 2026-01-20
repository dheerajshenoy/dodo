#include "HighlightSearchWidget.hpp"

#include <QHBoxLayout>
#include <QShortcut>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

HighlightSearchWidget::HighlightSearchWidget(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("Highlight Search");
    setMinimumSize(640, 420);

    m_spinner = new WaitingSpinnerWidget(this, false, false);
    m_spinner->setInnerRadius(5);
    m_spinner->setColor(palette().color(QPalette::Text));

    QLabel *title  = new QLabel("Highlights", this);
    m_filter_input = new QLineEdit(this);
    m_filter_input->setPlaceholderText("Filter highlights");
    m_filter_input->setFocusPolicy(Qt::StrongFocus);
    m_list = new QListWidget(this);
    m_list->setMinimumHeight(220);
    m_count_label    = new QLabel("0 results", this);
    m_refresh_button = new QPushButton("Refresh", this);
    // m_close_button   = new QPushButton("Close", this);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *header = new QHBoxLayout();
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_spinner);
    layout->addLayout(header);

    QHBoxLayout *filter_row = new QHBoxLayout();
    filter_row->addWidget(m_filter_input, 1);
    filter_row->addWidget(m_refresh_button);
    layout->addLayout(filter_row);

    layout->addWidget(m_list, 1);

    QHBoxLayout *footer = new QHBoxLayout();
    footer->addWidget(m_count_label);
    footer->addStretch();
    // footer->addWidget(m_close_button);
    layout->addLayout(footer);

    connect(m_filter_input, &QLineEdit::textChanged, this,
            [this]() { applyFilter(); });

    connect(m_filter_input, &QLineEdit::returnPressed, this,
            [this]() { activateCurrentSelection(); });

    connect(m_refresh_button, &QPushButton::clicked, this,
            [this]() { refresh(); });

    QWidget::setTabOrder(m_filter_input, m_list);
    QWidget::setTabOrder(m_list, m_refresh_button);
    // QWidget::setTabOrder(m_refresh_button, m_close_button);

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

    auto *activateShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    activateShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(activateShortcut, &QShortcut::activated, this,
            [this]() { activateCurrentSelection(); });

    auto *enterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), this);
    enterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(enterShortcut, &QShortcut::activated, this,
            [this]() { activateCurrentSelection(); });

    connect(m_list, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item)
    {
        if (!item)
            return;
        const int page    = item->data(Qt::UserRole).toInt();
        const QPointF pos = item->data(Qt::UserRole + 1).toPointF();
        emit gotoLocationRequested(page, pos);
    });

    connect(&m_watcher,
            &QFutureWatcher<std::vector<Model::HighlightText>>::finished, this,
            [this]()
    {
        m_entries = m_watcher.result();
        setLoading(false);
        applyFilter();
    });
}

void
HighlightSearchWidget::refresh() noexcept
{
    if (!m_model)
        return;

    if (m_watcher.isRunning())
        return;

    setLoading(true);
    QPointer<Model> model = m_model;
    QFuture<std::vector<Model::HighlightText>> future
        = QtConcurrent::run([model]()
    {
        if (!model)
            return std::vector<Model::HighlightText>{};
        return model->collectHighlightTexts(true);
    });
    m_watcher.setFuture(future);
}

void
HighlightSearchWidget::applyFilter() noexcept
{
    m_list->clear();

    const QString term = m_filter_input->text();
    bool caseSensitive = false;
    for (QChar c : term)
    {
        if (c.isUpper())
        {
            caseSensitive = true;
            break;
        }
    }
    const Qt::CaseSensitivity sensitivity
        = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

    int count = 0;
    for (const auto &entry : m_entries)
    {
        if (!term.isEmpty() && !entry.text.contains(term, sensitivity))
            continue;

        const QString label
            = QString("p%1: %2").arg(entry.page + 1).arg(entry.text);
        auto *item = new QListWidgetItem(label, m_list);
        item->setData(Qt::UserRole, entry.page);
        const double centerX = (entry.quad.ul.x + entry.quad.ur.x
                                + entry.quad.ll.x + entry.quad.lr.x)
                               * 0.25;
        const double centerY = (entry.quad.ul.y + entry.quad.ur.y
                                + entry.quad.ll.y + entry.quad.lr.y)
                               * 0.25;
        item->setData(Qt::UserRole + 1, QPointF(centerX, centerY));
        item->setToolTip(entry.text);
        ++count;
    }

    m_count_label->setText(QString("%1 results").arg(count));
    selectFirstItem();
}

void
HighlightSearchWidget::setLoading(bool state) noexcept
{
    if (state)
    {
        m_spinner->show();
        m_spinner->start();
        m_refresh_button->setEnabled(false);
    }
    else
    {
        m_spinner->hide();
        m_spinner->stop();
        m_refresh_button->setEnabled(true);
    }
}

void
HighlightSearchWidget::selectFirstItem() noexcept
{
    if (m_list->count() == 0)
        return;
    m_list->setCurrentRow(0);
    m_list->scrollToItem(m_list->currentItem());
}

void
HighlightSearchWidget::moveSelection(int delta) noexcept
{
    const int count = m_list->count();
    if (count == 0)
        return;

    int row = m_list->currentRow();
    if (row < 0)
        row = 0;

    row = std::clamp(row + delta, 0, count - 1);
    m_list->setCurrentRow(row);
    m_list->scrollToItem(m_list->currentItem());
}

void
HighlightSearchWidget::activateCurrentSelection() noexcept
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return;
    const int page    = item->data(Qt::UserRole).toInt();
    const QPointF pos = item->data(Qt::UserRole + 1).toPointF();
    emit gotoLocationRequested(page, pos);
}

void
HighlightSearchWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_filter_input->setFocus();
    m_filter_input->selectAll();
}

void
HighlightSearchWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        event->ignore();
}
