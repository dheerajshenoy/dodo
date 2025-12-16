#include "Panel.hpp"

#include "GraphicsView.hpp"

#include <mupdf/pdf/page.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qsizepolicy.h>

Panel::Panel(QWidget *parent) : QWidget(parent)
{
    initGui();
    initConnections();
}

void
Panel::initConnections() noexcept
{
    connect(m_mode_color_label, &CircleLabel::clicked,
            [&]() { emit modeColorChangeRequested(m_current_mode); });
    connect(m_pageno_box, &QLineEdit::returnPressed, [&]()
    {
        bool ok;
        int pageno = m_pageno_box->text().toInt(&ok);
        if (ok)
        {
            clearFocus();
            emit pageChangeRequested(pageno);
        }
        else
            QMessageBox::warning(this, "Invalid Page Number",
                                 "Please enter a valid page number");
    });
}

void
Panel::initGui() noexcept
{

    setContentsMargins(0, 0, 0, 0);
    // m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);

    auto *leftLayout = new QHBoxLayout;
    leftLayout->addWidget(m_filename_label);

    auto *centerLayout = new QHBoxLayout;
    m_pageno_box->setFocusPolicy(Qt::ClickFocus);
    centerLayout->addWidget(m_pageno_box);
    centerLayout->addWidget(m_pageno_separator);
    centerLayout->addWidget(m_totalpage_label);

    auto *rightLayout = new QHBoxLayout;
    rightLayout->addWidget(m_mode_color_label);
    rightLayout->addWidget(m_mode_label);

    m_layout->addLayout(leftLayout, 0, 0, Qt::AlignLeft);
    m_layout->addLayout(centerLayout, 0, 1, Qt::AlignCenter);
    m_layout->addLayout(rightLayout, 0, 2, Qt::AlignRight);

    // Stretch the columns correctly
    m_layout->setColumnStretch(0, 1); // left can expand
    m_layout->setColumnStretch(1, 0); // center fixed
    m_layout->setColumnStretch(2, 1); // right can expand

    connect(m_mode_label, &QPushButton::clicked,
            [&]() { emit modeChangeRequested(); });
}

void
Panel::labelBG(QLabel *label, const QColor &color) noexcept
{
    QPalette palette = label->palette();
    palette.setColor(QPalette::Window, color);
    label->setAutoFillBackground(true); // REQUIRED for background to show
    label->setPalette(palette);
}

void
Panel::setTotalPageCount(int total) noexcept
{
    m_totalpage_label->setText(QString::number(total));
}

void
Panel::setFileName(const QString &name) noexcept
{
    m_filename_label->setFullText(name);
}

void
Panel::setPageNo(int pageno) noexcept
{
    m_pageno_box->setText(QString::number(pageno));
    m_pageno_box->setMaximumWidth(
        m_pageno_box->fontMetrics().horizontalAdvance(QString::number(9999))
        + 10);
}

void
Panel::setMode(GraphicsView::Mode mode) noexcept
{
    switch (mode)
    {
        case GraphicsView::Mode::RegionSelection:
            m_mode_label->setText("Region Selection");
            m_mode_color_label->hide();
            break;
        case GraphicsView::Mode::TextSelection:
            m_mode_label->setText("Text Selection");
            m_mode_color_label->hide();
            break;
        case GraphicsView::Mode::TextHighlight:
            m_mode_label->setText("Text Highlight");
            m_mode_color_label->show();
            break;
        case GraphicsView::Mode::AnnotSelect:
            m_mode_label->setText("Annot Select");
            m_mode_color_label->hide();
            break;
        case GraphicsView::Mode::AnnotRect:
            m_mode_label->setText("Annot Rect");
            m_mode_color_label->show();
            break;
        default:
            break;
    }
    m_current_mode = mode;
}

void
Panel::setHighlightColor(const QColor &color) noexcept
{
    m_mode_color_label->setColor(color);
}

void
Panel::hidePageInfo(bool state) noexcept
{
    m_pageno_box->setVisible(!state);
    m_pageno_separator->setVisible(!state);
    m_totalpage_label->setVisible(!state);
    m_mode_label->setVisible(!state);
}
