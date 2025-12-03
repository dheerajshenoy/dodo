#pragma once

#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QMenu>
#include <QPainter>
#include <QTabBar>
#include <QTabWidget>

class TabWidget : public QTabWidget
{
    Q_OBJECT
public:
    TabWidget(QWidget *parent = nullptr) : QTabWidget(parent)
    {
        setElideMode(Qt::TextElideMode::ElideRight);
        setDocumentMode(true);
        setMovable(true);
        setTabsClosable(true);
        setAcceptDrops(false);
        setStyleSheet("border: 0");
        tabBar()->setDrawBase(false);
    }

    int addTab(QWidget *page, const QString &title)
    {
        int result = QTabWidget::addTab(page, title);
        emit tabAdded(result);
        return result;
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QTabWidget::paintEvent(event);

        if (count() == 0)
        {
            QPainter painter(this);
            painter.fillRect(rect(), palette().color(QPalette::Window));
            painter.setPen(palette().color(QPalette::Disabled, QPalette::Text));

            QString logoText = "Dd";
            QString info     = "Welcome to dodo!\n\n"
                               "Use File → Open to open a PDF file.\n"
                               "You can also drag and drop a PDF file here.";

            // Setup logo font - load from resources
            int fontId = QFontDatabase::addApplicationFont(":/resources/fonts/victor.ttf");
            QString fontFamily = QFontDatabase::applicationFontFamilies(fontId).value(0, QString());
            QFont logoFont;
            if (!fontFamily.isEmpty())
                logoFont.setFamily(fontFamily);
            logoFont.setPointSize(72);
            QFontMetrics logoFm(logoFont);
            int logoHeight = logoFm.height();

            // Setup info font
            QFont infoFont = painter.font();
            infoFont.setPointSize(12);
            QFontMetrics infoFm(infoFont);
            QRect infoBounds = infoFm.boundingRect(
                rect(), Qt::AlignCenter | Qt::TextWordWrap, info);
            int infoHeight = infoBounds.height();

            // Calculate total height and starting Y position
            int spacing     = 20;
            int totalHeight = logoHeight + spacing + infoHeight;
            int startY      = (rect().height() - totalHeight) / 2;

            // Draw logo text
            painter.setFont(logoFont);
            QRect logoRect(0, startY, rect().width(), logoHeight);
            painter.drawText(logoRect, Qt::AlignHCenter | Qt::AlignTop,
                             logoText);

            // Draw info text
            painter.setFont(infoFont);
            QRect infoRect(0, startY + logoHeight + spacing, rect().width(),
                           infoHeight);
            painter.drawText(infoRect,
                             Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                             info);
        }
    }

    int insertTab(int index, QWidget *page, const QString &title)
    {
        int result = QTabWidget::insertTab(index, page, title);
        emit tabAdded(result);
        return result;
    }

signals:
    void tabAdded(int index);
    void openInExplorerRequested(int index);
    void filePropertiesRequested(int index);
};
