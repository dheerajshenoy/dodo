#pragma once

#include "CircleLabel.hpp"
#include "DocumentView.hpp"
#include "ElidableLabel.hpp"
#include "GraphicsView.hpp"

#include <QVBoxLayout>
#include <QWidget>

enum class FitMode;

class Panel : public QWidget
{
public:
    Panel(QWidget *parent = nullptr);

    void hidePageInfo(bool state) noexcept;
    void setTotalPageCount(int total) noexcept;
    void setFileName(const QString &name) noexcept;
    void setPageNo(int pageno) noexcept;
    void setSearchCount(int count) noexcept;
    void setSearchIndex(int index) noexcept;
    void setFitMode(DocumentView::FitMode mode) noexcept;
    void setMode(GraphicsView::Mode) noexcept;
    inline bool searchMode() noexcept
    {
        return m_search_mode;
    }
    void setSearchMode(bool state) noexcept;
    void setHighlightColor(const QColor &color) noexcept;

private:
    void initGui() noexcept;
    void labelBG(QLabel *label, const QColor &color) noexcept;
    ElidableLabel *m_filename_label = new ElidableLabel();
    QLabel *m_mode_label            = new QLabel();
    CircleLabel *m_mode_color_label = new CircleLabel();
    QLabel *m_pageno_label          = new QLabel();
    QLabel *m_totalpage_label       = new QLabel();
    QLabel *m_pageno_separator      = new QLabel("/");
    QLabel *m_search_count_label    = new QLabel();
    QLabel *m_search_index_label    = new QLabel();
    QLabel *m_search_label          = new QLabel("Search: ");
    QLabel *m_fitmode_label         = new QLabel("");
    QHBoxLayout *m_layout           = new QHBoxLayout();
    bool m_search_mode{false};
};
