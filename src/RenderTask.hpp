#include <QRunnable>
#include <poppler/qt6/poppler-qt6.h>
#include <QImage>

class RenderTask : public QObject, public QRunnable {
    Q_OBJECT
    public:
    RenderTask(Poppler::Document *doc,
               int page,
               float dpiX,
               float dpiY,
               bool lowQuality,
               QObject *receiver)
    : m_doc(doc), m_page(page),
    m_dpiX(dpiX), m_dpiY(dpiY),
    m_lowQuality(lowQuality), m_receiver(receiver) {
        setAutoDelete(true);
}

void run() override {
    if (auto page = m_doc->page(m_page)) {
        QImage img = page->renderToImage(m_dpiX, m_dpiY);
        emit finished(m_page, img, m_lowQuality);
        // emitFinished(img);
    }
}

signals:
void finished(int page, QImage img, bool lowQuality);

private:
void emitFinished(const QImage &img) {
    qDebug() << "DD";
    QMetaObject::invokeMethod(m_receiver, [=]() {
        Q_EMIT static_cast<RenderTask*>(this)->finished(m_page, img, m_lowQuality);
    }, Qt::QueuedConnection);
}

Poppler::Document *m_doc;
int m_page;
float m_dpiX, m_dpiY;
bool m_lowQuality;
QObject *m_receiver;
};

