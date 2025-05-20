#pragma once

#include <QString>
#include <mupdf/fitz.h>

class QImage;
class QGraphicsScene;

class Model
{

public:
    Model(QGraphicsScene *scene);
    ~Model();
    bool openFile(const QString &fileName);
    bool valid();
    inline void setDPI(float dpi) { m_dpi = dpi; }
    inline void setLowDPI(float low_dpi) { m_low_dpi = low_dpi; }
    int numPages();
    QImage renderPage(int pageno, bool lowQuality);
    void renderLinks(int pageno);
    void setLinkBoundaryBox(bool state);
    void searchAll(const QString &term);

private:
    float m_dpi, m_low_dpi;
    bool m_link_boundary_enabled;

    QGraphicsScene *m_scene { nullptr };
    fz_context *m_ctx { nullptr };
    fz_document *m_doc { nullptr };

};
