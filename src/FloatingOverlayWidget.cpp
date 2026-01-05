#include "FloatingOverlayWidget.hpp"

#include <QHBoxLayout>
#include <QKeySequence>
#include <QShortcut>
#include <QVBoxLayout>
#include <qnamespace.h>

FloatingOverlayWidget::FloatingOverlayWidget(QWidget *parent) : QWidget(parent)
{
    setVisible(false);
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setWindowModality(Qt::ApplicationModal);
    setWindowFlags(Qt::FramelessWindowHint);
    setFocusPolicy(Qt::StrongFocus);
    if (parent)
    {
        setGeometry(parent->rect());
        parent->installEventFilter(this);
    }

    m_dim = new QWidget(this);
    m_dim->setStyleSheet("background-color: rgba(0, 0, 0, 120);");

    m_frame = new QFrame(this);
    m_frame->setFrameShape(QFrame::NoFrame);
    m_frame->setStyleSheet("background-color: palette(base);");
    m_frame->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    auto *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escShortcut, &QShortcut::activated, this, [this]() { hide(); });

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QVBoxLayout *center_v = new QVBoxLayout();
    center_v->addStretch();

    QHBoxLayout *center_h = new QHBoxLayout();
    center_h->addStretch();
    center_h->addWidget(m_frame);
    center_h->addStretch();

    center_v->addLayout(center_h);
    center_v->addStretch();

    root->addWidget(m_dim);
    root->addLayout(center_v);
}

void
FloatingOverlayWidget::setContentWidget(QWidget *widget) noexcept
{
    if (m_content == widget)
        return;

    if (m_content)
        m_content->setParent(nullptr);

    m_content = widget;
    if (!m_content)
        return;

    m_content->setParent(m_frame);
    QVBoxLayout *layout = new QVBoxLayout(m_frame);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->addWidget(m_content);
}

QWidget *
FloatingOverlayWidget::contentWidget() const noexcept
{
    return m_content;
}

void
FloatingOverlayWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_dim)
        m_dim->setGeometry(rect());
}

bool
FloatingOverlayWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize)
    {
        setGeometry(parentWidget()->rect());
    }
    return QWidget::eventFilter(watched, event);
}

void
FloatingOverlayWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_content)
        m_content->setFocus(Qt::OtherFocusReason);
}

void
FloatingOverlayWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        hide();
}

void
FloatingOverlayWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit overlayHidden();
}
