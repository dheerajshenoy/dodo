#pragma once

// This class represents a command to add a text highlight annotation to a PDF
// document using the MuPDF library. It supports undo and redo functionality by
// inheriting from QUndoCommand. The command stores the necessary information to
// create and remove the annotation, including the page number, quad points,
// color, and object numbers.

#include "../Model.hpp"

#include <QUndoCommand>
#include <QVector>
#include <mupdf/pdf.h>

class TextHighlightAnnotationCommand : public QUndoCommand
{
public:
    TextHighlightAnnotationCommand(Model *model, int pageno,
                                   const QVector<fz_quad> &quads,
                                   const float color[4],
                                   QUndoCommand *parent = nullptr)
        : QUndoCommand("Add Text Highlight Annotation", parent), m_model(model),
          m_pageno(pageno), m_quads(quads)
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

            // Find and delete the annotation by object number
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
        if (!m_model || m_quads.isEmpty())
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

            // Create a single highlight annotation with all quad points
            pdf_annot *annot = pdf_create_annot(ctx, page, PDF_ANNOT_HIGHLIGHT);
            if (!annot)
            {
                fz_drop_page(ctx, (fz_page *)page);
                fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create annotation");
            }

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
    QVector<fz_quad> m_quads;
    float m_color[4];
    int m_objNum = -1;
};