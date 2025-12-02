#pragma once

#include "../Model.hpp"

#include <QRectF>
#include <QUndoCommand>
#include <mupdf/pdf.h>

class RectAnnotationCommand : public QUndoCommand
{
public:
    RectAnnotationCommand(Model *model, int pageno, const fz_rect &bbox,
                          const float color[4], QUndoCommand *parent = nullptr)
        : QUndoCommand("Add Rectangle Annotation", parent), m_model(model),
          m_pageno(pageno), m_rect(bbox)
    {
        std::copy(color, color + 4, m_color);
    }

    void undo() override
    {
        if (!m_model || m_objNum < 0)
            return;

        fz_context *ctx   = m_model->context();
        fz_document *doc  = m_model->document();
        pdf_document *pdf = pdf_specifics(ctx, doc);

        if (!pdf)
            return;

        fz_try(ctx)
        {
            // Load the specific page for this annotation
            pdf_page *page = pdf_load_page(ctx, pdf, m_pageno);
            if (!page)
                fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to load page");

            pdf_annot *annot = pdf_first_annot(ctx, page);

            while (annot)
            {
                pdf_obj *obj = pdf_annot_obj(ctx, annot);
                if (pdf_to_num(ctx, obj) == m_objNum)
                {
                    pdf_delete_annot(ctx, page, annot);
                    pdf_update_page(ctx, page);
                    break;
                }
                annot = pdf_next_annot(ctx, annot);
            }

            // Drop the page we loaded (not the Model's internal page)
            fz_drop_page(ctx, (fz_page *)page);
        }
        fz_catch(ctx)
        {
            qWarning() << "Undo failed:" << fz_caught_message(ctx);
        }

        // Request a re-render of this page
        emit m_model->pageRenderRequested(m_pageno, true);
    }

    void redo() override
    {
        if (!m_model)
            return;

        fz_context *ctx   = m_model->context();
        fz_document *doc  = m_model->document();
        pdf_document *pdf = pdf_specifics(ctx, doc);

        if (!pdf)
            return;

        fz_try(ctx)
        {
            // Load the specific page for this annotation
            pdf_page *page = pdf_load_page(ctx, pdf, m_pageno);
            if (!page)
                fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to load page");

            pdf_annot *annot = pdf_create_annot(ctx, page, PDF_ANNOT_SQUARE);
            if (!annot)
            {
                fz_drop_page(ctx, (fz_page *)page);
                fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create annotation");
            }

            pdf_set_annot_rect(ctx, annot, m_rect);
            pdf_set_annot_interior_color(ctx, annot, 3, m_color);
            pdf_set_annot_opacity(ctx, annot, m_color[3]);
            pdf_update_annot(ctx, annot);

            // Store the object number for later undo
            pdf_obj *obj = pdf_annot_obj(ctx, annot);
            m_objNum     = pdf_to_num(ctx, obj);

            // Drop the page we loaded (not the Model's internal page)
            // Note: Don't drop the annot - it's owned by the page
            fz_drop_page(ctx, (fz_page *)page);
        }
        fz_catch(ctx)
        {
            qWarning() << "Redo failed:" << fz_caught_message(ctx);
        }

        // Request a re-render of this page
        emit m_model->pageRenderRequested(m_pageno, true);
    }

private:
    Model *m_model;
    int m_pageno;
    fz_rect m_rect;
    float m_color[4];
    int m_objNum = -1;
};
