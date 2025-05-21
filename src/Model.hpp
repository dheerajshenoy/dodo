#pragma once

#include <QString>
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include "BrowseLinkItem.hpp"
#include "RenderTask.hpp"
#include <QObject>
#include <QThreadPool>
#include <mutex>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include "OutlineWidget.hpp"
#define CSTR(x) x.toStdString().c_str()

class QImage;
class QGraphicsScene;


void lock_mutex(void* user, int lock);
void unlock_mutex(void* user, int lock);

class Model : public QObject
{
    Q_OBJECT
public:
    Model(QGraphicsScene *scene);
    ~Model();
    bool openFile(const QString &fileName);
    bool valid();
    inline void setDPI(float dpi) { m_dpi = dpi; }
    inline void setLowDPI(float low_dpi) { m_low_dpi = low_dpi; }
    int numPages();
    void renderPage(int pageno, bool lowQuality);
    void renderLinks(int pageno, const fz_matrix& transform);
    void setLinkBoundaryBox(bool state);
    void searchAll(const QString &term);
    void addHighlightAnnotation(int pageno, const QRectF &rect) noexcept;
    bool save() noexcept;
    fz_rect convertToMuPdfRect(const QRectF &qtRect,
                               const fz_matrix &transform, float dpiScale) noexcept;

    OutlineWidget* tableOfContents() noexcept;
    inline fz_matrix transform() noexcept { return m_transform; }

    signals:
    void jumpToPageRequested(int pageno);
    void jumpToLocationRequested(int pageno, const BrowseLinkItem::Location &loc);
    void imageRenderRequested(int pageno, QImage img, bool lowQuality);
    void searchResultsReady(const QMap<int, QList<QRectF>> &results, int matchCount);
    void horizontalFitRequested();
    void verticalFitRequested();
    void fitRectRequested(int pageno, float x, float y, float w, float h);
    void fitXYZRequested(int pageno, float x, float y, float zoom);

private:
    void clearLinks() noexcept;

    QString m_filename;
    QList<QRectF> searchHelper(int pageno, const QString &term);

    std::mutex m_locks[FZ_LOCK_MAX];
    float m_dpi, m_low_dpi;
    bool m_link_boundary_enabled;

    QGraphicsScene *m_scene { nullptr };
    fz_context *m_ctx { nullptr };
    fz_document *m_doc { nullptr };
    fz_matrix m_transform;
};
