#-------------------------------------------------
#
#        OpenCV Superpixels Segmentation
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#                v2.3 - 2019/07/08
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = superpixels-segmentation-opencv
TEMPLATE = app

INCLUDEPATH += /usr/local/include/opencv2
INCLUDEPATH += /usr/local/include/ImageMagick-7

LIBS += -L/usr/local/lib -lopencv_core -lopencv_imgcodecs -lopencv_highgui -ltiff
LIBS += $(shell Magick++-config --ldflags --libs)

SOURCES +=  main.cpp\
            mainwindow.cpp \
            mat-image-tools.cpp \

HEADERS  += mainwindow.h \
            mat-image-tools.h

FORMS    += mainwindow.ui

# we add the package opencv to pkg-config
CONFIG += link_pkgconfig
PKGCONFIG += opencv4

QMAKE_CXXFLAGS += -std=c++11
QMAKE_CXXFLAGS += $(shell Magick++-config --cppflags --cxxflags)

# icons
RESOURCES += resources.qrc
