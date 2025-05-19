#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

class Panel : public QWidget
{
    public:
    Panel(QWidget *parent = nullptr);

    void setTotalPageCount(const unsigned int &total) noexcept;
    void setFileName(const QString &name) noexcept;
    void setPageNo(const unsigned int &pageno) noexcept;
    void setSearchCount(const unsigned int &count) noexcept;
    void setSearchIndex(const unsigned int &index) noexcept;
    void setSearchMode(const bool &state) noexcept;

private:
    void initGui() noexcept;
    QLabel *m_filename_label = new QLabel();
    QLabel *m_pageno_label = new QLabel();
    QLabel *m_totalpage_label = new QLabel();
    QLabel *m_search_count_label = new QLabel();
    QLabel *m_search_index_label = new QLabel();
    QLabel *m_search_label = new QLabel("Search: ");
    QHBoxLayout *m_layout = new QHBoxLayout();

};
