#include <mupdf/fitz.h>
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
#include <qt6/QtWidgets/QGraphicsItem>
#include <qt6/QtWidgets/QGraphicsScene>
#include <string>
#include "gv.cpp"

class Dodo : public QMainWindow {

public:
    Dodo(int argc, char **argv, QWidget *parent = nullptr);
    ~Dodo();
    bool Open(QString filename, int page_number = 0);
    bool INIT_PDF();
    void SetKeyBinds();
    void Zoom(float rate);
    void ZoomReset();
    void Render();
    void GotoPage(int pinterval);
    
private:

	fz_context *m_ctx;
	fz_document *m_doc;
	fz_pixmap *m_pix;
    fz_page *m_page;
	int m_page_number = 1, m_page_count;
	float m_zoom = 100.0f, m_rotate = 0.0f;
    std::string m_filename = "/home/neo/test.pdf";
	fz_matrix m_ctm;
	int m_x, m_y;
    QGraphicsScene *m_gscene = new QGraphicsScene();
    QImage m_image;
    gv *m_gview = new gv(m_gscene);

    QLabel *m_label = new QLabel();

    QVBoxLayout *m_layout = new QVBoxLayout();
    QWidget *m_widget = new QWidget();
};
