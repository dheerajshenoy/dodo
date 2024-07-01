#include <qt6/QtCore/QDebug>
#include <qt6/QtGui/QShortcut>
#include <qt6/QtGui/QKeySequence>
#include <qt6/QtWidgets/QApplication>
#include <qt6/QtWidgets/QFileDialog>
#include <qt6/QtWidgets/QMainWindow>
#include <qt6/QtWidgets/QVBoxLayout>
#include <qt6/QtWidgets/QLabel>
#include <qt6/QtGui/QRgb>
#include <qt6/QtGui/QImage>
#include <qt6/QtGui/QColor>
#include <qt6/QtWidgets/QScrollArea>
#include <qt6/QtWidgets/QScrollBar>
#include <string>
#include <mupdf/fitz.h>
#include <mupdf/fitz/geometry.h>
#include "statusbar.hpp"

class Dodo : public QMainWindow {

public:
    Dodo(int argc, char **argv, QWidget *parent = nullptr);
    ~Dodo();

    void FitToWidth();
    void FitToHeight();
    void FitToWindow();
    void GotoPage(int pinterval);
    bool INIT_PDF();
    bool Open(QString filename, int page_number = 0);
    void Render();
    void Rotate(float angle);
    void ResetView();
    void SetKeyBinds();
    void ScrollVertical(int direction, int amount);
    void ScrollHorizontal(int direction, int amount);
    void Zoom(float rate);
    void ZoomReset();
    
private:

    fz_context *m_ctx;
    fz_document *m_doc;
    fz_pixmap *m_pix;
    fz_page *m_page;
    fz_device *m_dev;
    int m_page_number = 1, m_page_count = -1, m_scroll_len = 10;
    float m_zoom = 100.0f, m_rotate = 0.0f;
    QString m_filename;
    fz_matrix m_ctm = fz_identity;
    int m_x, m_y;
    QImage m_image;
    QScrollArea *m_scrollarea = new QScrollArea();
    QScrollBar* m_vscroll = m_scrollarea->verticalScrollBar();
    QScrollBar* m_hscroll = m_scrollarea->horizontalScrollBar();
    QLabel *m_label = new QLabel();
    QVBoxLayout *m_layout = new QVBoxLayout();
    QWidget *m_widget = new QWidget();
    StatusBar *m_statusbar = new StatusBar();
};
