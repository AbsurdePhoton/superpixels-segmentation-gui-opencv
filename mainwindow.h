/*#-------------------------------------------------
#
#        OpenCV Superpixels Segmentation
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#                v1 - 2018/08/22
#
#-------------------------------------------------*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <iostream>

#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include <opencv2/ximgproc.hpp>
#include "opencv2/opencv.hpp"
#include "opencv2/core/utility.hpp"
#include <QMainWindow>
#include <QFileDialog>

using namespace cv;
using namespace cv::ximgproc;
using namespace std;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:

private slots:
    void on_button_image_clicked(); // load image

    void on_button_save_session_clicked(); // save and load session
    void on_button_load_session_clicked();

    void on_button_save_conf_clicked(); // save and load configuration
    void on_button_load_conf_clicked();

    void on_button_undo_clicked(); // undo the last action

    void ShowSegmentation(); // display the image in the viewport with grid and mask

    void on_checkBox_mask_clicked(); // toggle mask view
    void on_checkBox_image_clicked(); // toggle image view
    void on_checkBox_grid_clicked(); // toggle grid view
    void on_horizontalSlider_blend_mask_valueChanged(int value); // mask transparency
    void on_horizontalSlider_blend_image_valueChanged(int value); // image transparency
    void on_pushButton_zoom_minus_clicked(); // levels of zoom
    void on_pushButton_zoom_plus_clicked();
    void on_pushButton_zoom_fit_clicked();
    void on_pushButton_zoom_100_clicked();
    void on_comboBox_grid_color_currentIndexChanged(int index); // change grid color
    void on_comboBox_algorithm_currentIndexChanged(int index); // hide and reveal algorithms parameters
    void on_checkBox_contours_clicked(); // hide and reveal contours parameters
    void on_horizontalScrollBar_segmentation_valueChanged(int value); // scroll the viewport
    void on_verticalScrollBar_segmentation_valueChanged(int value);

    void on_pushButton_color_red_clicked(); // choose cell color
    void on_pushButton_color_green_clicked();
    void on_pushButton_color_blue_clicked();
    void on_pushButton_color_cyan_clicked();
    void on_pushButton_color_magenta_clicked();
    void on_pushButton_color_yellow_clicked();
    void on_pushButton_color_white_clicked();
    void on_pushButton_color_orange_clicked();
    void on_pushButton_color_brown_clicked();
    void on_pushButton_color_olive_clicked();
    void on_pushButton_color_navy_clicked();
    void on_pushButton_color_emerald_clicked();
    void on_pushButton_color_purple_clicked();
    void on_pushButton_color_gray_clicked();

    void on_button_compute_clicked(); // compute segmentation mask and grid

    void mousePressEvent(QMouseEvent *eventPress); // mouse events
    void mouseMoveEvent(QMouseEvent *eventPress);
    void wheelEvent(QWheelEvent *wheelEvent);

private:
    // the UI object, to access the UI elements created with Qt Designer
    Ui::MainWindow *ui;

    Ptr<SuperpixelSLIC> slic; // SLIC Segmentation pointer
    Ptr<SuperpixelLSC> lsc; // LSC Segmentation pointer
    Ptr<SuperpixelSEEDS> seeds; // SEEDS Segmentation pointer

    Mat labels; // Segmentation labels
    int nb_cells; // Number of labels in segmentation
    cv::Mat image, backup, mask, grid, undo, thumbnail; // Images

    cv::Mat disp_color; // Processed segmentation
    cv::Rect viewport; // part of the segmentation image to display

    Vec3b color, gridColor; // current colors used

    std::string basefile, basedir; // filename without the end

    Qt::MouseButton mouseButton; // save mouse button value when holding down a mouse button
    QPoint mouse_origin; // save the mouse position when holding down a mouse button

    int num_zooms = 20; // number of zoom values
    double zooms[21] = {0, 0.05, 0.0625, 0.0833, 0.125, 0.1667, 0.25, 0.3333, 0.5, 0.6667, 1, 1.25, 1.5, 2, 3, 4, 5, 6, 7, 8, 1000};
    double zoom, oldZoom; // zoom factor for image display
    QString zoom_type;

    bool computed; // true when segmentation computed

    void DisplayThumbnail(); // update the thumbnail view
    void UpdateViewportDimensions(); // calculate width and height of the viewport
    void WhichCell(int posX, int posY); // find and display which cell has been clicked
    void ZoomPlus(); // zoom in
    void ZoomMinus(); // zoom out
    void ShowZoomValue(); // display current zoom level
    void SetViewportXY(int x, int y); // change the origin of the viewport
    Point Viewport2Image(Point p); // calculate coordinates in the image from the viewport

};

#endif // MAINWINDOW_H
