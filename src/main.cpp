#include "dodo.cpp"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Dodo d(argc, argv);

    app.exec();
}
