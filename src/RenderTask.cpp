#include "RenderTask.hpp"

#include <mupdf/fitz/context.h>
#include <mupdf/pdf/event.h>

RenderTask::RenderTask(fz_context *ctx, fz_document *doc, fz_colorspace *colorspace, int pageno, fz_matrix transform)
    : m_ctx(ctx), m_doc(doc), m_colorspace(colorspace), m_pageno(pageno), m_transform(transform)
{
    setAutoDelete(true); // let threadpool clean this up automatically
}

void
RenderTask::run()
{
    QImage image;
    if (!m_ctx)
        return;

    fz_try(m_ctx)
    {
        fz_page *page = fz_load_page(m_ctx, m_doc, m_pageno);
        if (!page)
        {
            emit finished(m_pageno, image);
            return;
        }

        fz_rect bounds;
        bounds = fz_bound_page(m_ctx, page);
        // TODO: Load link here maybe ?
        fz_rect transformed = fz_transform_rect(bounds, m_transform);
        fz_irect bbox       = fz_round_rect(transformed);

        fz_pixmap *pix;
        pix = fz_new_pixmap_with_bbox(m_ctx, m_colorspace, bbox, nullptr, 0);
        if (!pix)
        {
            fz_drop_page(m_ctx, page);
            fz_drop_context(m_ctx);
            return;
        }

        fz_clear_pixmap_with_value(m_ctx, pix, 255); // 255 = white
        fz_device *dev = fz_new_draw_device(m_ctx, m_transform, pix);

        if (!dev)
        {
            fz_drop_page(m_ctx, page);
            fz_drop_context(m_ctx);
            return;
        }
        fz_run_page(m_ctx, page, dev, fz_identity, nullptr);

        // Convert fz_pixmap to QImage
        int width              = fz_pixmap_width(m_ctx, pix);
        int height             = fz_pixmap_height(m_ctx, pix);
        unsigned char *samples = fz_pixmap_samples(m_ctx, pix);
        int stride             = fz_pixmap_stride(m_ctx, pix);

        // Assume RGB, 8-bit per channel (no alpha)
        image = QImage(width, height, QImage::Format_RGB888);

        for (int y = 0; y < height; ++y)
        {
            memcpy(image.scanLine(y), samples + y * stride, width * 3); // 3 bytes per pixel
        }

        // Cleanup
        fz_close_device(m_ctx, dev);
        fz_drop_device(m_ctx, dev);
        fz_drop_pixmap(m_ctx, pix);
        fz_drop_page(m_ctx, page);
        fz_drop_context(m_ctx);
    }
    fz_catch(m_ctx)
    {
        qWarning() << "Render failed for page" << m_pageno;
        emit finished(m_pageno, image);
        return;
    }

    emit finished(m_pageno, image);
}
