
#include "argparse.hpp"
#include "dodo.hpp"
#include <sys/resource.h>

void
init_args(argparse::ArgumentParser &program)
{
    program.add_argument("-v", "--version").help("Show version number").flag();

    program.add_argument("-p", "--page").help("Page number to go to").scan<'i', int>().default_value(-1);

    program.add_argument("--synctex-forward")
        .help("Format: --synctex-forward={pdf-file-path}#{src-file-path}:{line}:{column}")
        .default_value(std::string{});

    program.add_argument("files").remaining();
}

int
main(int argc, char *argv[])
{
    argparse::ArgumentParser program("dodo", __DODO_VERSION, argparse::default_arguments::all);
    init_args(program);
    try
    {
        program.parse_args(argc, argv);
    }
    catch (const std::exception &e)
    {
        qDebug() << e.what();
    }

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/resources/dodo2.svg"));
    dodo d;
    d.readArgsParser(program);
    app.exec();
}
