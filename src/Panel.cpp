#include "Panel.hpp"

#include "GraphicsView.hpp"

#include <qsizepolicy.h>

Panel::Panel(QWidget *parent) : QWidget(parent)
{
    initGui();
}

void
Panel::initGui() noexcept
{
    setLayout(m_layout);

    // Left widgets
    m_layout->addWidget(m_filename_label, 1);

    // Right widgets
    m_layout->addWidget(m_mode_color_label);
    m_layout->addWidget(m_mode_label);

    m_layout->addWidget(m_pageno_label);
    m_layout->addWidget(m_pageno_separator);
    m_layout->addWidget(m_totalpage_label);

    m_layout->addWidget(m_fitmode_label);
    m_layout->addWidget(m_search_label);
    m_layout->addWidget(m_search_index_label);
    m_layout->addWidget(m_search_count_label);

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
    switch(mode)
    {
        case DocumentView::FitMode::None:
            m_fitmode_label->clear();
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


void Panel::hidePageInfo(bool state) noexcept
{
    m_pageno_label->setVisible(state);
    m_pageno_separator->setVisible(state);
    m_totalpage_label->setVisible(state);
}
