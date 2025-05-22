#include "PropertiesWidget.hpp"

PropertiesWidget::PropertiesWidget(QWidget* parent)
: QDialog(parent)
{
    setWindowTitle("PDF Document Properties");
    setMinimumSize(400, 300);
    m_formLayout = new QFormLayout(this);
    setLayout(m_formLayout);
}

void PropertiesWidget::setProperties(const QList<QPair<QString, QString>> &properties) noexcept
{
    // Clear previous content
    QLayoutItem* child;
    while ((child = m_formLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    // Add new labels
    for (const auto &[k, v] : properties)
    {
        QLabel* keyLabel = new QLabel(k + ":", this);
        QLabel* valueLabel = new QLabel(v, this);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_formLayout->addRow(keyLabel, valueLabel);
    }
}

