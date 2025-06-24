#pragma once

#include <QDialog>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QtSvgWidgets/QSvgWidget>

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    void setAppInfo(const QString &version, const QString &description) noexcept;

private:
    QWidget* softwaresUsedSection() noexcept;
    QWidget* authorsSection() noexcept;

    QSvgWidget *icon;
    QLabel *infoLabel;
    QPushButton *closeButton;
    QTabWidget *m_tabWidget;
};
