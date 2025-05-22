#include "Panel.hpp"

Panel::Panel(QWidget *parent)
: QWidget(parent)
{
    initGui();
}

void Panel::initGui() noexcept
{
    this->setLayout(m_layout);

    // Left widgets
    m_layout->addWidget(m_filename_label);

    m_layout->addStretch();

    // Right widgets
    m_layout->addWidget(new QLabel("Page: "));
    m_layout->addWidget(m_pageno_label);
    m_layout->addWidget(new QLabel("/"));
    m_layout->addWidget(m_totalpage_label);

    m_layout->addWidget(m_search_label);
    m_layout->addWidget(m_search_index_label);
    m_layout->addWidget(m_search_count_label);

    this->setSearchMode(false);
}


void Panel::setTotalPageCount(const unsigned int &total) noexcept
{
    m_totalpage_label->setText(QString::number(total));
}

void Panel::setFileName(const QString &name) noexcept
{
    m_filename_label->setText(name);
}

void Panel::setPageNo(const unsigned int &pageno) noexcept
{
    m_pageno_label->setText(QString::number(pageno));
}


void Panel::setSearchCount(const unsigned int &count) noexcept
{
    m_search_count_label->setText(QString::number(count));
}

void Panel::setSearchIndex(const unsigned int &index) noexcept
{
    m_search_index_label->setText(QString::number(index));
}

void Panel::setSearchMode(const bool &state) noexcept
{
    if (!state)
    {
        m_search_index_label->setText("");
        m_search_count_label->setText("");
    }
    m_search_label->setVisible(state);
    m_search_index_label->setVisible(state);
    m_search_count_label->setVisible(state);
}
