#include "statusbar.hpp"

StatusBar::StatusBar(QWidget *parent)
    : QWidget(parent)
{
    this->setLayout(m_layout);

    m_msg_label->hide();
    m_layout->addWidget(m_msg_label);
    m_layout->addWidget(m_label_filename, 1);
    m_layout->addWidget(m_label_cur_page_number);
    m_layout->addWidget(m_label_filepagecount);

    m_layout->addWidget(m_label_cur_search_index);
    m_layout->addWidget(m_label_search_count);

    this->setFixedHeight(30);

    this->show();
}

void StatusBar::Message(QString msg, int s)
{
    m_msg_label->setText(msg);
    QTimer::singleShot(s * 1000, [&]() {
        m_msg_label->hide();
    });
}

void StatusBar::SetFileName(QString filename)
{
    m_label_filename->setText(filename);

}

void StatusBar::SetFilePageCount(int count)
{
    m_label_filepagecount->setText(QString::number(count));
}

void StatusBar::SetCurrentPage(int p)
{
    m_label_cur_page_number->setText(QString::number(p + 1));
}

void StatusBar::SetTotalSearchCount(int c)
{
    m_label_search_count->setText(QString::number(c));
}

void StatusBar::SetCurrentSearchIndex(int i)
{
    m_label_cur_search_index->setText(QString::number(i));
}

StatusBar::~StatusBar()
{}
