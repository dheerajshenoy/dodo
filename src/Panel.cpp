#include "Panel.hpp"
#include "GraphicsView.hpp"

Panel::Panel(QWidget *parent)
: QWidget(parent)
{
    initGui();
}

void Panel::initGui() noexcept
{
    this->setLayout(m_layout);

    // Left widgets
    m_layout->addWidget(m_mode_label);
    m_layout->addWidget(m_filename_label);

    // Right widgets
    m_layout->addWidget(new QLabel("["));
    m_layout->addWidget(m_pageno_label);
    m_layout->addWidget(new QLabel("/"));
    m_layout->addWidget(m_totalpage_label);
    m_layout->addWidget(new QLabel("]"));

    m_layout->addWidget(m_fitmode_label);

    m_layout->addWidget(m_search_label);
    m_layout->addWidget(m_search_index_label);
    m_layout->addWidget(m_search_count_label);

    this->setSearchMode(false);
}


void Panel::setTotalPageCount(int total) noexcept
{
    m_totalpage_label->setText(QString::number(total));
}

void Panel::setFileName(const QString &name) noexcept
{
    m_filename_label->setFullText(name);
}

void Panel::setPageNo(int pageno) noexcept
{
    m_pageno_label->setText(QString::number(pageno));
}


void Panel::setSearchCount(int count) noexcept
{
    m_search_count_label->setText(QString::number(count));
}

void Panel::setSearchIndex(int index) noexcept
{
    m_search_index_label->setText(QString::number(index));
}

void Panel::setSearchMode(bool state) noexcept
{
    if (!state)
    {
        m_search_index_label->setText("");
        m_search_count_label->setText("");
    }
    m_search_label->setVisible(state);
    m_search_index_label->setVisible(state);
    m_search_count_label->setVisible(state);
    m_search_mode = state;
}

void Panel::setFitMode(const QString &fit) noexcept
{
    m_fitmode_label->setText(fit);
}

void Panel::setMode(GraphicsView::Mode mode) noexcept
{
    using enum GraphicsView::Mode;
    switch(mode)
    {
        case GraphicsView::Mode::None:
            m_mode_label->setText("None");
            break;
        case GraphicsView::Mode::TextSelection:
            m_mode_label->setText("Selection");
            break;
        case GraphicsView::Mode::TextHighlight:
            m_mode_label->setText("Text Highlight");
            break;
        case GraphicsView::Mode::AnnotSelect:
            m_mode_label->setText("Annot Select");
            break;
        case GraphicsView::Mode::AnnotRect:
            m_mode_label->setText("Annot Rect");
            break;
    }
}
