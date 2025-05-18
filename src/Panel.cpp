#include "Panel.hpp"

Panel::Panel(QWidget *parent)
: QWidget(parent)
{
    initGui();
}

void Panel::initGui() noexcept
{
    this->setLayout(m_layout);

    m_layout->addWidget(m_filename_label);
    m_layout->addStretch();
    m_layout->addWidget(m_pageno_label);
    m_layout->addWidget(m_totalpage_label);
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
