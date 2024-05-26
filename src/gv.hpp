#include <qt6/QtGui/QWheelEvent>
#include <qt6/QtWidgets/QGraphicsView>

class gv : public QGraphicsView
{
    Q_OBJECT

    public:
    gv(QGraphicsScene *scene) : QGraphicsView(scene) {
        setRenderHint(QPainter::Antialiasing);
        setRenderHint(QPainter::SmoothPixmapTransform);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        setDragMode(QGraphicsView::ScrollHandDrag);
    }

protected:
    void wheelEvent(QWheelEvent *e) override;

private:
    double scaleFactor = 1.5;
};
