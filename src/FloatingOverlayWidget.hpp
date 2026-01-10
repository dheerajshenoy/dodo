#pragma once

#include <QEvent>
#include <QFrame>
#include <QHideEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QWidget>

class FloatingOverlayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FloatingOverlayWidget(QWidget *parent = nullptr);
    void setContentWidget(QWidget *widget) noexcept;
    QWidget *contentWidget() const noexcept;

signals:
    void overlayHidden();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QFrame *m_frame{nullptr};
    QWidget *m_content{nullptr};
};
