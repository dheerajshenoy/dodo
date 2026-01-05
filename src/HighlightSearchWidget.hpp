#pragma once

#include "Model.hpp"
#include "WaitingSpinnerWidget.hpp"

#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QWidget>
#include <vector>

class HighlightSearchWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HighlightSearchWidget(QWidget *parent = nullptr);
    void setModel(Model *model) noexcept;
    void refresh() noexcept;
    void focusFilterInput() noexcept;

signals:
    void gotoLocationRequested(int page, const QPointF &pagePos);

private:
    void applyFilter() noexcept;
    void setLoading(bool state) noexcept;

    QPointer<Model> m_model;
    QLineEdit *m_filter_input{nullptr};
    QListWidget *m_list{nullptr};
    QLabel *m_count_label{nullptr};
    QPushButton *m_refresh_button{nullptr};
    QPushButton *m_close_button{nullptr};
    WaitingSpinnerWidget *m_spinner{nullptr};
    QFutureWatcher<std::vector<Model::HighlightText>> m_watcher;
    std::vector<Model::HighlightText> m_entries;

protected:
    void showEvent(QShowEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
};
