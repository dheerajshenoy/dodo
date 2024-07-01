#include "commandbar.hpp"

CommandBar::CommandBar(QWidget *parent)
    : QWidget(parent)
{
    m_layout->addWidget(m_prompt);
    m_layout->addWidget(m_input, 1);
    this->setLayout(m_layout);
}

void CommandBar::Search()
{
    this->show();
    m_prompt->setText("Search: ");
    SetMode(Mode::SEARCH_MODE);
    m_prompt->show();
    m_input->show();
    m_input->setFocus(Qt::FocusReason::TabFocusReason);
    connect(m_input, &QLineEdit::returnPressed, this, [&]() {
        CommandBar::ProcessInput();
        this->hide();
        SetMode(Mode::VIEW_MODE);
    });

}

void CommandBar::ProcessInput()
{
    QString text = m_input->text();

    if (text.isEmpty())
    {
        emit searchClear();
    }

    if (GetMode() == Mode::SEARCH_MODE)
    {
        emit searchMode(text);
    }
}

CommandBar::~CommandBar()
{}
