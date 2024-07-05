#include "label.hpp"

Label::Label(QWidget *parent)
: QLabel(parent)
{
    
}

Label::~Label()
{}


void Label::mousePressEvent(QMouseEvent *e)
{
    m_annot_start_pos = e->pos();

    if (!m_rubber_band)
    {
        m_rubber_band = new QRubberBand(QRubberBand::Rectangle, this);
    }

    m_rubber_band->setGeometry(QRect(m_annot_start_pos, QSize()));
    m_rubber_band->show();
}

void Label::mouseMoveEvent(QMouseEvent *e)
{
    m_rubber_band->setGeometry(QRect(m_annot_start_pos, e->pos()).normalized());
}

void Label::mouseReleaseEvent(QMouseEvent *e)
{
        m_annot_end_pos = e->pos();
        
        if(m_rubber_band)
        {
            m_rubber_band->hide();
            emit HighlightRect(m_rubber_band->frameGeometry());
        }
}


