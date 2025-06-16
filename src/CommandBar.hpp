#pragma once

#include <QLineEdit>
#include <QKeyEvent>

class CommandBar : public QLineEdit
{
    Q_OBJECT
public:
    CommandBar(QWidget *parent = nullptr);

signals:
    void processCommand(const QString &cmd);

protected:
    void keyPressEvent(QKeyEvent *e) override;

};
