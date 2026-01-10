#pragma once

#include <QLineEdit>
#include <QTableView>
#include <QWidget>
#include <qsortfilterproxymodel.h>

// Model for command palette
class CommandModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit CommandModel(const QHash<QString, QString> &shortcutMap,
                          QObject *parent = nullptr) noexcept;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

private:
    struct CommandEntry
    {
        QString name;
        QString shortcut;
        // QString description;
    };

    std::vector<CommandEntry> m_commands;
};

// Widget for command palette
class CommandPaletteWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CommandPaletteWidget(const QHash<QString, QString> &shortcutMap,
                                  QWidget *parent = nullptr) noexcept;
    void selectFirstItem() noexcept;

protected:
    void showEvent(QShowEvent *event) override;

signals:
    void commandSelected(const QString &commandName,
                         const QStringList &args = {});

private:
    void initConnections() noexcept;
    QLineEdit *m_input_line{nullptr};
    QTableView *m_command_table{nullptr};
    CommandModel *m_command_model{nullptr};
    QSortFilterProxyModel *m_proxy_model{nullptr};
    QFrame *m_frame{nullptr};
};
