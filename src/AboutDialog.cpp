#include "AboutDialog.hpp"
#include <qfont.h>
#include <qnamespace.h>

AboutDialog::AboutDialog(QWidget *parent)
: QDialog(parent), bannerLabel(new QLabel), infoLabel(new QLabel), closeButton(new QPushButton("Close")) {

    int fontid = QFontDatabase::addApplicationFont(":/resources/fonts/comfortaa.ttf");
    setWindowTitle("About");
    setModal(true);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint);

    resize(600, 300);

    QFont bannerFont;
    bannerFont.setFamily(QFontDatabase::applicationFontFamilies(fontid).at(0));
    bannerFont.setBold(true);
    bannerFont.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
    bannerFont.setPixelSize(40);
    bannerLabel->setFont(bannerFont);

    QPalette bannerPalette = bannerLabel->palette();
    bannerPalette.setColor(QPalette::WindowText, QColor::fromString("#000000"));
    bannerPalette.setBrush(QPalette::ColorRole::Window, QBrush("#f7a5b9"));
    bannerLabel->setAutoFillBackground(true);
    bannerLabel->setPalette(bannerPalette);

    bannerLabel->setText(" dodo");
    bannerLabel->setAlignment(Qt::AlignLeft);
    infoLabel->setWordWrap(true);

    auto *bannerLayout = new QVBoxLayout();
    bannerLayout->setContentsMargins(0, 0, 0, 0);
    bannerLayout->addWidget(bannerLabel);

    auto *otherLayout = new QVBoxLayout();
    otherLayout->addWidget(infoLabel);
    otherLayout->addWidget(closeButton, 0, Qt::AlignCenter);
    otherLayout->setContentsMargins(10, 10, 10,10);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(bannerLayout);
    layout->addLayout(otherLayout);
    layout->setContentsMargins(0, 0, 0, 0);

    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void AboutDialog::setAppInfo(const QString &version,
                             const QString &description) noexcept {
    auto link = "<a href='https://github.com/dheerajshenoy/dodo'>https://github.com/dheerajshenoy/dodo</a>";
    infoLabel->setText(QString("%1<br>Version: %2<br>Project homepage: %3").arg(description, version, link));
}

