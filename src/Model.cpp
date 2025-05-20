#include "Model.hpp"
#include <QGraphicsScene>
#include <QImage>
#include <QDebug>

#define CSTR(x) x.toStdString().c_str()

Model::Model(QGraphicsScene *scene)
: m_scene(scene)
{
    m_ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (!m_ctx) {
        qWarning("Unable to create mupdf context");
        exit(-1);
    }
    fz_register_document_handlers(m_ctx);
}

Model::~Model()
{
    fz_drop_document(m_ctx, m_doc);
    fz_drop_context(m_ctx);
}

bool Model::openFile(const QString &fileName)
{
    m_doc = fz_open_document(m_ctx, CSTR(fileName));
    if (!m_doc)
    {
        qWarning("Unable to open document");
        return false;
    }

    return true;
}

bool Model::valid()
{
    return m_doc;
}

int Model::numPages()
{
    if (m_doc)
        return fz_count_pages(m_ctx, m_doc);
    else
        return -1;
}

void Model::setLinkBoundaryBox(bool state)
{
    m_link_boundary_enabled = state;
}

QImage Model::renderPage(int pageno, bool lowQuality)
{
    float scale = m_dpi / 72.0;
    qDebug() << scale;
    fz_page *page = fz_load_page(m_ctx, m_doc, pageno);
    fz_rect bounds;
    bounds = fz_bound_page(m_ctx, page);
    fz_matrix transform = fz_scale(scale, scale);
    fz_rect transformed = fz_transform_rect(bounds, transform);
    fz_irect bbox = fz_round_rect(transformed);

    fz_pixmap *pix = fz_new_pixmap_with_bbox(m_ctx,
                                    fz_device_rgb(m_ctx),
                                    bbox,
                                    nullptr,
                                    0);
    fz_clear_pixmap_with_value(m_ctx, pix, 255); // 255 = white
    fz_device *dev = fz_new_draw_device(m_ctx, transform, pix);
    fz_run_page(m_ctx, page, dev, fz_identity, nullptr);

    // Convert fz_pixmap to QImage
    int width = fz_pixmap_width(m_ctx,pix);
    int height = fz_pixmap_height(m_ctx,pix);
    unsigned char *samples = fz_pixmap_samples(m_ctx,pix);
    int stride = fz_pixmap_stride(m_ctx,pix);

    // Assume RGB, 8-bit per channel (no alpha)
    QImage image(width, height, QImage::Format_RGB888);

    for (int y = 0; y < height; ++y)
    {
        memcpy(image.scanLine(y), samples + y * stride, width * 3);  // 3 bytes per pixel
    }

    // Cleanup
    fz_drop_pixmap(m_ctx, pix);
    fz_drop_page(m_ctx, page);

    return image;
}

void Model::searchAll(const QString &term)
{

}

void Model::renderLinks(int pageno)
{
    fz_page *page = fz_load_page(m_ctx, m_doc, pageno);

    fz_link *link = fz_load_links(m_ctx, page);

    while (link)
    {
        if (link->uri) {
            qDebug() << link->uri;
        }
        link = link->next;
    }

    fz_drop_page(m_ctx, page);
}
