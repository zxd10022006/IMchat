#include <QApplication>
#include "kernel.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Kernel::instance()->openTcpNet();
    return QApplication::exec();
}
