/*#-------------------------------------------------
#
#        OpenCV Superpixels Segmentation
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#                v0 - 2018/08/11
#
#-------------------------------------------------*/

#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
