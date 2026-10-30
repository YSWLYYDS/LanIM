#include "mainwindow.h"

#include <QApplication>
#include "logging.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("LanIMClient");
    Logging::install();
    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
