#pragma once

#include <QWidget>
#include <qboxlayout.h>
#include <qlabel.h>

class PlaceholderWidget : public QWidget
{
public:
    PlaceholderWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        QVBoxLayout *layout = new QVBoxLayout();
        layout->setAlignment(Qt::AlignCenter);

        QLabel *text = new QLabel("Open file(s) to see them here");
        QPalette palette = text->palette();
        QColor color = palette.color(QPalette::PlaceholderText);
        palette.setColor(QPalette::WindowText, color);
        text->setPalette(palette);
        layout->addWidget(text);
        setLayout(layout);
    }

private:
};
