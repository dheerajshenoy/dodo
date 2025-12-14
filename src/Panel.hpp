#pragma once

#include "CircleLabel.hpp"
#include "DocumentView.hpp"
#include "ElidableLabel.hpp"
#include "GraphicsView.hpp"

#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

enum class FitMode;

class Panel : public QWidget
{
    Q_OBJECT
public:
    Panel(QWidget *parent = nullptr);

    void hidePageInfo(bool state) noexcept;
    void setTotalPageCount(int total) noexcept;
    void setFileName(const QString &name) noexcept;
    void setPageNo(int pageno) noexcept;
    void setFitMode(DocumentView::FitMode mode) noexcept;
    void setMode(GraphicsView::Mode) noexcept;
    void setHighlightColor(const QColor &color) noexcept;

signals:
    void modeChangeRequested();
    void fitModeChangeRequested();
    void modeColorChangeRequested(GraphicsView::Mode);
    void pageChangeRequested(int pageno);

private:
    void initGui() noexcept;
    void labelBG(QLabel *label, const QColor &color) noexcept;
    ElidableLabel *m_filename_label = new ElidableLabel();
    QPushButton *m_mode_label       = new QPushButton();
    CircleLabel *m_mode_color_label = new CircleLabel();
    QLineEdit *m_pageno_box         = new QLineEdit();
    QLabel *m_totalpage_label       = new QLabel();
    QLabel *m_pageno_separator      = new QLabel(" of ");
    QGridLayout *m_layout           = new QGridLayout();
    GraphicsView::Mode m_current_mode;
};
