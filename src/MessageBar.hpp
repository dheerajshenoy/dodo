#pragma once

#include <QLabel>
#include <QWidget>
#include <QHBoxLayout>

class MessageBar : public QWidget
{
public:
    MessageBar(QWidget *parent = nullptr);
    void showMessage(const QString &msg, float sec = 2.0f) noexcept;

private:
    QLabel *m_label{new QLabel()};
};
