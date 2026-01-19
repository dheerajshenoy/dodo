#include "FloatingOverlayWidget.hpp"

#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <qnamespace.h>

FloatingOverlayWidget::FloatingOverlayWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setWindowFlags(Qt::Widget);
    setFocusPolicy(Qt::StrongFocus);
    if (parent)
    {
        setGeometry(parent->rect());
        parent->installEventFilter(this);
    }

    m_frame = new QFrame(this);
    m_frame->setFrameShape(QFrame::NoFrame);
    m_frame->setStyleSheet("background-color: palette(base);");
    m_frame->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    m_frame->setAttribute(Qt::WA_StyledBackground, true);
    m_shadow_effect = new QGraphicsDropShadowEffect(m_frame);
    m_frame->setGraphicsEffect(m_shadow_effect);
    applyFrameStyle();

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
    applyFrameStyle();
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
FloatingOverlayWidget::setFrameStyle(const FrameStyle &style) noexcept
{
    m_frame_style = style;
    applyFrameStyle();
}

void
FloatingOverlayWidget::applyFrameStyle() noexcept
{
    if (!m_frame)
        return;

    if (m_frame_style.border)
    {
        m_frame->setObjectName("overlayFrameBorder");
        m_frame->setStyleSheet("QFrame#overlayFrameBorder {"
                               " background-color: palette(base);"
                               " border: 1px solid palette(highlight);"
                               " border-radius: 8px;"
                               " }");
    }
    else
    {
        m_frame->setObjectName(QString());
        m_frame->setStyleSheet("background-color: palette(base);");
    }

    if (!m_shadow_effect)
        return;

    m_shadow_effect->setEnabled(m_frame_style.shadow);
    if (!m_frame_style.shadow)
        return;

    const int blur  = std::max(0, m_frame_style.shadow_blur_radius);
    const int alpha = std::clamp(m_frame_style.shadow_opacity, 0, 255);
    m_shadow_effect->setBlurRadius(blur);
    m_shadow_effect->setOffset(m_frame_style.shadow_offset_x,
                               m_frame_style.shadow_offset_y);
    m_shadow_effect->setColor(QColor(0, 0, 0, alpha));
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
    {
        m_content->show();
        m_content->activateWindow();
    }
}

void
FloatingOverlayWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        m_content->clearFocus();
        close();
    }
}

void
FloatingOverlayWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit overlayHidden();
}
