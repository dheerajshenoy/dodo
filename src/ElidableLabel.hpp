#pragma once

#include <QLabel>

class ElidableLabel : public QLabel
{
    Q_OBJECT
    public:
    explicit ElidableLabel(QWidget *parent = nullptr)
    : QLabel(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void setFullText(const QString &text) {
    m_fullText = text;
    updateElidedText();
}

protected:
void resizeEvent(QResizeEvent *event) override {
    QLabel::resizeEvent(event);
updateElidedText();
    }

private:
QString m_fullText;

void updateElidedText() {
    QFontMetrics metrics(font());
    QString elided = metrics.elidedText(m_fullText, Qt::ElideRight, width());
    setText(elided);
}
};

