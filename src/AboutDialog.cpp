#include "AboutDialog.hpp"

#include <QPainter>
#include <QTextEdit>
#include <qfont.h>
#include <qnamespace.h>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent), bannerLabel(new QLabel), infoLabel(new QLabel), closeButton(new QPushButton("Close"))
{
    setWindowTitle("About");
    setModal(true);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint);

    setFixedSize(400, 300);

    QPixmap bannerPix(":/resources/hicolor/512x512/apps/dodo.png");
    bannerPix = bannerPix.scaled(100, 100, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    bannerLabel->setPixmap(bannerPix);

    bannerLabel->setAlignment(Qt::AlignCenter);
    infoLabel->setWordWrap(true);

    QHBoxLayout *otherLayout = new QHBoxLayout();
    otherLayout->addWidget(bannerLabel);
    otherLayout->addWidget(infoLabel);

    QTextEdit *licenseTextEdit = new QTextEdit();
    licenseTextEdit->setReadOnly(true);

    QFile file(":/LICENSE"); // Or use an absolute/relative path

    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        QString text = in.readAll();
        licenseTextEdit->setPlainText(text);
        file.close();
    }
    else
    {
        licenseTextEdit->setPlainText("Could not load license text.");
    }

    otherLayout->setContentsMargins(10, 10, 10, 10);

    auto *layout = new QVBoxLayout();
    layout->addLayout(otherLayout);
    layout->addWidget(licenseTextEdit);
    layout->addWidget(closeButton, 0, Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);

    setLayout(layout);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void
AboutDialog::setAppInfo(const QString &version, const QString &description) noexcept
{
    auto link = "<a href='https://github.com/dheerajshenoy/dodo'>github</a>";
    infoLabel->setText(QString("%1<br>Version: %2<br>Project homepage: %3").arg(description, version, link));
}
