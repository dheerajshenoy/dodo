#include "AboutDialog.hpp"

#include <QFormLayout>
#include <QPainter>
#include <QTextEdit>
#include <qboxlayout.h>
#include <qfont.h>
#include <qnamespace.h>

extern "C"
{
#include <mupdf/fitz.h>
#include <synctex/synctex_version.h>
}

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent), icon(new QSvgWidget), infoLabel(new QLabel),
      closeButton(new QPushButton("Close"))
{
    setWindowTitle("About");
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint
                   & ~Qt::WindowMaximizeButtonHint);

    setMinimumSize(600, 400);

    icon->load(QString(":/resources/dodo2.svg"));
    icon->setFixedSize(100, 100);
    infoLabel->setWordWrap(true);

    m_tabWidget = new QTabWidget(this);

    QHBoxLayout *otherLayout = new QHBoxLayout();
    otherLayout->addWidget(icon);
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

    m_tabWidget->addTab(licenseTextEdit, "License");
    auto *layout = new QVBoxLayout();
    layout->addLayout(otherLayout);
    layout->addWidget(m_tabWidget);
    layout->addWidget(closeButton, 0, Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);

    setLayout(layout);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    QWidget *authorWidget = authorsSection();
    m_tabWidget->addTab(authorWidget, "Author");

    QWidget *softwaresUsed = softwaresUsedSection();
    m_tabWidget->addTab(softwaresUsed, "Libraries Used");
    setWindowModality(Qt::NonModal);
}

void
AboutDialog::setAppInfo(const QString &version,
                        const QString &description) noexcept
{
    auto link = "<a href='https://github.com/dheerajshenoy/dodo'>github</a>";
    infoLabel->setText(
        QString("%1<br>Build Type: %2<br>Version: %3<br>Project homepage: %4")
            .arg(description, __DODO_BUILD_TYPE, version, link));
}

QWidget *
AboutDialog::softwaresUsedSection() noexcept
{
    QWidget *widget     = new QWidget();
    QFormLayout *layout = new QFormLayout();

    QVBoxLayout *outerLayout = new QVBoxLayout();
    layout->setAlignment(Qt::AlignCenter);

    layout->addRow("Qt", new QLabel(QT_VERSION_STR));
    layout->addRow("MuPDF", new QLabel(QString(FZ_VERSION)));
    layout->addRow("SyncTeX", new QLabel(QString(SYNCTEX_VERSION_STRING)));

    outerLayout->addLayout(layout, Qt::AlignCenter);
    widget->setLayout(outerLayout);
    return widget;
}

QWidget *
AboutDialog::authorsSection() noexcept

{
    QWidget *widget = new QWidget(this);

    QFormLayout *layout = new QFormLayout(widget);
    layout->addRow("Created by", new QLabel("Dheeraj Vittal Shenoy"));
    layout->addRow("Github",
                   new QLabel("<a "
                              "href='https://github.com/dheerajshenoy'>https://"
                              "github.com/dheerajshenoy</a>"));
    widget->setLayout(layout);

    return widget;
}
