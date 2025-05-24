#pragma once

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPixmap>
#include <QFontDatabase>

class AboutDialog : public QDialog {
    Q_OBJECT

    public:
    explicit AboutDialog(QWidget *parent = nullptr);
    void setAppInfo(const QString &version, const QString &description) noexcept;

private:
    QLabel *bannerLabel;
    QLabel *infoLabel;
    QPushButton *closeButton;
};

