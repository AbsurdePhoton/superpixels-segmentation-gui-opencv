/*#-------------------------------------------------
#
#        OpenCV Superpixels Segmentation
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#                v2.1 - 2019/05/12
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
