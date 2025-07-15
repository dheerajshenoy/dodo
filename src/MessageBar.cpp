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
    m_queue.enqueue({.message = msg, .duration = sec});
    if (!m_showing)
        showNext();
}

void
MessageBar::showNext() noexcept
{
    if (m_queue.isEmpty())
    {
        m_showing = false;
        return;
    }

    m_showing             = true;
    const auto [msg, sec] = m_queue.dequeue();

    m_label->setText(msg);
    show();
    QTimer::singleShot(sec * 1000, this, [this]()
    {
        hide();
        m_label->clear();
        showNext(); // Show the next message after the current one
    });
}
