#pragma once

#include "BrowseLinkItem.hpp"
#include "HighlightItem.hpp"
#include "LinkHint.hpp"
#include "OutlineWidget.hpp"
#include "RenderTask.hpp"

#include <QFuture>
#include <QImage>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
// #include <mutex>

#define CSTR(x) x.toStdString().c_str()

class QImage;
class QGraphicsScene;

QString
generateHint(int index) noexcept;
// void
// lock_mutex(void *user, int lock);
// void
// unlock_mutex(void *user, int lock);
fz_quad
union_quad(const fz_quad &a, const fz_quad &b);

class Model : public QObject
{
    Q_OBJECT
public:
    Model(QGraphicsScene *scene, QObject *parent = nullptr);
    ~Model();
    explicit operator bool() const noexcept
    {
        return m_doc != nullptr;
    }

    bool authenticate(const QString &pwd) noexcept;
    bool passwordRequired() noexcept;
    bool openFile(const QString &fileName);
    void closeFile() noexcept;
    bool valid();
    inline void setDPI(float dpi)
    {
        m_dpi = dpi;
    }
    int numPages();
    QPixmap renderPage(int pageno, float zoom, float rotation) noexcept;
    QList<BrowseLinkItem *> getLinks() noexcept;
    pdf_annot *get_annot_by_index(int index) noexcept;
    QList<pdf_annot *> get_annots_by_indexes(const QSet<int> &index) noexcept;
    void annotDeleteRequested(int index) noexcept;
    QList<HighlightItem *> getAnnotations() noexcept;
    void setLinkBoundary(bool state);
    void searchAll(const QString &term, bool caseSensitive);
    void addRectAnnotation(const QRectF &rect) noexcept;
    bool save() noexcept;
    bool saveAs(const char *filename) noexcept;
    fz_rect convertToMuPdfRect(const QRectF &qtRect) noexcept;
    bool hasUnsavedChanges() noexcept;
    void enableICC() noexcept;
    void setAntialiasingBits(int bits) noexcept;
    QList<QPair<QString, QString>> extractPDFProperties() noexcept;
    inline void setDPR(float dpr) noexcept
    {
        m_dpr     = dpr;
        m_inv_dpr = 1 / dpr;
    }
    fz_context *clonedContext() noexcept;
    void annotHighlightSelection(const QPointF &selectionStart, const QPointF &selectionEnd) noexcept;
    QSet<int> getAnnotationsInArea(const QRectF &area) noexcept;
    inline fz_context *context() noexcept
    {
        return m_ctx;
    }
    inline void setSelectionColor(const QRgb &color) noexcept
    {
        m_selection_color = color;
    }

    inline QColor highlightColor() noexcept {
        return QColor(m_highlight_color[0],
                m_highlight_color[1],
                m_highlight_color[2],
                m_highlight_color[3]);
    }

    inline void setHighlightColor(const QColor &color) noexcept
    {
        m_highlight_color[0] = color.redF();
        m_highlight_color[1] = color.greenF();
        m_highlight_color[2] = color.blueF();
        m_highlight_color[3] = color.alphaF();
    }

    inline void setAnnotRectColor(const QColor &color) noexcept
    {
        m_annot_rect_color[0] = color.redF();
        m_annot_rect_color[1] = color.greenF();
        m_annot_rect_color[2] = color.blueF();
        m_annot_rect_color[3] = color.alphaF();
    }

    enum class MessageType
    {
        INFO = 0,
        WARNING,
        CRITICAL,
    };

    struct LinkInfo
    {
        QString uri;
        fz_link_dest dest;
    };

    struct SearchResult
    {
        int page;
        fz_quad quad; // Raw text quad from MuPDF, in page coordinates
        int index;
    };

    QString selectAllText(const QPointF &start, const QPointF &end) noexcept;
    void initSaveOptions() noexcept;
    fz_outline *getOutline() noexcept;
    inline fz_matrix transform() noexcept
    {
        return m_transform;
    }
    void followLink(const LinkInfo &info) noexcept;
    void loadColorProfile(const QString &profileName) noexcept;
    inline bool invertColor() noexcept
    {
        return m_invert_color_mode;
    }
    void toggleInvertColor() noexcept;
    inline float width() noexcept
    {
        return m_width;
    }
    inline float height() noexcept
    {
        return m_height;
    }
    inline fz_link_dest resolveLink(const char* URI) noexcept { return fz_resolve_link_dest(m_ctx, m_doc, URI); }

    void selectWord(const QPointF &loc) noexcept;
    void selectLine(const QPointF &loc) noexcept;
    void highlightHelper(const QPointF &selectionStart, const QPointF &selectionEnd, fz_point &a, fz_point &b) noexcept;
    void highlightTextSelection(const QPointF &selectionStart, const QPointF &selectionEnd) noexcept;
    void highlightQuad(fz_quad quad) noexcept;
    QString getSelectionText(const QPointF &selectionStart, const QPointF &selectionEnd) noexcept;
    void deleteAnnots(const QSet<int> &indices) noexcept;
    void annotChangeColorForIndexes(const QSet<int> &indexes, const QColor &color) noexcept;
    void annotChangeColorForIndex(const int index, const QColor &color) noexcept;
    fz_point mapToPdf(QPointF loc) noexcept;
    bool isBreak(int c) noexcept;

signals:
    void jumpToPageRequested(int pageno);
    void jumpToLocationRequested(int pageno, const BrowseLinkItem::Location &loc);
    void imageRenderRequested(int pageno, QImage img);
    void searchResultsReady(const QMap<int, QList<SearchResult>> &results, int matchCount);
    void horizontalFitRequested(int pageno, const BrowseLinkItem::Location &location);
    void verticalFitRequested(int pageno, const BrowseLinkItem::Location &location);
    void fitRectRequested(int pageno, float x, float y, float w, float h);
    void fitXYZRequested(int pageno, float x, float y, float zoom);
    void showMessageRequested(const char *message, MessageType type);
    void documentSaved();

private:
    void apply_night_mode(fz_pixmap *pixmap) noexcept;
    void reloadDocument() noexcept;

    QList<SearchResult> searchHelper(int pageno, const QString &term, bool caseSensitive);

    fz_colorspace *m_colorspace;

    QString m_filename;
    int m_match_count{0};

    // std::mutex m_locks[FZ_LOCK_MAX];
    float m_dpi;
    bool m_link_boundary, m_invert_color_mode{false};
    QGraphicsScene *m_scene{nullptr};

    fz_context *m_ctx{nullptr};
    fz_document *m_doc{nullptr};
    fz_page *m_page{nullptr};
    pdf_page *m_pdfpage{nullptr};
    fz_stext_page *m_text_page{nullptr};
    pdf_document *m_pdfdoc{nullptr};
    pdf_write_options m_write_opts;
    fz_matrix m_transform;
    int m_pageno{-1};

    float m_height, m_width;
    float m_dpr{1.0f}, m_inv_dpr{1.0f};
    float m_highlight_color[4], m_annot_rect_color[4];
    QRgb m_selection_color;
    float m_page_height{0.0f};

};
