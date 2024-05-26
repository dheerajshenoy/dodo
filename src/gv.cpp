#include "gv.hpp"

void gv::wheelEvent(QWheelEvent *e)
{

    if (e->angleDelta().y() > 0)
    {
        scale(scaleFactor, scaleFactor);
    }
    else {
        scale(1/scaleFactor, 1/scaleFactor);
    }
}
