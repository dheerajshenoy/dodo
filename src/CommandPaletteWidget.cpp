#include "CommandPaletteWidget.hpp"

#include <QEvent>
#include <QHeaderView>
#include <QKeySequence>
#include <QShortcut>
#include <QVBoxLayout>
#include <sys/select.h>

// ---- CommandPaletteWidget Implementation ----

CommandPaletteWidget::CommandPaletteWidget(
    const QHash<QString, QString> &commandHash, QWidget *parent) noexcept
    : QWidget(parent)
{
    m_command_table = new QTableView(this);
    m_command_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_command_model = new CommandModel(commandHash, this);
    m_proxy_model   = new QSortFilterProxyModel(this);
    m_proxy_model->setSourceModel(m_command_model);
    m_command_table->setModel(m_proxy_model);
    m_command_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_command_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_command_table->horizontalHeader()->setVisible(false);
    m_command_table->verticalHeader()->setVisible(false);

    m_command_table->horizontalHeader()->setStretchLastSection(true);
    this->setMinimumWidth(400);

    m_input_line = new QLineEdit(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);
    layout->addWidget(m_input_line);
    layout->addWidget(m_command_table);
    this->setLayout(layout);

    initConnections();
    selectFirstItem();
}

void
CommandPaletteWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_input_line->clear();
    m_input_line->setFocus();
}

void
CommandPaletteWidget::selectFirstItem() noexcept
{
    if (!m_proxy_model || m_proxy_model->rowCount() == 0)
        return;

    const QModelIndex first = m_proxy_model->index(0, 0);
    if (!first.isValid())
        return;

    m_command_table->setCurrentIndex(first);
    m_command_table->selectionModel()->select(
        first, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_command_table->scrollTo(first);
}

void
CommandPaletteWidget::initConnections() noexcept
{
    connect(m_input_line, &QLineEdit::textChanged, this,
            [this](const QString &text)
    {
        m_proxy_model->setFilterFixedString(text);
        selectFirstItem();
    });

    connect(m_input_line, &QLineEdit::returnPressed, this, [this]()
    {
        const QModelIndex current = m_command_table->currentIndex();
        if (!current.isValid())
            return;
        const QString commandName
            = current.siblingAtColumn(0).data().toString();

        // Get args by splitting command name by spaces, and assume args start
        // from 2nd word
        QStringList args = commandName.split(' ');
        if (args.size() > 1)
            args = args.mid(1);
        else
            args.clear();
        emit commandSelected(commandName, args);
    });

    auto *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escShortcut->setContext(Qt::WidgetWithChildrenShortcut);
}

// ---- CommandModel Implementation ----

CommandModel::CommandModel(const QHash<QString, QString> &commands,
                           QObject *parent) noexcept
    : QAbstractTableModel(parent)
{
    for (auto it = commands.constBegin(); it != commands.constEnd(); ++it)
    {
        m_commands.push_back(
            {it.key(),
             it.value()}); // Description and shortcut can be filled later
    }
}

int
CommandModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return static_cast<int>(m_commands.size());
}

int
CommandModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2; // Name, Shortcut
}

QVariant
CommandModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    if (index.row() < 0 || index.row() >= static_cast<int>(m_commands.size()))
        return QVariant();
    const CommandEntry &entry = m_commands[index.row()];
    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case 0:
                return entry.name;
            case 1:
                return entry.shortcut;
                // case 2:
                //     return entry.description;
            default:
                return QVariant();
        }
    }

    return QVariant();
}
