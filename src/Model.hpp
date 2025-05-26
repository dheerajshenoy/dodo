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
#include <QImage>
#include "HighlightItem.hpp"

#define CSTR(x) x.toStdString().c_str()

class QImage;
class QGraphicsScene;

QString generateHint(int index) noexcept;
void lock_mutex(void* user, int lock);
void unlock_mutex(void* user, int lock);
fz_quad union_quad(const fz_quad& a, const fz_quad& b);

class Model : public QObject
{
    Q_OBJECT
    public:

        Model(QGraphicsScene *scene);
        ~Model();
        explicit operator bool() const noexcept {
            return m_doc != nullptr;
        }

        bool authenticate(const QString &pwd) noexcept;
        bool passwordRequired() noexcept;
        bool openFile(const QString &fileName);
        void closeFile() noexcept;
        bool valid();
        inline void setDPI(float dpi) { m_dpi = dpi; }
        inline void setLowDPI(float low_dpi) { m_low_dpi = low_dpi; }
        int numPages();
        QPixmap renderPage(int pageno, float zoom, float rotation, bool renderonly = false) noexcept;
        QList<BrowseLinkItem*> getLinks() noexcept;
        pdf_annot* get_annot_by_index(int index) noexcept;
        void annotDeleteRequested(int index) noexcept;
        QList<HighlightItem*> getAnnotations() noexcept;
        void setLinkBoundary(bool state);
        void searchAll(const QString &term, bool caseSensitive);
        void addRectAnnotation(const QRectF &rect) noexcept;
        bool save() noexcept;
        fz_rect convertToMuPdfRect(const QRectF &qtRect) noexcept;
        bool hasUnsavedChanges() noexcept;
        void enableICC() noexcept;
        void setAntialiasingBits(int bits) noexcept;
        QList<QPair<QString, QString>> extractPDFProperties() noexcept;
        inline void setDPR(float dpr) noexcept { m_dpr = dpr; m_inv_dpr = 1 / dpr; }
        fz_context* clonedContext() noexcept;
        void annotHighlightSelection(const QPointF &selectionStart, const QPointF &selectionEnd) noexcept;
        QSet<int> getAnnotationsInArea(const QRectF &area) noexcept;
        inline fz_context* context() noexcept { return m_ctx; }
        inline void setSelectionColor(const QRgb &color) noexcept {
            m_selection_color = color;
        }

        inline void setHighlightColor(const QColor &color) noexcept {
            m_highlight_color[0] = color.redF();
            m_highlight_color[1] = color.greenF();
            m_highlight_color[2] = color.blueF();
            m_highlight_color[3] = color.alphaF();
        }

        struct LinkInfo {
            QString uri;
            fz_link_dest dest;
        };

        struct SearchResult {
            int page;
            fz_quad quad; // Raw text quad from MuPDF, in page coordinates
            int index;
        };

        QString selectAllText(const QPointF &start, const QPointF &end) noexcept;
        void initSaveOptions() noexcept;
        fz_outline* getOutline() noexcept;
        inline fz_matrix transform() noexcept { return m_transform; }
        void visitLinkKB(int pageno, float zoom) noexcept;
        void copyLinkKB(int pageno) noexcept;
        QMap<QString, LinkInfo> hintToLinkMap() {
            return m_hint_to_link_map;
        };
        void clearKBHintsOverlay() noexcept;
        void followLink(const LinkInfo &info) noexcept;
        void loadColorProfile(const QString &profileName) noexcept;
        void invertColor() noexcept;
        inline float width() noexcept { return m_width; }
        inline float height() noexcept { return m_height; }
        inline void setLinkHintForeground(int fg) noexcept { m_link_hint_fg = fg; }
        inline void setLinkHintBackground(int bg) noexcept { m_link_hint_bg = bg; }

        void highlightHelper(const QPointF &selectionStart, const QPointF &selectionEnd,
                fz_point &a, fz_point &b) noexcept;
        void highlightTextSelection(const QPointF &selectionStart,
                const QPointF &selectionEnd) noexcept;
        QString getSelectionText(const QPointF &selectionStart, const QPointF &selectionEnd) noexcept;

signals:
        void jumpToPageRequested(int pageno);
        void jumpToLocationRequested(int pageno, const BrowseLinkItem::Location &loc);
        void imageRenderRequested(int pageno, QImage img);
        void searchResultsReady(const QMap<int, QList<SearchResult>> &results, int matchCount);
        void horizontalFitRequested(int pageno, const BrowseLinkItem::Location &location);
        void verticalFitRequested(int pageno, const BrowseLinkItem::Location &location);
        void fitRectRequested(int pageno, float x, float y, float w, float h);
        void fitXYZRequested(int pageno, float x, float y, float zoom);

    private:
        void apply_night_mode(fz_pixmap* pixmap) noexcept;

        QList<SearchResult> searchHelper(int pageno, const QString &term, bool caseSensitive);

        fz_colorspace *m_colorspace;

        QString m_filename;
        int m_match_count { 0 };

        std::mutex m_locks[FZ_LOCK_MAX];
        float m_dpi, m_low_dpi;
        bool m_link_boundary, m_invert_color_mode { false };
        QGraphicsScene *m_scene { nullptr };
        fz_context *m_ctx { nullptr };
        fz_document *m_doc { nullptr };
        fz_page *m_page { nullptr };
        pdf_page *m_pdfpage { nullptr };
        fz_stext_page *m_text_page { nullptr };
        fz_matrix m_transform;
        float m_height, m_width;
        float m_dpr { 1.0f }, m_inv_dpr { 1.0f };
        int m_link_hint_fg = 0x000000, m_link_hint_bg = 0xFFFF00;
        pdf_write_options m_write_opts;
        pdf_document *m_pdfdoc { nullptr };
        float m_highlight_color[4];
        QRgb m_selection_color;

        QMap<QString, LinkInfo> m_hint_to_link_map;
};
