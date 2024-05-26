#include <mupdf/fitz.h>
#include <qt6/QtCore/QDebug>
#include <qt6/QtWidgets/QApplication>
#include <qt6/QtWidgets/QMainWindow>
#include <qt6/QtWidgets/QVBoxLayout>
#include <qt6/QtWidgets/QLabel>
#include <qt6/QtGui/QRgb>
#include <qt6/QtGui/QImage>
#include <qt6/QtGui/QColor>
#include <string>

class Dodo : public QMainWindow {

public:
    Dodo(QWidget *parent = nullptr);
    ~Dodo();
    int INIT_PDF();

private:

	fz_context *ctx;
	fz_document *doc;
	fz_pixmap *pix;
	int page_number = 1, page_count;
	float zoom = 400.0f, rotate = 0.0f;
    std::string input = "/home/neo/test.pdf";
	fz_matrix ctm;
	int x, y;

    QLabel *m_label = new QLabel();
    QVBoxLayout *m_layout = new QVBoxLayout();
    QWidget *m_widget = new QWidget();
};
