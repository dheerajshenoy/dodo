#include <QObject>
#include <poppler/qt6/poppler-qt6.h>
#include <QImage>
#include <atomic>

class RenderWorker : public QObject {
    Q_OBJECT
    public:
    RenderWorker(Poppler::Document *doc,
                 int pageno,
                 float dpi_x,
                 float dpi_y)
    : m_doc(doc), m_pageno(pageno), m_dpi_x(dpi_x), m_dpi_y(dpi_y)
    {}

    void abort() noexcept { m_abort.store(true); }

    signals:
        void finished(int pageno, QImage image);


public slots:
    void process() noexcept {
        if (m_abort.load())
            return emit finished(m_pageno, QImage());

        auto page = m_doc->page(m_pageno);

        if (!page) {
            emit finished(m_pageno, QImage()); // null image
            return;
        }

        QImage image = page->renderToImage(m_dpi_x, m_dpi_y);

        if (m_abort.load())
            return emit finished(m_pageno, QImage());

        emit finished(m_pageno, image);
    }

private:
    Poppler::Document *m_doc;
    int m_pageno;
    float m_dpi_x;
    float m_dpi_y;
    std::atomic<bool> m_abort;
};

