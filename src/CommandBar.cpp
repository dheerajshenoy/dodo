#include "CommandBar.hpp"

#include <qnamespace.h>

CommandBar::CommandBar(QWidget *parent) : QLineEdit(parent)
{
    setPlaceholderText("Enter a valid command");
    this->installEventFilter(this);
}

void
CommandBar::keyReleaseEvent(QKeyEvent *e)
{
    switch (e->key())
    {
        case Qt::Key_Escape:
            hide();
            break;

        case Qt::Key_Return:
        case Qt::Key_Enter:
            hide();
            emit processCommand(this->text());
            break;
    }
}
