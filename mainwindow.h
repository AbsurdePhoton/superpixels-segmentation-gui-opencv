/*#-------------------------------------------------
#
#        OpenCV Superpixels Segmentation
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#                v2 - 2018/09/02
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
#include <QButtonGroup>
#include <QListWidgetItem>

#include <ImageMagick-7/Magick++.h>

using namespace cv;
using namespace cv::ximgproc;
using namespace std;
using namespace Magick;

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

    //// UI
    void InitializeValues();

    //// Tabs
    void on_Tabs_currentChanged(int); // tabs handling

    //// labels
    int GetCurrentLabelNumber(); // label value for use with label_mask
    QListWidgetItem *AddNewLabel(QString newLabel); // add new label - empty string creates a "rename me!" label
    void on_pushButton_label_delete_clicked(); // delete label
    void DeleteAllLabels(bool newLabel); // delete ALL labels and create one new if wanted
    void on_listWidget_labels_currentItemChanged(QListWidgetItem *currentItem); // show current label color when item change
    void on_listWidget_labels_itemChanged(QListWidgetItem *currentItem); // show current label name used
    void on_pushButton_label_add_clicked(); // add one default label
    void on_pushButton_label_hide_clicked(); // set label color to 0 => transparent
    void on_pushButton_label_create_clicked(); // special mode where a new cell can be drawn
    void on_pushButton_label_join_clicked(); // join two or more labels

    //// image export
    void on_pushButton_psd_clicked(); // export to PSD Photoshop image
    void on_pushButton_tif_clicked(); // export to TIFF image
    void SavePSDorTIF(std::string type); // save image in PSD or TIFF format with background = image and layers = labels

    //// load & save
    void on_button_image_clicked(); // load image

    void on_button_save_session_clicked(); // save and load session
    void on_button_load_session_clicked();

    void on_button_save_conf_clicked(); // save and load configuration
    void on_button_load_conf_clicked();

    //// Undo
    void SaveUndo(); // save current masks for undo
    void on_button_undo_clicked(); // undo the last action

    void on_checkBox_mask_clicked(); // toggle mask view
    void on_checkBox_image_clicked(); // toggle image view
    void on_checkBox_grid_clicked(); // toggle grid view
    void on_horizontalSlider_blend_mask_valueChanged(); // mask transparency
    void on_horizontalSlider_blend_image_valueChanged(); // image transparency
    void on_pushButton_zoom_minus_clicked(); // levels of zoom
    void on_pushButton_zoom_plus_clicked();
    void on_pushButton_zoom_fit_clicked();
    void on_pushButton_zoom_100_clicked();
    void on_comboBox_grid_color_currentIndexChanged(int); // change grid color
    void on_comboBox_algorithm_currentIndexChanged(int); // hide and reveal algorithms parameters
    void on_horizontalScrollBar_segmentation_valueChanged(); // scroll the viewport
    void on_verticalScrollBar_segmentation_valueChanged();

    void ShowCurrentColor(int red, int green, int blue); // set and show color for current label
    void on_pushButton_color_pick_clicked(); // pick custom color for current label
    void on_pushButton_color_red_clicked(); // choose color for current label
    void on_pushButton_color_green_clicked();
    void on_pushButton_color_blue_clicked();
    void on_pushButton_color_cyan_clicked();
    void on_pushButton_color_magenta_clicked();
    void on_pushButton_color_yellow_clicked();
    void on_pushButton_color_orange_clicked();
    void on_pushButton_color_brown_clicked();
    void on_pushButton_color_olive_clicked();
    void on_pushButton_color_navy_clicked();
    void on_pushButton_color_emerald_clicked();
    void on_pushButton_color_purple_clicked();
    void on_pushButton_color_lime_clicked();
    void on_pushButton_color_rose_clicked();
    void on_pushButton_color_violet_clicked();
    void on_pushButton_color_azure_clicked();
    void on_pushButton_color_malibu_clicked();
    void on_pushButton_color_laurel_clicked();

    //// Image tab
    void on_button_apply_clicked(); // apply corrections and filters on main image
    void on_button_original_clicked(); // revert to original image

    //// Segmentation tab
    void on_button_compute_clicked(); // compute segmentation mask and grid

    //// Keyboard & mouse events
    void keyPressEvent(QKeyEvent *keyEvent); // for the create cell mode
    void keyReleaseEvent(QKeyEvent *keyEvent);

    void mouseReleaseEvent(QMouseEvent *eventRelease); // when the mouse button is released
    void mousePressEvent(QMouseEvent *eventPress); // mouse events = zoom, set cell color etc
    void mouseMoveEvent(QMouseEvent *eventPress);
    void wheelEvent(QWheelEvent *wheelEvent);

    //// Display
    void ShowSegmentation(); // display image in viewport with grid and mask
    void DisplayThumbnail(); // display thumbnail view

    void WhichCell(int posX, int posY); // find and display which cell has been clicked

    void ZoomPlus(); // zoom in
    void ZoomMinus(); // zoom out
    void ShowZoomValue(); // display current zoom level

    void SetViewportXY(int x, int y); // change the origin of the viewport
    void UpdateViewportDimensions(); // calculate width and height of the viewport
    cv::Point Viewport2Image(cv::Point p); // calculate coordinates in the image from the viewport

private:
    // the UI object, to access the UI elements created with Qt Designer
    Ui::MainWindow *ui;

    Ptr<SuperpixelSLIC> slic; // SLIC Segmentation pointer
    Ptr<SuperpixelLSC> lsc; // LSC Segmentation pointer
    Ptr<SuperpixelSEEDS> seeds; // SEEDS Segmentation pointer

    cv::Mat labels, labels_mask, undo_labels; // Segmentation cells and labels
    int maxLabels; // max number of labels
    int nb_cells; // number of labels in segmentation

    cv::Mat image, image_backup, // main image
            thumbnail, // thumbnail of main image
            mask, undo_mask, // painted cells
            grid, // grid mask
            create_cell_labels_save, create_cell_mask_save, mask_line_save; // used by cell paint

    cv::Mat disp_color; // Processed image display with mask and grid
    cv::Rect viewport; // part of the segmentation image to display

    Vec3b color, gridColor; // current colors used

    std::string basefile, basedir; // main image filename: directory and filename without extension

    Qt::MouseButton mouseButton; // save mouse button value when holding down a mouse button
    QPoint mouse_origin; // save the mouse position when holding down a mouse button
    cv::Point pos_save; // save the mouse position in image coordinates

    int num_zooms = 22; // number of zoom values
    double zooms[23] = {0, 0.05, 0.0625, 0.0833, 0.125, 0.1667, 0.25, 0.3333, 0.5, 0.6667, // reduced view
                        1, 1.25, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10000}; // zoomed view
    double zoom, oldZoom; // zoom factor for image display
    QString zoom_type; // if zoom changes indicates where it came from: button clic or mouse wheel

    bool loaded, computed; // indicators: image loaded & segmentation computed

};

#endif // MAINWINDOW_H
