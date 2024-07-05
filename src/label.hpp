#include <qt6/QtWidgets/QLabel>
#include <qt6/QtWidgets/QRubberBand>
#include <qt6/QtGui/QMouseEvent>
#include <qt6/QtCore/QObject>
#include <mupdf/fitz.h>

class Dodo;

class Label : public QLabel
{

Q_OBJECT
public:
    Label(QWidget *parent = nullptr);
    ~Label();

signals:
    void HighlightRect(QRect);

private:

    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

    QRubberBand *m_rubber_band = nullptr;
    QPoint m_annot_start_pos, m_annot_end_pos;
};
