#pragma once

#include <QWidget>
#include <QFormLayout>
#include <QDialog>
#include <QLabel>
#include <QPair>
#include <QString>

class PropertiesWidget : public QDialog
{
    Q_OBJECT

    public:
    explicit PropertiesWidget(QWidget* parent = nullptr);
    void setProperties(const QList<QPair<QString, QString>> &properties) noexcept;

private:
    QFormLayout* m_formLayout;
};

