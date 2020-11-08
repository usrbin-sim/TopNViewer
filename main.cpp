#include "mainwindow.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <unistd.h>
#include <sys/syscall.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    w.show();
    return a.exec();
}
