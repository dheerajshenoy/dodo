#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include "ElidableLabel.hpp"
#include "GraphicsView.hpp"

enum class FitMode;

class Panel : public QWidget
{
    public:
    Panel(QWidget *parent = nullptr);

    void setTotalPageCount(int total) noexcept;
    void setFileName(const QString &name) noexcept;
    void setPageNo(int pageno) noexcept;
    void setSearchCount(int count) noexcept;
    void setSearchIndex(int index) noexcept;
    void setFitMode(const QString &mode) noexcept;
    void setMode(GraphicsView::Mode) noexcept;

    inline bool searchMode() noexcept { return m_search_mode; }
    void setSearchMode(bool state) noexcept;

private:
    void initGui() noexcept;
    ElidableLabel *m_filename_label = new ElidableLabel();
    ElidableLabel *m_mode_label = new ElidableLabel();
    QLabel *m_pageno_label = new QLabel();
    QLabel *m_totalpage_label = new QLabel();
    QLabel *m_search_count_label = new QLabel();
    QLabel *m_search_index_label = new QLabel();
    QLabel *m_search_label = new QLabel("Search: ");
    QLabel *m_fitmode_label = new QLabel("");
    QHBoxLayout *m_layout = new QHBoxLayout();
    bool m_search_mode { false };

};
