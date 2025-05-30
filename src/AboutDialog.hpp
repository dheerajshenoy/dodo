#pragma once

#include <QDialog>
#include <QFontDatabase>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFile>

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    void setAppInfo(const QString &version, const QString &description) noexcept;

private:
    QLabel *bannerLabel;
    QLabel *infoLabel;
    QPushButton *closeButton;
};
