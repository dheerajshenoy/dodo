#include "MessageBar.hpp"

#include <QTimer>

MessageBar::MessageBar(QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(m_label);
    setLayout(layout);
}

void
MessageBar::showMessage(const QString &msg, float sec) noexcept
{
    show();
    m_label->setText(msg);
    QTimer::singleShot(sec * 1000, [this]()
    {
        hide();
        m_label->clear();
    });
}
