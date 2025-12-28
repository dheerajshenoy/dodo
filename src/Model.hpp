#pragma once

// Wrapper for MuPDF Model

#include "Annotation.hpp"
#include "BrowseLinkItem.hpp"

#include <QColor>
#include <QPixmap>
#include <QString>
#include <QUndoStack>

extern "C"
{
#include <mupdf/fitz.h>
#include <mupdf/fitz/geometry.h>
#include <mupdf/fitz/image.h>
#include <mupdf/pdf.h>
}

class Model
{

public:
    Model(const QString &filepath) noexcept;
    ~Model() noexcept;

    struct LinkInfo
    {
        QString uri;
        fz_link_dest dest;
    };

    struct EncryptInfo
    {
        QString user_password;
        QString owner_password;
        int perm_flags{0};
        int enc_level{128}; // 40, 128, 256
    };

    inline float zoom() const noexcept
    {
        return m_zoom;
    }

    inline void setZoom(float zoom) noexcept
    {
        m_zoom = zoom;
    }

    inline QString filePath() const noexcept
    {
        return m_filepath;
    }

    inline int numPages() const noexcept
    {
        return m_page_count;
    }

    inline QUndoStack *undoStack() noexcept
    {
        return m_undo_stack;
    }

    inline void setInvertColor(bool invert) noexcept
    {
        m_invert_color = invert;
    }

    inline bool invertColor() const noexcept
    {
        return m_invert_color;
    }

    inline void setDPR(float dpr) noexcept
    {
        m_dpr     = dpr;
        m_inv_dpr = 1.0f / dpr;
    }

    inline float DPR() const noexcept
    {
        return m_dpr;
    }

    inline bool success() const noexcept
    {
        return m_success;
    }

    inline QColor highlightAnnotColor() const noexcept
    {
        return QColor(static_cast<int>(m_highlight_color[0] * 255),
                      static_cast<int>(m_highlight_color[1] * 255),
                      static_cast<int>(m_highlight_color[2] * 255),
                      static_cast<int>(m_highlight_color[3] * 255));
    }

    inline bool hasUnsavedChanges() const noexcept
    {
        if (m_undo_stack)
            return m_undo_stack->isClean() == false;
        return false;
    }

    inline float pageWidthPts() const noexcept
    {
        return m_page_width_pts;
    }

    inline float pageHeightPts() const noexcept
    {
        return m_page_height_pts;
    }

    inline float DPI() const noexcept
    {
        return m_dpi;
    }

    fz_outline *getOutline() noexcept;
    bool followLink(const LinkInfo &info) noexcept;
    bool reloadDocument() noexcept;
    void open() noexcept;
    bool decrypt() noexcept;
    bool encrypt(const EncryptInfo &info) noexcept;
    void setPopupColor(const QColor &color) noexcept;
    void setHighlightColor(const QColor &color) noexcept;
    void setSelectionColor(const QColor &color) noexcept;
    void setAnnotRectColor(const QColor &color) noexcept;
    bool passwordRequired() const noexcept;
    bool authenticate(const QString &password) noexcept;
    bool SaveChanges() noexcept;
    QPixmap renderPageToPixmap(int pageno) noexcept;
    void cachePageDimension() noexcept;
    std::vector<QPolygonF>
    computeTextSelectionQuad(int pageno, const QPointF &start,
                             const QPointF &end) noexcept;
    std::vector<Annotation *> getAnnotations(int pageno) noexcept;
    std::vector<BrowseLinkItem *> getLinks(int pageno) noexcept;
    std::string getSelectedText(int pageno, const fz_point &a,
                                const fz_point &b) const noexcept;
    inline std::pair<fz_point, fz_point> getTextSelectionRange() const noexcept
    {
        return {m_selection_start, m_selection_end};
    }

private:
    QString m_filepath;
    int m_page_count{0};
    float m_dpr{1.25f}, m_dpi{96.0f}, m_zoom{1.0f}, m_rotation{0.0f},
        m_inv_dpr{1.0f};
    bool m_invert_color{false};

    fz_context *m_ctx{nullptr};
    fz_document *m_doc{nullptr};
    pdf_document *m_pdf_doc{nullptr};
    float m_popup_color[4], m_highlight_color[4], m_selection_color[4],
        m_annot_rect_color[4];

    QUndoStack *m_undo_stack{nullptr};
    bool m_success{false};
    fz_colorspace *m_colorspace{nullptr};
    fz_outline *m_outline{nullptr};
    fz_matrix m_transform{fz_identity};
    float m_page_width_pts{0.0f}, m_page_height_pts{0.0f};
    fz_point m_selection_start{}, m_selection_end{};
};

/**
 * @brief Clean up image data when the last copy of the QImage is destoryed.
 */
static inline void
imageCleanupHandler(void *data)
{
    unsigned char *samples = static_cast<unsigned char *>(data);

    if (samples)
    {
        delete[] samples;
    }
}
