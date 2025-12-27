#pragma once

// This class represents a command to add a text highlight annotation to a PDF
// document using the MuPDF library. It supports undo and redo functionality by
// inheriting from QUndoCommand. The command stores the necessary information to
// create and remove the annotation, including the page number, quad points,
// color, and object numbers. Each quad creates a separate annotation to avoid
// visual issues with multi-line highlights.

#include "../Model.hpp"

#include <QUndoCommand>
#include <vector>

extern "C"
{
#include <mupdf/pdf.h>
}

class TextHighlightAnnotationCommand : public QUndoCommand
{
public:
    TextHighlightAnnotationCommand(Model *model, int pageno,
                                   const std::vector<fz_quad> &quads,
                                   const float color[4],
                                   QUndoCommand *parent = nullptr)
        : QUndoCommand("Add Text Highlight Annotation", parent), m_model(model),
          m_pageno(pageno), m_quads(quads)
    {
        std::copy(color, color + 4, m_color);
    }

    void undo() override
    {
        if (!m_model || m_objNums.empty())
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

            // Delete all annotations that were created by this command
            // We need to delete them one at a time, re-finding each by objNum
            // since the list changes after each deletion
            for (int objNum : m_objNums)
            {
                pdf_annot *annot = pdf_first_annot(ctx, page);
                while (annot)
                {
                    pdf_obj *obj = pdf_annot_obj(ctx, annot);
                    if (pdf_to_num(ctx, obj) == objNum)
                    {
                        pdf_delete_annot(ctx, page, annot);
                        pdf_update_page(ctx, page);
                        break;
                    }
                    annot = pdf_next_annot(ctx, annot);
                }
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
        if (!m_model || m_quads.empty())
            return;

        fz_context *ctx   = m_model->context();
        fz_document *doc  = m_model->document();
        pdf_document *pdf = pdf_specifics(ctx, doc);

        if (!pdf)
            return;

        m_objNums.clear();

        fz_try(ctx)
        {
            // Load the specific page for this annotation
            pdf_page *page = pdf_load_page(ctx, pdf, m_pageno);
            if (!page)
                fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to load page");

            // Create a separate highlight annotation for each quad
            // This looks better visually for multi-line selections
            pdf_annot *annot = pdf_create_annot(ctx, page, PDF_ANNOT_HIGHLIGHT);
            if (!annot)
                continue;

            pdf_set_annot_quad_points(ctx, annot, m_quads.size(), &m_quads[0]);
            pdf_set_annot_color(ctx, annot, 3, m_color);
            pdf_set_annot_opacity(ctx, annot, m_color[3]);
            pdf_update_annot(ctx, annot);

            // Store the object number for later undo
            pdf_obj *obj = pdf_annot_obj(ctx, annot);
            m_objNums.push_back(pdf_to_num(ctx, obj));

            pdf_drop_annot(ctx, annot);

            // Drop the page we loaded (not the Model's internal page)
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
    std::vector<fz_quad> m_quads;
    float m_color[4];
    std::vector<int> m_objNums;
};
