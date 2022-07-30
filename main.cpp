#include "qstdcdec.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QStdcdec w;
    w.show();
    return a.exec();
}
