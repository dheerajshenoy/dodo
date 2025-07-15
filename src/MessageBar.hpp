#pragma once

#include <QLabel>
#include <QWidget>
#include <QHBoxLayout>
#include <QQueue>

class MessageBar : public QWidget
{
public:
    MessageBar(QWidget *parent = nullptr);
    void showMessage(const QString &msg, float sec = 2.0f) noexcept;

    struct MessageItem
    {
        QString message;
        float duration;
    };

private:
    void showNext() noexcept;
    bool m_showing { false };
    QQueue<MessageItem> m_queue;
    QLabel *m_label{new QLabel()};
};
