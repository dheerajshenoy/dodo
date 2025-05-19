#include <QObject>
#include <poppler/qt6/poppler-qt6.h>
#include <QImage>

class RenderWorker : public QObject {
    Q_OBJECT
    public:
    RenderWorker(Poppler::Document *doc,
                 int pageno,
                 float dpi_x,
                 float dpi_y)
    : m_doc(doc), m_pageno(pageno), m_dpi_x(dpi_x), m_dpi_y(dpi_y)
    {}

    signals:
        void finished(int pageno, QImage image);

public slots:
    void process() {
        auto page = m_doc->page(m_pageno);
        if (!page) {
            emit finished(m_pageno, QImage()); // null image
            return;
        }

        QImage image = page->renderToImage(m_dpi_x, m_dpi_y);
        emit finished(m_pageno, image);
    }

private:
    Poppler::Document *m_doc;
    int m_pageno;
    float m_dpi_x;
    float m_dpi_y;
};

