#include "Panel.hpp"

#include "GraphicsView.hpp"

#include <qnamespace.h>
#include <qsizepolicy.h>

Panel::Panel(QWidget *parent) : QWidget(parent)
{
    initGui();
}

void
Panel::initGui() noexcept
{
    setLayout(m_layout);

    auto *leftLayout = new QHBoxLayout;
    leftLayout->addWidget(m_filename_label);

    auto *centerLayout = new QHBoxLayout;
    centerLayout->addWidget(m_pageno_label);
    centerLayout->addWidget(m_pageno_separator);
    centerLayout->addWidget(m_totalpage_label);

    auto *rightLayout = new QHBoxLayout;
    rightLayout->addWidget(m_mode_color_label);
    rightLayout->addWidget(m_mode_label);
    rightLayout->addWidget(m_fitmode_label);
    rightLayout->addWidget(m_search_label);
    rightLayout->addWidget(m_search_index_label);
    rightLayout->addWidget(m_search_count_label);

    m_layout->addLayout(leftLayout, 0, 0, Qt::AlignLeft);
    m_layout->addLayout(centerLayout, 0, 1, Qt::AlignCenter);
    m_layout->addLayout(rightLayout, 0, 2, Qt::AlignRight);

    // Stretch the columns correctly
    m_layout->setColumnStretch(0, 1); // left can expand
    m_layout->setColumnStretch(1, 0); // center fixed
    m_layout->setColumnStretch(2, 1); // right can expand

    connect(m_mode_label, &QPushButton::clicked,
            [&]() { emit modeChangeRequested(); });

    connect(m_fitmode_label, &QPushButton::clicked,
            [&]() { emit fitModeChangeRequested(); });

    this->setSearchMode(false);
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
    m_pageno_label->setText(QString::number(pageno));
}

void
Panel::setSearchCount(int count) noexcept
{
    m_search_count_label->setText(QString::number(count));
}

void
Panel::setSearchIndex(int index) noexcept
{
    m_search_index_label->setText(QString::number(index));
}

void
Panel::setSearchMode(bool state) noexcept
{
    if (!state)
    {
        m_search_index_label->setText("");
        m_search_count_label->setText("");
    }
    m_search_label->setVisible(state);
    m_search_index_label->setVisible(state);
    m_search_count_label->setVisible(state);
    m_search_mode = state;
}

void
Panel::setFitMode(DocumentView::FitMode mode) noexcept
{
    switch (mode)
    {
        case DocumentView::FitMode::None:
            m_fitmode_label->setText("None");
            break;

        case DocumentView::FitMode::Width:
            m_fitmode_label->setText("Width");
            break;
        case DocumentView::FitMode::Height:
            m_fitmode_label->setText("Height");
            break;
        case DocumentView::FitMode::Window:
            m_fitmode_label->setText("Window");
            break;
    }
}

void
Panel::setMode(GraphicsView::Mode mode) noexcept
{
    switch (mode)
    {
        case GraphicsView::Mode::TextSelection:
            m_mode_label->setText("Selection");
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
    }
}

void
Panel::setHighlightColor(const QColor &color) noexcept
{
    m_mode_color_label->setColor(color);
}

void
Panel::hidePageInfo(bool state) noexcept
{
    m_pageno_label->setVisible(!state);
    m_pageno_separator->setVisible(!state);
    m_totalpage_label->setVisible(!state);
    m_mode_label->setVisible(!state);
    m_fitmode_label->setVisible(!state);
}
