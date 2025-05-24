#include "dodo.hpp"
#include "argparse.hpp"

void init_args(argparse::ArgumentParser &program)
{
    program.add_argument("-v", "--version")
        .help("Show version number")
        .flag();

    program.add_argument("files")
        .remaining();
}

int main (int argc, char *argv[])
{
    argparse::ArgumentParser program("dodo",
                                     __DODO_VERSION,
                                     argparse::default_arguments::all);
    init_args(program);
    program.parse_args(argc, argv);
    QApplication app(argc, argv);
    dodo d;
    d.readArgsParser(program);
    app.exec();
}
