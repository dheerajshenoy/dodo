#include "CommandBar.hpp"

CommandBar::CommandBar(QWidget *parent) : QLineEdit(parent) {}

void
CommandBar::keyPressEvent(QKeyEvent *e)
{
    Q_UNUSED(e);
    if (e->key() == Qt::Key_Return)
    {
        hide();
        emit processCommand(this->text());
    }
    QLineEdit::keyPressEvent(e);
}
