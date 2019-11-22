/*#-------------------------------------------------
#
#        OpenCV Superpixels Segmentation
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#                v2.3 - 2019/07/08
#
#-------------------------------------------------*/

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QMouseEvent>
#include <QScrollBar>
#include <QCursor>
#include <QColorDialog>

//#include <opencv2/ximgproc.hpp>

#include <ImageMagick-7/Magick++.h>

#include "mat-image-tools.h"

using namespace cv;
using namespace cv::ximgproc;
using namespace std;
using namespace Magick;

/////////////////// Window init //////////////////////

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // window
    setWindowFlags((((windowFlags() | Qt::CustomizeWindowHint)
                            & ~Qt::WindowCloseButtonHint) | Qt::WindowMinMaxButtonsHint)); // don't show buttons in title bar
    this->setWindowState(Qt::WindowMaximized); // maximize window
    setFocusPolicy(Qt::StrongFocus); // catch keyboard and mouse in priority

    // Populate combo lists
    ui->comboBox_algorithm->blockSignals(true); // don't trigger the automatic actions for these widgets
    ui->comboBox_algorithm->addItem(tr("SLIC")); // algorithms
    ui->comboBox_algorithm->addItem(tr("SLICO"));
    ui->comboBox_algorithm->addItem(tr("MSLIC"));
    ui->comboBox_algorithm->addItem(tr("LSC"));
    ui->comboBox_algorithm->addItem(tr("SEEDS"));
    ui->comboBox_algorithm->blockSignals(false);

    ui->comboBox_grid_color->blockSignals(true);
    ui->comboBox_grid_color->addItem(tr("Red")); // grid colors
    ui->comboBox_grid_color->addItem(tr("Green"));
    ui->comboBox_grid_color->addItem(tr("Blue"));
    ui->comboBox_grid_color->addItem(tr("Cyan"));
    ui->comboBox_grid_color->addItem(tr("Magenta"));
    ui->comboBox_grid_color->addItem(tr("Yellow"));
    ui->comboBox_grid_color->addItem(tr("White"));
    ui->comboBox_grid_color->blockSignals(false);

    ui->comboBox_algorithm->setCurrentIndex(4); // SLIC

    InitializeValues(); // init all indicators and zoom etc

    basedirinifile = QDir::currentPath().toUtf8().constData();
    basedirinifile += "/dir.ini";
    cv::FileStorage fs(basedirinifile, FileStorage::READ); // open dir ini file
    if (fs.isOpened()) {
        fs["BaseDir"] >> basedir; // load base dir
    }
        else basedir = "/home/"; // base path and file
    basefile = "";
}

MainWindow::~MainWindow()
{
    delete ui;
}

///////////////////      GUI       //////////////////////

void MainWindow::on_button_quit_clicked()
{
    int quit = QMessageBox::question(this, "Quit this wonderful program", "Are you sure you want to quit?", QMessageBox::Yes|QMessageBox::No); // quit, are you sure ?
    if (quit == QMessageBox::No) // don't quit !
        return;

    QCoreApplication::quit();
}

void MainWindow::on_Tabs_currentChanged(int) // when a tab is clicked
{
    if(ui->Tabs->currentIndex()==2) ui->label_segmentation->setCursor(Qt::PointingHandCursor); // labels tab ? => labels view cursor
        else ui->label_segmentation->setCursor(Qt::ArrowCursor); // default cursor
}

void MainWindow::InitializeValues() // initialize all sorts of indicators, zoom, etc
{
    // Hide tabs
    ui->Tabs->setTabEnabled(1, false);
    ui->Tabs->setTabEnabled(2, false);

    // Hide drawing tools
    ui->frame_draw->setVisible(false);

    // LCD
    ui->lcd_cells->setPalette(Qt::red);

    // Global variables init
    loaded = false; // main image loaded ?
    computed = false; // segmentation not yet computed
    color = Vec3b(0,0,255); // pen color
    gridColor = Vec3b(0, 0, 255); // current grid color
    zoom = 1; // init zoom
    oldZoom = 1; // to detect a zoom change
    zoom_type = ""; // "button" or (mouse) "wheel"

    // labels list
    maxLabels = 0; // for new labels

    ui->comboBox_grid_color->setCurrentIndex(0);
}

///////////////////    Labels     //////////////////////

int MainWindow::GetCurrentLabelNumber() // label number for use with labels_mask
{
    return ui->listWidget_labels->currentItem()->data(Qt::UserRole).toInt(); // label number stored in the special field
}

void MainWindow::DeleteAllLabels(bool newLabel) // add a label to the list
{
    maxLabels = 0; // reset labels
    ui->listWidget_labels->blockSignals(true); // the labels list must not trigger an action
    ui->listWidget_labels->clear(); // delete all labels
    ui->listWidget_labels->blockSignals(false); // return to normal
    if (newLabel) AddNewLabel(""); // add one new default label or not
}

void MainWindow::UnselectAllLabels() // unselect all labels in list
{
    for (int n = 0; n < ui->listWidget_labels->count(); n++) // for each label
        ui->listWidget_labels->item(n)->setSelected(false); // unselect it
}

QListWidgetItem* MainWindow::AddNewLabel(QString newLabel) // add a label to the list
{
    UnselectAllLabels(); // the new one will be the only selected

    QListWidgetItem *item = new QListWidgetItem ();
    if (newLabel == "") item->setText("Rename me!"); // default text of the label
        else item->setText(newLabel); // or custom text
    maxLabels++; // increase total number of labels
    item->setData(Qt::UserRole, maxLabels); // custom field = label number
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled); // item enabled, editable and selectable
    item->setBackground(QColor(0, 0, 255)); // label color
    if (IsRGBColorDark(0, 0, 255)) // is the label color dark ?
        item->setForeground(QColor(255, 255, 255)); // if so light text color
        else item->setForeground(QColor(0, 0, 0)); // if not dark text color
    ui->listWidget_labels->addItem(item); // add the item to the list
    ui->listWidget_labels->setCurrentItem(item); // select the new label

    return item; // pointer to new item
}

void MainWindow::on_listWidget_labels_currentItemChanged(QListWidgetItem *currentItem) // show current label color
{
    QBrush brush = currentItem->background(); // the label color is stored in the background color of the list
    ShowCurrentColor(brush.color().red(), brush.color().green(), brush.color().blue()); // show current color used
}

void MainWindow::on_listWidget_labels_itemChanged(QListWidgetItem *currentItem) // show current label text
{
    ui->label_color->setText(currentItem->text()); // show label name
}

void MainWindow::on_pushButton_label_add_clicked() // add new default label
{
    if (!computed) { // if image not computed yet get out
        QMessageBox::warning(this, "No cells found",
                                 "Not now!\n\nBefore anything else, compute the segmentation or load a previous session");
        return;
    }

    AddNewLabel("");
}

void MainWindow::on_pushButton_label_hide_clicked() // hide label = set its color to 0
{
    if (!computed) { // if image not computed yet get out
        QMessageBox::warning(this, "No cells found",
                                 "Not now!\n\nBefore anything else, compute the segmentation or load a previous session");
        return;
    }

    color = Vec3b(0,0,0); // black = transparent
    ShowCurrentColor(color[2], color[1], color[0]); // set black color to label
}

void MainWindow::on_pushButton_label_delete_clicked() // delete the current label
{
    if (!computed) { // if image not computed yet get out
        QMessageBox::warning(this, "No cells found",
                                 "Not now!\n\nBefore anything else, compute the segmentation or load a previous session");
        return;
    }

    if (ui->listWidget_labels->count() == 1) { // last label ?
        QMessageBox::warning(this, "Delete label", "You can't delete the last label in the list");
        return;
    }

    QListWidgetItem *item = ui->listWidget_labels->currentItem();

    int deletion = QMessageBox::question(this, "Delete label", "Are you sure?\n''" + item->text() + "'' label will be deleted", QMessageBox::Yes|QMessageBox::No); // sure ?
    if (deletion == QMessageBox::No) // don't delete it !
        return;

    Mat1b superpixel_mask = labels_mask == GetCurrentLabelNumber(); // extract label to delete from labels mask
    mask.setTo(0, superpixel_mask); // set 0 (= unclaimed) to labels mask

    ShowSegmentation(); // display mask

    int row = ui->listWidget_labels->currentRow(); // current label to delete
    ui->listWidget_labels->removeItemWidget(item); // delete item object
    ui->listWidget_labels->takeItem(row); // delete label from list
}

void MainWindow::on_pushButton_label_join_clicked() // join 2 or more labels, result to one selected label
{
    int join = QMessageBox::question(this, "Join labels", "Are you sure?\n\nSelected labels in the list will be joined", QMessageBox::Yes|QMessageBox::No); // sure ?
    if (join == QMessageBox::No) // join the mask !
        return;

    QList<QListWidgetItem *> items = ui->listWidget_labels->selectedItems(); // get selected items

    if (items.count() < 2) // only one label selected ?
    {
        QMessageBox::warning(this, "Join labels",
                                 "You have to select more than one label to join");
        return;
    }

    QBrush brush = items[0]->background(); // color of first label encountered: this label will contain the result
    Vec3b col = Vec3b(brush.color().red(), brush.color().green(), brush.color().blue()); // get its color
    int id = items[0]->data(Qt::UserRole).toInt(); // and get its label number for labels mask

    for (int i = 1; i < items.count(); i++) { // each item of the list minus the first
        Mat1b superpixel_mask = labels_mask == items[i]->data(Qt::UserRole).toInt(); // label number for labels mask
        labels_mask.setTo(id, superpixel_mask); // claim cells for the first label
        mask.setTo(col, superpixel_mask); // and set its color to the mask

        ui->listWidget_labels->setCurrentItem(items[i]); // select the item
        int row = ui->listWidget_labels->currentRow(); // find its row number in the list
        ui->listWidget_labels->removeItemWidget(items[i]); // delete this item
        ui->listWidget_labels->takeItem(row); // and delete the label from the list
    }
    items[0]->setText(items[0]->text() + " (joined)"); // add "join" to the first label name containing the result

    ShowSegmentation(); // show result
}

void MainWindow::on_pushButton_label_draw_clicked() // special mode to modify the cells (superpixels)
{
    if (!computed) { // if image not computed yet get out
        QMessageBox::warning(this, "No cells found",
                                 "Not now!\n\nBefore anything else, compute the segmentation or load a previous session");
        ui->pushButton_label_draw->setChecked(false); // get out of the special mode
        return;
    }

    if (ui->pushButton_label_draw->isChecked()) { // initialize cell update
        ui->label_segmentation->setCursor(Qt::CrossCursor); // cursor change to show this is a special mode

        ui->frame_pick_colors->setVisible(false); // disable all tabs/elements to keep the user in the special mode
        ui->pushButton_label_add->setVisible(false);
        ui->pushButton_label_delete->setVisible(false);
        ui->pushButton_label_hide->setVisible(false);
        ui->pushButton_label_join->setVisible(false);
        ui->button_load_session->setVisible(false);
        ui->button_save_session->setVisible(false);
        ui->pushButton_psd->setVisible(false);
        ui->pushButton_tif->setVisible(false);
        ui->button_quit->setVisible(false);
        ui->frame_draw->setVisible(true);
        ui->Tabs->setTabEnabled(0, false);
        ui->Tabs->setTabEnabled(1, false);
        ui->listWidget_labels->setEnabled(false);
        ui->checkBox_mask->setChecked(true);
        ui->pushButton_draw_grabcut_iteration->setVisible(false);
        ui->checkBox_selection->setChecked(false);

        mask.copyTo(draw_cell_mask_save); // save all the masks
        labels_mask.copyTo(draw_cell_labels_mask_save);
        labels.copyTo(draw_cell_labels_save);
        grid.copyTo(draw_cell_grid_save);

        pos_save = cv::Point(-1, -1); // no first line point defined
    }
    else { // special mode ended by clicking again the button
        int draw = QMessageBox::question(this, "Update cells", "Are you sure you want to update the cells from the white mask?\nIf not, the mask will be deleted", QMessageBox::Yes|QMessageBox::No); // really update the cells ?

        double min, max;
        minMaxIdx(labels, &min, &max); // find the highest superpixel number
        int maxLabel = max + 1; // superpixel value of new cell

        Mat1b white_mask;
        inRange(mask, Vec3b(255, 255, 255), Vec3b(255, 255, 255), white_mask); // extract white color from mask

        if ((cv::sum(white_mask) != Scalar(0,0,0)) & (draw == QMessageBox::Yes)) { // update the cells
            vector<vector<cv::Point>> contours;
            vector<Vec4i> hierarchy;

            findContours(white_mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE); // find new cell contours

            grid.setTo(0, white_mask); // erase cell in grid mask
            drawContours(grid, contours, -1, gridColor, 1, 8, hierarchy ); // draw contour of new cell in grid

            draw_cell_mask_save.copyTo(mask); // restore mask
            labels.setTo(maxLabel, white_mask); // set new superpixel value in labels
            labels_mask.setTo(GetCurrentLabelNumber(), white_mask); // set current label value in labels mask
            mask.setTo(color, white_mask); // update mask with current label color
        }
        else // don't update the cells !
        {
            if ((cv::sum(white_mask) == Scalar(0,0,0)) & (draw == QMessageBox::Yes))
                QMessageBox::warning(this, "Not updating the cells",
                                     "The mask was empty, cells not updated");

            draw_cell_mask_save.copyTo(mask); // restore original mask

            ShowSegmentation(); // show view back to previous state
        }

        //SaveUndo(); // save current state

        ui->label_segmentation->setCursor(Qt::PointingHandCursor); // cursor back to normal

        ui->frame_pick_colors->setVisible(true); // show again the elements hidden for the special mode
        ui->pushButton_label_add->setVisible(true);
        ui->pushButton_label_delete->setVisible(true);
        ui->pushButton_label_hide->setVisible(true);
        ui->pushButton_label_join->setVisible(true);
        ui->button_load_session->setVisible(true);
        ui->button_save_session->setVisible(true);
        ui->pushButton_psd->setVisible(true);
        ui->pushButton_tif->setVisible(true);
        ui->button_quit->setVisible(true);
        ui->frame_draw->setVisible(false);
        ui->Tabs->setTabEnabled(0, true);
        ui->Tabs->setTabEnabled(1, true);
        ui->listWidget_labels->setEnabled(true);
        ui->checkBox_selection->setChecked(true);

        ShowCurrentColor(color[2], color[1], color[0]); // redraw label
    }
}

void MainWindow::on_pushButton_draw_clear_clicked() // clear the cell drawing mask
{
    SaveUndo(); // save current state

    draw_cell_mask_save.copyTo(mask); // Restore current mask

    ShowSegmentation(); // show view back to previous state
}

void MainWindow::on_pushButton_draw_grabcut_clicked() // use GrabCut in cell drawing mode
{
    SaveUndo(); // save current state

    QApplication::setOverrideCursor(Qt::WaitCursor); // wait cursor

    grabcut_mask = Mat::zeros(image.rows, image.cols, CV_8UC1);
    grabcut_mask = GC_BGD; // init GrabCut with 'maybe Background'
    grabcut_foreground.release(); // clear grabcut internal masks
    grabcut_background.release();

    Mat1b mask_tmp = Mat::zeros(image.rows, image.cols, CV_8UC1); // to find white, red or blue pixels

    cv::inRange(mask, Vec3b(255,255,255), Vec3b(255,255,255), mask_tmp); // find white
    grabcut_mask.setTo(GC_FGD, mask_tmp); // white = foreground
    cv::inRange(mask, Vec3b(0,0,255), Vec3b(0,0,255), mask_tmp); // find red
    grabcut_mask.setTo(GC_BGD, mask_tmp); // red = background
    cv::inRange(mask, Vec3b(255,0,0), Vec3b(255,0,0), mask_tmp); // find blue
    grabcut_mask.setTo(GC_PR_FGD, mask_tmp); // blue = maybe foreground

    cv::grabCut(image,    // input image
                grabcut_mask,   // segmentation result
                cv::Rect(0,0,image.cols,image.rows), // rectangle containing foreground, not used here
                grabcut_background, grabcut_foreground, // internal for GrabCut
                1,        // number of iterations = 1
                cv::GC_INIT_WITH_MASK); // init with mask (not rectangle)

    draw_cell_mask_save.copyTo(mask); // restore mask

    cv::inRange(grabcut_mask, GC_FGD, GC_FGD, mask_tmp); // get 'sure' foreground pixels
    mask.setTo(Vec3b(255,255,255), mask_tmp); // set foreground = white in mask
    cv::inRange(grabcut_mask, GC_PR_FGD, GC_PR_FGD, mask_tmp); // get 'maybe' foreground pixels
    mask.setTo(Vec3b(255,0,0), mask_tmp); // set maybe = blue in mask

    QApplication::restoreOverrideCursor(); // Restore cursor

    ui->pushButton_draw_grabcut_iteration->setVisible(true); // show the new iteration button

    ShowSegmentation(); // show GrabCut result
}

void MainWindow::on_pushButton_draw_grabcut_iteration_clicked() // repeat GrabCut in cell drawing mode
{
    Mat1b mask_tmp; // temp mask

    QApplication::setOverrideCursor(Qt::WaitCursor); // wait cursor

    cv::grabCut(image,    // input image
                grabcut_mask,   // segmentation result
                cv::Rect(0,0,image.cols,image.rows), // rectangle containing foreground, not used here
                grabcut_background, grabcut_foreground, // internal for GrabCut
                1,        // number of iterations = 1
                cv::GC_EVAL); // resume algorithm

    draw_cell_mask_save.copyTo(mask); // restore mask

    cv::inRange(grabcut_mask, GC_FGD, GC_FGD, mask_tmp); // get 'sure' foreground pixels
    mask.setTo(Vec3b(255,255,255), mask_tmp); // set foreground = white in mask
    cv::inRange(grabcut_mask, GC_PR_FGD, GC_PR_FGD, mask_tmp); // get 'maybe' foreground pixels
    mask.setTo(Vec3b(255,0,0), mask_tmp); // set maybe = blue in mask

    QApplication::restoreOverrideCursor(); // Restore cursor

    ShowSegmentation(); // show GrabCut result
}

Vec3b MainWindow::DrawColor() // return drawing color in cell drawing mode
{
    Vec3b draw_color;

    if (ui->radioButton_draw_mask->isChecked()) draw_color = Vec3b(255, 255, 255); // white = keep
    if (ui->radioButton_draw_reject->isChecked()) draw_color = Vec3b(0, 0, 255); // red = reject
    if (ui->radioButton_draw_maybe->isChecked()) draw_color = Vec3b(255, 0, 0); // blue = maybe

    return draw_color;
}

/////////////////// Save and load //////////////////////

void MainWindow::SaveDirBaseFile()
{
    cv::FileStorage fs(basedirinifile, cv::FileStorage::WRITE); // open dir ini file for writing
    fs << "BaseDir" << basedir; // write folder name
    fs.release(); // close file
}

void MainWindow::on_pushButton_psd_clicked() // save image (background) + layers (labels) to PSD Photoshop image
{
    SavePSDorTIF("psd");
}

void MainWindow::on_pushButton_tif_clicked() // save image + layers as pages to multipage TIFF image
{
    SavePSDorTIF("tif");
}

Magick::Image Mat2Magick(const Mat &src) // image conversion from Mat to Magick (only for RGB images)
{
    Magick::Image mgk(Geometry(src.cols, src.rows), "black"); // result image
    mgk.read(src.cols, src.rows, "BGR", Magick::CharPixel, (char*)src.data); // transfer image data from Mat

    return mgk;
}

void MainWindow::SavePSDorTIF(std::string type) // save image + layers to PSD or TIFF image
{
    if (!loaded) { // if image not loaded yet get out
        QMessageBox::warning(this, "Image not loaded",
                                 "Not now!\n\nBefore anything else, load an image");
        return;
    }

    std::string ext = type; // file type
    std::transform(type.begin(), type.end(), type.begin(), ::toupper); // file type uppercase

    QString filename = QFileDialog::getSaveFileName(this, "Save labels to multi-layers image file...", // choose filename
                                                    "./" + QString::fromStdString(basedir + basefile + "-segmentation-labels." + ext),
                                                    QString::fromStdString("*." + ext + " *." + type));

    if (filename.isNull() || filename.isEmpty()) // cancel ?
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor); // wait cursor

    std::string fileImage = filename.toUtf8().constData(); // convert filename to std::string
    if (type == "TIF") type = "TIFF"; // add a F to TIF

    vector<Image> imageList; // layers (labels) list
    Magick::Image bkg; // result

    bkg = Mat2Magick(image); // convert image
    bkg.magick(type); // set layer type to PSD or TIFF
    if (type == "PSD") bkg.compressType(RLECompression); // RLE compression for PSD
        else bkg.compressType(LZWCompression); // and LZW for TIFF
    bkg.density(300); // dpi
    bkg.resolutionUnits(PixelsPerInchResolution); // dpi unit
    bkg.label("Background"); // for Photoshop image is the "background"
    imageList.push_back(bkg); // add image to the list
    if (type == "PSD") imageList.push_back(bkg); // for PSD the first image is the preview, the second is the background

    int nb_count = ui->listWidget_labels->count(); // how many layerss (labels) to save ?
    for (int i = 0; i < nb_count; i++) { // for each label
        QListWidgetItem *item = ui->listWidget_labels->item(i); // this label from the list
        int id = item->data(Qt::UserRole).toInt(); // get its label id for labels mask
        //std::string name = std::to_string(i) + "-" + item->text().toUtf8().constData(); // unique label name
        std::string name = item->text().toUtf8().constData(); // convert label name to std::string
        QBrush brush = item->background(); // label color
        Vec3b col = Vec3b(brush.color().red(), brush.color().green(), brush.color().blue());

        Mat1b superpixel_mask = labels_mask == id; // extract label
        Mat save; // current layer image
        image.copyTo(save); // copy image structure
        save = 0; // set it to 0 (transparent)
        save.setTo(Vec3b(col[2], col[1], col[0]), superpixel_mask); // copy cells to layer

        Magick::Image img(Geometry(image.cols, image.rows), "black"); // Magick current layer
        img = Mat2Magick(save); // convert Mat layer to Magick layer
        if (type == "TIFF") bkg.depth(8); // 8-bit TIFF
        img.transparent(Magick::Color(0,0,0)); // set the transparent color
        if (type == "PSD") img.compressType(RLECompression); // RLE compression for PSD
            else img.compressType(LZWCompression); // and LZW for TIFF
        img.label(name); // set the name (doesn't work for TIFF pages)
        img.density(300); // dpi
        img.resolutionUnits(PixelsPerInchResolution); // dpi unit
        img.magick(type); // set PSD or TIFF type

        imageList.push_back(img); // save the image to the list
    }

    Magick::writeImages(imageList.begin(), imageList.end(), fileImage, true); // Write result to disk

    QApplication::restoreOverrideCursor(); // Restore cursor
}

void MainWindow::on_button_image_clicked() // Load main image
{
    QString filename = QFileDialog::getOpenFileName(this, "Select picture file", QString::fromStdString(basedir), "Images (*.jpg *.JPG *.jpeg *.JPEG *.jp2 *.JP2 *.png *.PNG *.tif *.TIF *.tiff *.TIFF *.bmp *.BMP)"); // image filename
    if (filename.isNull() || filename.isEmpty()) // cancel ?
        return;

    basefile = filename.toUtf8().constData(); // base file name and dir are used after to save other files
    basefile = basefile.substr(0, basefile.size()-4); // strip the file extension
    basedir = basefile;
    size_t found = basefile.find_last_of("/"); // find last directory
    basedir = basedir.substr(0,found) + "/"; // extract file location
    basefile = basefile.substr(found+1); // delete ending slash
    ui->label_filename->setText(filename); // display file name in ui
    SaveDirBaseFile(); // Save current path to ini file

    std::string filename_s = filename.toUtf8().constData(); // convert filename from QString

    cv::Mat mat = cv::imread(filename_s, IMREAD_COLOR); // Load image
    if (mat.empty()) { // problem ?
        QMessageBox::critical(this, "File error",
                              "There was a problem reading the image file");
        return;
    }

    ui->label_thumbnail->setPixmap(QPixmap()); // unset all images in GUI
    ui->label_segmentation->setPixmap(QPixmap());
    ui->label_thumbnail->setPixmap(Mat2QPixmapResized(mat, ui->label_thumbnail->width(), ui->label_thumbnail->height(), true)); // Show thumbnail

    image = mat; // store the image for further use
    mat.copyTo(image_backup); // for undo
    selection = Mat::zeros(image.rows, image.cols, CV_8UC3); // init selection mask

    ui->label_segmentation->setPixmap(QPixmap()); // Delete the depthmap image
    ui->label_segmentation->setText("Segmentation not computed"); // Text in the segmentation area

    ui->horizontalScrollBar_segmentation->setMaximum(0); // update scrollbars
    ui->verticalScrollBar_segmentation->setMaximum(0);
    ui->horizontalScrollBar_segmentation->setValue(0);
    ui->verticalScrollBar_segmentation->setValue(0);

    ui->lcd_cells->setPalette(Qt::red); // LCD count red = not yet computed
    ui->lcd_cells->display(0); // LCD count
    ui->label_image_width->setText(QString::number(image.cols)); // display image dimensions
    ui->label_image_height->setText(QString::number(image.rows));

    InitializeValues(); // reset all variables

    double zoomX = double(ui->label_segmentation->width()) / image.cols; // try vertical and horizontal ratios
    double zoomY = double(ui->label_segmentation->height()) / image.rows;
    if (zoomX < zoomY) zoom = zoomX; // the lowest fit the view
        else zoom = zoomY;
    oldZoom = zoom; // for center view
    ShowZoomValue(); // display current zoom
    viewport = Rect(0, 0, image.cols, image.rows); // update viewport

    cv::resize(image, thumbnail, Size(ui->label_thumbnail->pixmap()->width(),ui->label_thumbnail->pixmap()->height()),
               0, 0, INTER_AREA); // create thumbnail

    mask.release(); // reinit mask
    grid.release(); // reinit grid
    undo_mask.release(); // reinit undo masks
    undo_labels.release();
    selection.release();
    loaded = true; // image loaded successfuly
    computed = false; // segmentation not performed
    DeleteAllLabels(true); // empty a previous labels list
    SaveUndo(); // for undo

    CopyFromImage(image, viewport).copyTo(disp_color); // copy only the viewport part of image
    QPixmap D;
    D = Mat2QPixmapResized(disp_color, int(viewport.width*zoom), int(viewport.height*zoom), true); // zoomed image
    ui->label_segmentation->setPixmap(D); // Set new image content to viewport
    DisplayThumbnail(); // update thumbnail view

    ShowSegmentation(); // show the result

    ui->Tabs->setTabEnabled(1, true); // enable Segmentation and Labels tabs
    ui->Tabs->setTabEnabled(2, true);
}

void MainWindow::on_button_save_session_clicked() // save session files
{
    if (!computed) { // if image not computed yet get out
        QMessageBox::warning(this, "No cells found",
                                 "Not now!\n\nBefore anything else, compute the segmentation or load a previous session");
        return;
    }

    QString filename = QFileDialog::getSaveFileName(this, "Save session to XML file...", "./" + QString::fromStdString(basedir + basefile + "-segmentation-data.xml"), "*.xml *.XML"); // filename

    if (filename.isNull() || filename.isEmpty()) // cancel ?
        return;

    std::string filesession = filename.toUtf8().constData(); // base file name
    size_t pos = filesession.find("-segmentation-data.xml");
    if (pos != std::string::npos) filesession.erase(pos, filesession.length());
    pos = filesession.find(".xml");
    if (pos != std::string::npos) filesession.erase(pos, filesession.length());

    bool write;
    write = cv::imwrite(filesession + "-segmentation-mask.png", mask); // save mask
    if (!write) {
        QMessageBox::critical(this, "File error",
                              "There was a problem saving the segmentation mask image file");
        return;
    }

    write = cv::imwrite(filesession + "-segmentation-grid.png", grid); // save grid
    if (!write) {
        QMessageBox::critical(this, "File error",
                              "There was a problem saving the segmentation grid image file");
        return;
    }

    write = cv::imwrite(filesession + "-segmentation-image.png", image); // save processed image
    if (!write) {
        QMessageBox::critical(this, "File error",
                              "There was a problem saving the segmentation image file");
        return;
    }

    cv::FileStorage fs(filesession + "-segmentation-data.xml", cv::FileStorage::WRITE); // open labels file for writing
    if (!fs.isOpened()) {
        QMessageBox::critical(this, "File error",
                              "There was a problem writing the segmentation data file");
        return;
    }

    int gridValue = ui->comboBox_grid_color->currentIndex(); // save grid color
    fs << "GridColor" << gridValue; // write labels count
    int nb_count = ui->listWidget_labels->count(); // how many labels to save ?
    fs << "LabelsCount" << nb_count; // write labels count

    for (int i = 0; i < nb_count; i++) { // for each label
        std::string field;

        QListWidgetItem *item = ui->listWidget_labels->item(i); // label id

        field = "LabelId" + std::to_string(i);
        fs << field << item->data(Qt::UserRole).toInt(); // write label id

        std::string name = item->text().toUtf8().constData(); // write label name
        field = "LabelName" + std::to_string(i);
        fs << field << name;

        QBrush brush = item->background(); // write label color
        Vec3b col = Vec3b(brush.color().red(), brush.color().green(), brush.color().blue());
        field = "LabelColor" + std::to_string(i);
        fs << field << col;
    }

    fs << "Labels" << labels; // write labels Mat
    fs << "LabelsMask" << labels_mask; // write labels mask Mat

    fs.release(); // close file

    QMessageBox::information(this, "Save segmentation session", "Session saved with base name:\n" + QString::fromStdString(filesession));

    basefile = filesession; // base file name and dir are used after to save other files
    basedir = basefile;
    size_t found = basefile.find_last_of("/"); // find last directory
    basedir = basedir.substr(0,found) + "/"; // extract file location
    basefile = basefile.substr(found+1); // delete ending slash
    pos = basefile.find("-segmentation-data");
    if (pos != std::string::npos) basefile.erase(pos, basefile.length());
    ui->label_filename->setText(filename); // display file name in ui
    SaveDirBaseFile(); // Save current path to ini file
}

void MainWindow::on_button_load_session_clicked() // load previous session
{
    //if (image.empty()) // image mandatory
    //        return;

    QString filename = QFileDialog::getOpenFileName(this, "Load session from XML file...", QString::fromStdString(basedir), "*.xml *.XML");

    if (filename.isNull() || filename.isEmpty()) // cancel ?
        return;

    basefile = filename.toUtf8().constData(); // base file name and dir are used after to save other files
    size_t pos = basefile.find(".xml");
    if (pos != std::string::npos) basefile.erase(pos, basefile.length());
    basedir = basefile;
    size_t found = basefile.find_last_of("/"); // find last directory
    basedir = basedir.substr(0,found) + "/"; // extract file location
    basefile = basefile.substr(found+1); // delete ending slash
    pos = basefile.find("-segmentation-data");
    if (pos != std::string::npos) basefile.erase(pos, basefile.length());
    ui->label_filename->setText(filename); // display file name in ui
    SaveDirBaseFile(); // Save current path to ini file

    std::string filesession = filename.toUtf8().constData(); // base file name
    pos = filesession.find("-segmentation-data.xml");
    if (pos != std::string::npos) filesession.erase(pos, filesession.length());

    InitializeValues(); // reinit all variables
    image.release();
    mask.release();
    grid.release();

    mask = cv::imread(filesession + "-segmentation-mask.png", IMREAD_COLOR); // load mask
    if (mask.empty()) {
        QMessageBox::critical(this, "File error",
                              "There was a problem reading the segmentation mask file:\nit must end with ''-segmentation-mask.png''");
        return;
    }

    grid = cv::imread(filesession + "-segmentation-grid.png"); // load grid
    if (grid.empty()) {
        QMessageBox::critical(this, "File error",
                              "There was a problem reading the segmentation grid file:\nit must end with ''-segmentation-grid.png''");
        return;
    }

    image = cv::imread(filesession + "-segmentation-image.png"); // load processed image
    if (image.empty()) {
        QMessageBox::critical(this, "File error",
                              "There was a problem reading the segmentation image file:\nit must end with ''-segmentation-image.png''");
        return;
    }

    DeleteAllLabels(false); // delete all labels but do not create a new one

    cv::FileStorage fs(filesession + "-segmentation-data.xml", FileStorage::READ); // open labels file

    if (!fs.isOpened()) {
        QMessageBox::critical(this, "File error",
                              "There was a problem reading the segmentation data file:\nit must end with ''-segmentation-data.xml''");
        return;
    }

    Mat labels_temp(image.rows, image.cols, CV_32SC1);
    fs["Labels"] >> labels_temp; // load labels
    labels_temp.copyTo(labels);
    fs["LabelsMask"] >> labels_temp; // load labels mask
    labels_temp.copyTo(labels_mask);

    maxLabels = 0; // reset label count
    int nb_count, gridValue;

    fs["GridColor"] >> gridValue; // grid color
    fs["LabelsCount"] >> nb_count; // read how many labels to load ?

    for (int i = 0; i < nb_count; i++) { // for each label to load
        QListWidgetItem *item = new QListWidgetItem (); // create new label
        std::string field;

        int num;
        field = "LabelId" + std::to_string(i); // read label id
        fs [field] >> num;
        item->setData(Qt::UserRole, num); // set it to current label
        if (num > maxLabels) maxLabels = num; // numLabels must be equal to the max label value

        std::string name;
        field = "LabelName" + std::to_string(i); // read label name
        fs [field] >> name;
        item->setText(QString::fromStdString(name)); // set name to current label

        Vec3b col;
        field = "LabelColor" + std::to_string(i); // read label color
        fs [field] >> col;
        item->setBackground(QColor(col[0], col[1], col[2])); // set color to current label
        if (IsRGBColorDark(col[0], col[1], col[2])) // set text color relative to background darkness
            item->setForeground(QColor(255, 255, 255));
            else item->setForeground(QColor(0, 0, 0));

        item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled); // item enabled, editable and selectable

        ui->listWidget_labels->addItem(item); // add the new item to the list
        ui->listWidget_labels->setCurrentItem(item); // select the new label
        item->setSelected(false); // don't select it !

        ui->Tabs->setTabEnabled(1, true); // enable Segmentation and Labels tabs
        ui->Tabs->setTabEnabled(2, true);
        ui->Tabs->setCurrentIndex(2);
    }

    fs.release(); // close file

    ui->label_thumbnail->setPixmap(QPixmap()); // reinit all images in GUI
    ui->label_segmentation->setPixmap(QPixmap());
    ui->label_thumbnail->setPixmap(Mat2QPixmapResized(image, ui->label_thumbnail->width(), ui->label_thumbnail->height(), true)); // Show thumbnail

    double zoomX = double(ui->label_segmentation->width()) / image.cols; // try vertical and horizontal ratios
    double zoomY = double(ui->label_segmentation->height()) / image.rows;
    if (zoomX < zoomY) zoom = zoomX; // the lowest fit the view
        else zoom = zoomY;
    oldZoom = zoom; // for center view
    ShowZoomValue(); // display current zoom
    viewport = Rect(0, 0, image.cols, image.rows); // update viewport

    cv::resize(image, thumbnail, Size(ui->label_thumbnail->pixmap()->width(),ui->label_thumbnail->pixmap()->height()),
               0, 0, INTER_AREA); // create thumbnail

    undo_mask.release(); // reinit undo masks
    undo_labels.release();
    selection.release();

    image.copyTo(image_backup); // for undo
    selection = Mat::zeros(image.rows, image.cols, CV_8UC3);

    double min, max;
    minMaxIdx(labels, &min, &max); // find the highest superpixel number
    ui->lcd_cells->setPalette(Qt::red); // LCD count red = not yet computed
    ui->lcd_cells->display(max); // LCD count
    ui->label_image_width->setText(QString::number(image.cols)); // display image dimensions
    ui->label_image_height->setText(QString::number(image.rows));

    SaveUndo(); // for undo
    loaded = true; // done !
    computed = true; // not really computed but necessary

    ui->comboBox_grid_color->setCurrentIndex(gridValue); // update grid color in ui
    UnselectAllLabels(); // only the first label must be selected
    ui->listWidget_labels->setCurrentRow(0);

    //QMessageBox::information(this, "Load segmentation session", "Session loaded with base name:\n" + QString::fromStdString(filesession));
}

void MainWindow::on_button_save_conf_clicked() // save configuration
{
    QString filename = QFileDialog::getSaveFileName(this, "Save configuration to XML file...", "./" + QString::fromStdString(basedir + basefile + "-segmentation-conf.xml"));
    if (filename.isNull() || filename.isEmpty()) // cancel ?
        return;

    std::string filename_s = filename.toUtf8().constData(); // Convert from QString

    int algorithm = ui->comboBox_algorithm->currentIndex(); // Initialize variables to save from GUI
    int region_size = ui->horizontalSlider_region_size->value();
    int ruler = ui->horizontalSlider_ruler->value();
    int min_element_size = ui->horizontalSlider_connectivity->value();
    int num_iterations = ui->horizontalSlider_iterations->value();
    int line_color = ui->comboBox_grid_color->currentIndex();
    bool thick_lines = ui->checkBox_thick->isChecked();
    int num_superpixels = ui->horizontalSlider_num_superpixels->value();
    int num_levels = ui->horizontalSlider_num_levels->value();
    int prior = ui->horizontalSlider_prior->value();
    int num_histogram_bins = ui->horizontalSlider_num_histogram_bins->value();
    bool double_step = ui->checkBox_double_step->isChecked();
    double ratio = ui->doubleSpinBox_ratio->value();
    bool lab = ui->checkBox_lab_colors->isChecked();
    bool gaussian_blur = ui->checkBox_gaussian_blur->isChecked();
    bool normalize = ui->checkBox_normalize->isChecked();
    bool equalize = ui->checkBox_equalize->isChecked();
    bool color_balance = ui->checkBox_color_balance->isChecked();
    int color_balance_percent = ui->spinBox_color_balance_percent->value();
    bool contours = ui->checkBox_contours->isChecked();
    int contours_sigma = ui->spinBox_contours_sigma->value();
    int contours_thickness = ui->spinBox_contours_thickness->value();
    int contours_aperture = ui->comboBox_contours_aperture->currentIndex() * 2 + 3;
    bool denoise = ui->checkBox_denoise->isChecked();
    int luminance = ui->horizontalSlider_luminance->value();
    int chrominance = ui->horizontalSlider_chrominance->value();

    FileStorage fs(filename_s, FileStorage::WRITE); // save to openCV XML file type

    if (!fs.isOpened()) {
        QMessageBox::critical(this, "File error",
                              "There was a problem writing the configuration file");
        return;
    }

    //// Segmentation parameters
    fs << "Algorithm" << algorithm;
    fs << "RegionSize" << region_size;
    fs << "Ruler" << ruler;
    fs << "Connectivity" << min_element_size;
    fs << "NumIterations" << num_iterations;
    fs << "Superpixels" << num_superpixels;
    fs << "Levels" << num_levels;
    fs << "Prior" << prior;
    fs << "Histograms" << num_histogram_bins;
    fs << "DoubleStep" << double_step;
    fs << "Ratio" << ratio;
    fs << "ThickLines" << thick_lines;
    fs << "LabColors" << lab;
    fs << "Color" << line_color;
    //// image pre-production parameters
    fs << "ColorBalance" << color_balance;
    fs << "ColorBalancePercent" << color_balance_percent;
    fs << "GaussianBlur" << gaussian_blur;
    fs << "Normalize" << normalize;
    fs << "Equalize" << equalize;
    fs << "Contours" << contours;
    fs << "ContoursSigma" << contours_sigma;
    fs << "ContoursThickness" << contours_thickness;
    fs << "ContoursAperture" << contours_aperture;
    fs << "Noise" << denoise;
    fs << "NoiseLuminance" << luminance;
    fs << "NoiseChrominance" << chrominance;

    fs.release(); // close file

    QMessageBox::information(this, "Save configuration", "Configuration saved to:\n" + filename);
}

void MainWindow::on_button_load_conf_clicked() // load configuration
{
    QString filename = QFileDialog::getOpenFileName(this, "Select XML configuration file", QString::fromStdString(basedir + basefile + "-segmentation-conf.xml"), "XML parameters (*.xml *.XML)"); // filename
    if (filename.isNull()) // cancel ?
        return;

    std::string filename_s = filename.toUtf8().constData(); // convert filename from QString to std::string

    // Initialize variables to save
    int algorithm, region_size, ruler, min_element_size, num_iterations, line_color, color_balance_percent,
        num_superpixels, num_levels, prior, num_histogram_bins,
        contours_sigma, contours_thickness, contours_aperture, luminance, chrominance;
    double ratio;
    bool thick_lines, double_step, lab, gaussian_blur, equalize, normalize, color_balance, contours, denoise;

    FileStorage fs(filename_s, FileStorage::READ); // load from openCV XML file

    if (!fs.isOpened()) {
        QMessageBox::critical(this, "File error",
                              "There was a problem reading the configuration file");
        return;
    }

    fs["Algorithm"] >> algorithm;
    fs["RegionSize"] >> region_size;
    fs["Ruler"] >> ruler;
    fs["Connectivity"] >> min_element_size;
    fs["NumIterations"] >> num_iterations;
    fs["Superpixels"] >> num_superpixels;
    fs["Levels"] >> num_levels;
    fs["Prior"] >> prior;
    fs["Histograms"] >> num_histogram_bins;
    fs["DoubleStep"] >> double_step;
    fs["Ratio"] >> ratio;
    fs["ThickLines"] >> thick_lines;
    fs["LabColors"] >> lab;
    fs["Color"] >> line_color;

    fs["ColorBalance"] >> color_balance;
    fs["ColorBalancePercent"] >> color_balance_percent;
    fs["GaussianBlur"] >> gaussian_blur;
    fs["Normalize"] >> normalize;
    fs["Equalize"] >> equalize;
    fs["Contours"] >> contours;
    fs["ContoursSigma"] >> contours_sigma;
    fs["ContoursThickness"] >> contours_thickness;
    fs["ContoursAperture"] >> contours_aperture;
    fs["Noise"] >> denoise;
    fs["NoiseLuminance"] >> luminance;
    fs["NoiseChrominance"] >> chrominance;

    fs.release();

    ui->comboBox_algorithm->setCurrentIndex(algorithm); // update GUI parameters values
    ui->horizontalSlider_region_size->setValue(region_size);
    ui->horizontalSlider_ruler->setValue(ruler);
    ui->horizontalSlider_connectivity->setValue(min_element_size);
    ui->horizontalSlider_iterations->setValue(num_iterations);
    ui->comboBox_grid_color->setCurrentIndex(line_color);
    ui->checkBox_thick->setChecked(thick_lines);
    ui->horizontalSlider_num_superpixels->setValue(num_superpixels);
    ui->horizontalSlider_num_levels->setValue(num_levels);
    ui->horizontalSlider_prior->setValue(prior);
    ui->horizontalSlider_num_histogram_bins->setValue(num_histogram_bins);
    ui->checkBox_double_step->setChecked(double_step);
    ui->doubleSpinBox_ratio->setValue(ratio);
    ui->checkBox_lab_colors->setChecked(lab);
    ui->checkBox_gaussian_blur->setChecked(gaussian_blur);
    ui->checkBox_normalize->setChecked(normalize);
    ui->checkBox_equalize->setChecked(equalize);
    ui->checkBox_color_balance->setChecked(color_balance);
    ui->spinBox_color_balance_percent->setValue(color_balance_percent);
    ui->checkBox_contours->setChecked(contours);
    ui->spinBox_contours_sigma->setValue(contours_sigma);
    ui->spinBox_contours_thickness->setValue(contours_thickness);
    ui->comboBox_contours_aperture->setCurrentIndex((contours_aperture - 3) / 2);
    ui->checkBox_denoise->setChecked(denoise);
    ui->horizontalSlider_luminance->setValue(luminance);
    ui->horizontalSlider_chrominance->setValue(chrominance);

    QMessageBox::information(this, "Load configuration", "Configuration loaded from:\n" + filename);
}

/////////////////// Display controls //////////////////////

void MainWindow::on_checkBox_mask_clicked() // Refresh image : mask on/off
{
    ShowSegmentation(); // update display
}

void MainWindow::on_checkBox_image_clicked() // Refresh image : image on/off
{
    ShowSegmentation(); // update display
}

void MainWindow::on_checkBox_grid_clicked() // Refresh image : grid on/off
{
    ShowSegmentation(); // update display
}

void MainWindow::on_checkBox_selection_clicked() // Refresh image : selection on/off
{
    ShowSegmentation(); // update display
}

void MainWindow::on_checkBox_holes_clicked() // Refresh image : holes on/off
{
    ShowSegmentation(); // update display
}

void MainWindow::on_horizontalSlider_blend_mask_valueChanged() // Refresh image : blend mask transparency changed
{
    ShowSegmentation(); // update display
}

void MainWindow::on_horizontalSlider_blend_image_valueChanged() // Refresh image : image mask transparency changed
{
    ShowSegmentation(); // update display
}

void MainWindow::on_horizontalSlider_blend_grid_valueChanged() // Refresh image : grid mask transparency changed
{
    ShowSegmentation(); // update display
}

void MainWindow::on_horizontalScrollBar_segmentation_valueChanged() // update viewport
{
    viewport.x = ui->horizontalScrollBar_segmentation->value(); // new value
    ShowSegmentation(); // update display
}

void MainWindow::on_verticalScrollBar_segmentation_valueChanged()  // update viewport
{
    viewport.y = ui->verticalScrollBar_segmentation->value(); // new value
    ShowSegmentation(); // update display
}

void MainWindow::on_comboBox_grid_color_currentIndexChanged(int) // Change grid color
{
    if (!computed) // not computed = no mask !
        return;

    switch (ui->comboBox_grid_color->currentIndex()) { // color values in BGR order
        case 0: gridColor = Vec3b(0,0,255);break; // Red
        case 1: gridColor = Vec3b(0,255,0);break; // Green
        case 2: gridColor = Vec3b(255,0,0);break; // Blue
        case 3: gridColor = Vec3b(255,255,0);break; // Cyan
        case 4: gridColor = Vec3b(255,0,255);break; // Magenta
        case 5: gridColor = Vec3b(0,255,255);break; // Yellow
        case 6: gridColor = Vec3b(255,255,255);break; // White
    }

    Mat maskTemp;
    cv::cvtColor(grid, maskTemp, COLOR_RGB2GRAY); // convert grid to gray values
    grid.setTo(gridColor, maskTemp > 0); // update only non-zero values to new color
    ShowSegmentation(); // update display
}

void MainWindow::ZoomPlus()
{
    int z = 0;
    while (zoom >= zooms[z]) z++; // from lowest to highest value find the next one
    if (z == num_zooms) z--; // maximum

    if (zoom != zooms[z]) { // zoom changed ?
        QApplication::setOverrideCursor(Qt::SizeVerCursor); // zoom cursor
        oldZoom = zoom;
        zoom = zooms[z]; // new zoom value
        ShowSegmentation(); // update display
        QApplication::restoreOverrideCursor(); // Restore cursor
    }
}

void MainWindow::ZoomMinus()
{
    int z = num_zooms;
    while (zoom <= zooms[z]) z--; // from highest to lowest value find the next one
    if (z == 0) z++; // minimum

    if (zoom != zooms[z]) { // zoom changed ?
        QApplication::setOverrideCursor(Qt::SizeVerCursor); // zoom cursor
        oldZoom = zoom;
        zoom = zooms[z]; // new zoom value
        ShowSegmentation(); // update display
        QApplication::restoreOverrideCursor(); // Restore cursor
    }
}

void MainWindow::on_pushButton_zoom_minus_clicked() // zoom -
{
    zoom_type = "button";
    ZoomMinus();
}

void MainWindow::on_pushButton_zoom_plus_clicked() // zoom +
{
    zoom_type = "button";
    ZoomPlus();
}

void MainWindow::on_pushButton_zoom_fit_clicked() // zoom fit
{
    zoom_type = "button";
    oldZoom = zoom;
    double zoomX = double(ui->label_segmentation->width()) / image.cols; // find the best value of zoom
    double zoomY = double(ui->label_segmentation->height()) / image.rows;
    if (zoomX < zoomY) zoom = zoomX; // less = fit view borders
        else zoom = zoomY;

    QApplication::setOverrideCursor(Qt::SizeVerCursor); // zoom cursor
    ShowSegmentation(); // update display
    QApplication::restoreOverrideCursor(); // Restore cursor
}

void MainWindow::on_pushButton_zoom_100_clicked() // zoom 100%
{
    zoom_type = "button";
    oldZoom = zoom;
    zoom = 1; // new zoom value
    QApplication::setOverrideCursor(Qt::SizeVerCursor); // zoom cursor
    ShowSegmentation(); // update display
    QApplication::restoreOverrideCursor(); // Restore cursor
}

void MainWindow::ShowZoomValue() // display zoom percentage in ui
{
    ui->label_zoom->setText(QString::number(int(zoom * 100)) +"%"); // show new zoom value in percent
}

/////////////////// Undo //////////////////////

void MainWindow::SaveUndo()
{
    mask.copyTo(undo_mask); // mask
    if (!ui->pushButton_label_draw->isChecked()) {
        labels_mask.copyTo(undo_labels); // save labels mask
        draw_cell_mask_save.release(); // release draw saves
        draw_cell_labels_mask_save.release();
        draw_cell_labels_save.release();
        draw_cell_grid_save.release();
    }
}

void MainWindow::on_button_undo_clicked() // Undo last action
{
    if (!computed) { // if image not computed yet get out
        QMessageBox::warning(this, "No cells found",
                                 "Not now!\n\nBefore anything else, compute the segmentation or load a previous session");
        return;
    }

    undo_mask.copyTo(mask); // get back !

    if (!ui->pushButton_label_draw->isChecked()) { //if not in draw cell mode
        if (!draw_cell_labels_save.empty()) { // but if something was drawn before
            draw_cell_mask_save.copyTo(mask); // restore saved masks
            draw_cell_labels_mask_save.copyTo(labels_mask);
            draw_cell_labels_save.copyTo(labels);
            draw_cell_grid_save.copyTo(grid);
            selection = 0;
            ShowCurrentColor(color[2], color[1], color[0]); // redraw label
        }
        else {
            undo_labels.copyTo(labels_mask); // normal case : also save labels_mask
        }
    }
    else { // is label currently drawn ?
        mask.copyTo(mask_line_save); // for draw cell mode
    }

    ShowSegmentation(); // update display
}

/////////////////// Algorithm parameters visibility //////////////////////

void MainWindow::on_comboBox_algorithm_currentIndexChanged(int) // algorithm changed : hide/show options
{
    ui->horizontalSlider_region_size->setVisible(false); // hide all options first
    ui->horizontalSlider_ruler->setVisible(false);
    ui->horizontalSlider_connectivity->setVisible(false);
    ui->horizontalSlider_num_superpixels->setVisible(false);
    ui->horizontalSlider_num_levels->setVisible(false);
    ui->horizontalSlider_prior->setVisible(false);
    ui->horizontalSlider_num_histogram_bins->setVisible(false);
    ui->checkBox_double_step->setVisible(false);
    ui->doubleSpinBox_ratio->setVisible(false);
    ui->label_region_size->setVisible(false);
    ui->label_ruler->setVisible(false);
    ui->label_connectivity->setVisible(false);
    ui->label_superpixels->setVisible(false);
    ui->label_levels->setVisible(false);
    ui->label_prior->setVisible(false);
    ui->label_histograms->setVisible(false);
    ui->label_ratio->setVisible(false);
    ui->label_value_region_size->setVisible(false);
    ui->label_value_ruler->setVisible(false);
    ui->label_value_connectivity->setVisible(false);
    ui->label_value_connectivity_percent->setVisible(false);
    ui->label_value_num_superpixels->setVisible(false);
    ui->label_value_num_levels->setVisible(false);
    ui->label_value_prior->setVisible(false);
    ui->label_value_num_histogram_bins->setVisible(false);

    switch (ui->comboBox_algorithm->currentIndex()) { // and show the good ones for the chosen algorithm
        case 0: ui->horizontalSlider_region_size->setVisible(true);
                ui->horizontalSlider_ruler->setVisible(true);
                ui->horizontalSlider_connectivity->setVisible(true);
                ui->label_region_size->setVisible(true);
                ui->label_ruler->setVisible(true);
                ui->label_connectivity->setVisible(true);
                ui->label_value_region_size->setVisible(true);
                ui->label_value_ruler->setVisible(true);
                ui->label_value_connectivity->setVisible(true);
                ui->label_value_connectivity_percent->setVisible(true);
                break;
        case 1: ui->horizontalSlider_region_size->setVisible(true);
                ui->horizontalSlider_connectivity->setVisible(true);
                ui->label_region_size->setVisible(true);
                ui->label_connectivity->setVisible(true);
                ui->label_value_region_size->setVisible(true);
                ui->label_value_connectivity->setVisible(true);
                ui->label_value_connectivity_percent->setVisible(true);
                break;
        case 2: ui->horizontalSlider_region_size->setVisible(true);
                ui->horizontalSlider_ruler->setVisible(true);
                ui->horizontalSlider_connectivity->setVisible(true);
                ui->label_region_size->setVisible(true);
                ui->label_ruler->setVisible(true);
                ui->label_connectivity->setVisible(true);
                ui->label_value_region_size->setVisible(true);
                ui->label_value_ruler->setVisible(true);
                ui->label_value_connectivity->setVisible(true);
                ui->label_value_connectivity_percent->setVisible(true);
                break;
        case 3: ui->horizontalSlider_region_size->setVisible(true);
                ui->horizontalSlider_connectivity->setVisible(true);
                ui->doubleSpinBox_ratio->setVisible(true);
                ui->label_region_size->setVisible(true);
                ui->label_connectivity->setVisible(true);
                ui->label_value_region_size->setVisible(true);
                ui->label_value_connectivity->setVisible(true);
                ui->label_value_connectivity_percent->setVisible(true);
                ui->label_ratio->setVisible(true);
                break;
        case 4: ui->horizontalSlider_num_superpixels->setVisible(true);
                ui->horizontalSlider_num_levels->setVisible(true);
                ui->horizontalSlider_prior->setVisible(true);
                ui->horizontalSlider_num_histogram_bins->setVisible(true);
                ui->checkBox_double_step->setVisible(true);
                ui->label_superpixels->setVisible(true);
                ui->label_levels->setVisible(true);
                ui->label_prior->setVisible(true);
                ui->label_histograms->setVisible(true);
                ui->label_value_num_superpixels->setVisible(true);
                ui->label_value_num_levels->setVisible(true);
                ui->label_value_prior->setVisible(true);
                ui->label_value_num_histogram_bins->setVisible(true);
                break;
    }
}

/////////////////// Labels colors ////////////////////

void MainWindow::ShowCurrentColor(int red, int green, int blue) // show current label color and set it to label in list
{
    QString text_color;
    if (IsRGBColorDark(red, green, blue)) // text color white or black according to label color
        text_color = "white";
        else text_color = "black";

    ui->label_color->setStyleSheet("QLabel { color:" + text_color + "; background-color:rgb("
                                   + QString::number(red) + ","
                                   + QString::number(green) + ","
                                   + QString::number(blue)
                                   + ");border: 3px solid black; }"); // update current color background and text color

    color = Vec3b(blue, green, red); // color in mask
    QListWidgetItem *item = ui->listWidget_labels->currentItem(); // current label
    item->setBackground(QColor(color[2], color[1], color[0])); // label color
    if (IsRGBColorDark(color[2], color[1], color[0])) // label text color according to label color
        item->setForeground(QColor(255, 255, 255)); // white
        else item->setForeground(QColor(0, 0, 0)); // black
    ui->label_color->setText(item->text()); // show color name
    selection = 0; // erase selection

    if (computed) { // a mask exist ?
        Mat1b superpixel_mask = labels_mask == GetCurrentLabelNumber(); // extract label
        mask.setTo(color, superpixel_mask); // set new color to the mask

        //selection.setTo(Vec3b(0, 64, 64), mask_temp); // fill with dark yellow
        vector<vector<cv::Point>> contours;
        vector<Vec4i> hierarchy;
        findContours(superpixel_mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE); // find new cell contours
        drawContours(selection, contours, -1, Vec3b(255, 255, 255), 1, 8, hierarchy ); // draw contour of new cell in selection mask

        ShowSegmentation(); // show result
    }
}

void MainWindow::on_pushButton_color_pick_clicked() // Pick custom color for current label
{
    QColor pick_color(color[2], color[1], color[0]); // current label color
    pick_color = QColorDialog::getColor(pick_color, this, "Select Color", QColorDialog::DontUseNativeDialog); // pick color dialog

    if (pick_color.isValid()) { // color selected ?
        color = Vec3b(pick_color.green(), pick_color.blue(), pick_color.red()); // get selected color
        if (color == Vec3b(0,0,0)) color = Vec3b(1,1,1);
        QListWidgetItem *item = ui->listWidget_labels->currentItem(); // current label
        item->setBackground(QColor(color[2], color[1], color[0])); // set current label color
        if (IsRGBColorDark(color[2], color[1], color[0])) // text color according to label color
            item->setForeground(QColor(255, 255, 255)); // white
            else item->setForeground(QColor(0, 0, 0)); // black
        ShowCurrentColor(pick_color.red(), pick_color.green(), pick_color.blue()); // show new color
    }
}

// All following functions are the same, only the BGR colors change

void MainWindow::on_pushButton_color_red_clicked() //Red
{
    color = Vec3b(0,0,255); // chosen color
    ShowCurrentColor(color[2], color[1], color[0]); // show new color
}

void MainWindow::on_pushButton_color_green_clicked() // Green
{
    color = Vec3b(0,255,0);
    ShowCurrentColor(0, 255, 0);
}

void MainWindow::on_pushButton_color_blue_clicked() // Blue
{
    color = Vec3b(255,0,0);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_cyan_clicked() // Cyan
{
    color = Vec3b(255,255,0);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_magenta_clicked() // Magenta
{
    color = Vec3b(255,0,255);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_yellow_clicked() // Yellow
{
    color = Vec3b(0,255,255);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_orange_clicked() // Orange
{
    color = Vec3b(0,128,255);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_brown_clicked() // Brown
{
    color = Vec3b(0,0,128);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_olive_clicked() // Olive = dark green
{
    color = Vec3b(0,128,128);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_navy_clicked() // Navy blue
{
    color = Vec3b(128,0,0);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_emerald_clicked() // Emerald = blue / green
{
    color = Vec3b(128,128,0);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_purple_clicked() // Purple
{
    color = Vec3b(255,0,128);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_lime_clicked() // Lime
{
    color = Vec3b(0,255,192);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_rose_clicked() // Rose
{
    color = Vec3b(128,0,255);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_violet_clicked() // Violet
{
    color = Vec3b(128,0,128);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_azure_clicked() // Azure
{
    color = Vec3b(255,128,0);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_malibu_clicked() // Malibu
{
    color = Vec3b(255,128,128);
    ShowCurrentColor(color[2], color[1], color[0]);
}

void MainWindow::on_pushButton_color_laurel_clicked() // Laurel
{
    color = Vec3b(0,128,0);
    ShowCurrentColor(color[2], color[1], color[0]);
}

///////////////////  Key events  //////////////////////

void MainWindow::keyReleaseEvent(QKeyEvent *keyEvent) // draw cell mode and move view + show holes
{
    if (!loaded) return;// no image = get out

    if (keyEvent->key() == Qt::Key_Space) { // spacebar = move the view
        QPoint mouse_pos = QCursor::pos(); // current mouse position

        int decX = mouse_pos.x() - mouse_origin.x(); // distance from the first click
        int decY = mouse_pos.y() - mouse_origin.y();

        SetViewportXY(viewport.x - double(decX) / zoom, viewport.y - double(decY) / zoom); // update viewport

        ShowSegmentation(); // display result

        QApplication::restoreOverrideCursor(); // Restore cursor
    }

    if (!computed) return;// no labels = get out
    if (ui->Tabs->currentIndex() != 2) return; // Not on labels tab

    if ((keyEvent->key() == Qt::Key_X) & (ui->pushButton_label_draw->isChecked())) { // add cell mode line
        if (pos_save == cv::Point(-1, -1)) // first point not set ?
            return;

        if (keyEvent->isAutoRepeat()) { // if <X> still pressed
            mask_line_save.copyTo(mask); // erase line

            ShowSegmentation(); // show result
        }
        else { // <X> released = really draw line
            QPoint mouse_pos = ui->label_segmentation->mapFromGlobal(QCursor::pos()); // current mouse position
            cv::Point pos = Viewport2Image(cv::Point(mouse_pos.x(), mouse_pos.y())); // convert position from viewport to image coordinates

            mask_line_save.copyTo(mask); // erase line (mouse position can have changed a bit)
            SaveUndo(); // for undo

            Vec3b draw_color = DrawColor();
            cv::line(mask, pos_save, pos, draw_color, ui->horizontalSlider_draw_size->value() + 1, LINE_4); // line drawn in mask

            mask.copyTo(mask_line_save); // save result for next line
            pos_save = pos; // the last pixel is the new line origin

        }
    }
    else if ((keyEvent->key() == Qt::Key_C) & (ui->pushButton_label_draw->isChecked())) { // add cell mode circle
            mask_line_save.copyTo(mask); // restore mask

            ShowSegmentation(); // show result
    }
    else if (keyEvent->key() == Qt::Key_H) { // show holes mode ?
        if (keyEvent->isAutoRepeat()) { // if <H> still pressed
            selection = 0;
            Vec3b col(100 + rand() % 156, 100 + rand() % 156, 100 + rand() % 156);

            Mat1b holes_mask;
            cv::inRange(mask, Vec3b(0,0,0), Vec3b(0,0,0), holes_mask); // extract holes
            selection.setTo(col, holes_mask); // draw holes in random color

            vector<vector<cv::Point>> contours;
            vector<Vec4i> hierarchy;
            findContours(holes_mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE); // find holes contours
            drawContours(selection, contours, -1, col, rand() % 5 + 1, 8, hierarchy ); // draw contour of holes in selection mask

            ShowSegmentation(); // show result
        }
        else { // <H> released = don't show holes anymore
            mask_line_save.copyTo(selection); // erase line (mouse position can have changed a bit

            ShowSegmentation(); // show result
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *keyEvent) //
{
    if (!loaded) return;// no image = get out

    if (keyEvent->key() == Qt::Key_Space) { // spacebar = move the view
        mouse_origin = QCursor::pos(); // current mouse position

        QApplication::setOverrideCursor(Qt::SizeAllCursor); // Move cursor
    }

    if (!computed) return;// no labels = get out
    if (ui->Tabs->currentIndex() != 2) return; // Not on labels tab

    if ((keyEvent->key() == Qt::Key_X) & (ui->pushButton_label_draw->isChecked())) { // draw cell mode line
        if (pos_save == cv::Point(-1, -1)) // first point not set
            return;

        if (!keyEvent->isAutoRepeat()) mask.copyTo(mask_line_save); // first time <X> is pressed

        QPoint mouse_pos = ui->label_segmentation->mapFromGlobal(QCursor::pos()); // current mouse position
        cv::Point pos = Viewport2Image(cv::Point(mouse_pos.x(), mouse_pos.y())); // convert position from viewport to image coordinates

        Vec3b draw_color = DrawColor();
        cv::line(mask, pos_save, pos, draw_color, ui->horizontalSlider_draw_size->value() + 1, LINE_4); // draw temp line

        ShowSegmentation(); // show result
    }
    if ((keyEvent->key() == Qt::Key_C) & (ui->pushButton_label_draw->isChecked())) { // draw cell mode show circle
        if (!keyEvent->isAutoRepeat()) mask.copyTo(mask_line_save); // first time <C> is pressed save mask

        QPoint mouse_pos = ui->label_segmentation->mapFromGlobal(QCursor::pos()); // current mouse position
        cv::Point pos = Viewport2Image(cv::Point(mouse_pos.x(), mouse_pos.y())); // convert position from viewport to image coordinates

        Vec3b draw_color = DrawColor();
        if (ui->horizontalSlider_draw_size->value() == 0)
            mask.at<Vec3b>(pos.y, pos.x) = draw_color; // draw pixel in mask
        else // circle
            cv::circle(mask, pos, ui->horizontalSlider_draw_size->value() + 1, draw_color, 1, LINE_8); // draw temporary circle

        ShowSegmentation(); // show result
    }
    else if ((keyEvent->key() == Qt::Key_H) & (!keyEvent->isAutoRepeat())) {
        selection.copyTo(mask_line_save); // first time <H> is pressed save selection
        srand (time(NULL)); // Random seed
    }
}

/////////////////// Mouse events //////////////////////

void MainWindow::mouseReleaseEvent(QMouseEvent *eventRelease) // event triggered by releasing a mouse click
{
    QApplication::restoreOverrideCursor(); // Restore cursor
}

void MainWindow::mousePressEvent(QMouseEvent *eventPress) // event triggered by a mouse click
{
    if (!loaded) return; // no image = get out

    if (ui->label_segmentation->underMouse()) { // mouse over the viewport ?
        bool key_control = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier); // modifier keys pressed ? (shift control etc)
        bool key_alt = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier);

        mouse_origin = ui->label_segmentation->mapFromGlobal(QCursor::pos()); // mouse position
        cv::Point pos = Viewport2Image(cv::Point(mouse_origin.x(), mouse_origin.y())); // convert from viewport to image coordinates
        mouseButton = eventPress->button(); // mouse button value

        if (mouseButton == Qt::MiddleButton) { // not for a zoom !
            QApplication::setOverrideCursor(Qt::SizeAllCursor); // Move cursor
            return;
        }

        if ((computed) & (ui->Tabs->currentIndex() == 2)) { // mouse in labels tab ?
            if ((key_alt) & (mouseButton == Qt::LeftButton) & (pos.x >= 0) & (pos.x < image.cols)
                    & (pos.y >= 0) & (pos.y < image.rows)
                    & (!ui->pushButton_label_draw->isChecked())) { // <alt> pressed at the same time = label selection
                int value = labels_mask.at<int>(pos.y, pos.x); // get label mask value
                if (value != 0) {
                    for (int i = 0; i < ui->listWidget_labels->count(); i++) { // find the clicked label
                        QListWidgetItem *item = ui->listWidget_labels->item(i);
                        if (item->data(Qt::UserRole).toInt() == value) {
                            ui->listWidget_labels->setCurrentRow(i);
                            break;
                        }
                    }
                }
            }
            else if (key_control) { // <control> pressed at the same time = fill function
                if ((mouseButton == Qt::LeftButton) & (pos.x >= 0) & (pos.x < image.cols)
                        & (pos.y >= 0) & (pos.y < image.rows)) { // fill with color
                    SaveUndo(); // save for undo
                    Vec3b draw_color;
                    draw_color = color;
                    if (ui->pushButton_label_draw->isChecked()) { // direct draw pixel for new label
                        draw_color = DrawColor();
                    }
                    floodFill(mask, cv::Point(pos.x, pos.y), draw_color); // floodfill mask
                    if (!ui->pushButton_label_draw->isChecked()) floodFill(labels_mask, cv::Point(pos.x, pos.y), GetCurrentLabelNumber()); // floodfill labels mask
                }
                else if ((mouseButton == Qt::RightButton) & (pos.x >= 0) & (pos.x < image.cols)
                        & (pos.y >= 0) & (pos.y < image.rows)) { // floodfill with zeros = delete
                    SaveUndo(); // save for undo
                    if (ui->pushButton_label_draw->isChecked()) { // direct draw pixel for new label
                        Mat1b mask_tmp = Mat::zeros(image.rows, image.cols, CV_8UC1);
                        floodFill(mask, cv::Point(pos.x, pos.y), Vec3b(1, 0, 0)); // floodfill mask
                        //mask_tmp = mask == Vec3b(255, 0, 0);
                        cv::inRange(mask, Vec3b(1,0,0), Vec3b(1,0,0), mask_tmp);
                        draw_cell_mask_save.copyTo(mask, mask_tmp);
                    }
                    else {
                        floodFill(mask, cv::Point(pos.x, pos.y), 0); // floodfill mask
                        floodFill(labels_mask, cv::Point(pos.x, pos.y), 0); // floodfill labels mask
                    }
                }
                ShowSegmentation(); // show result
            }
            else {
                if (ui->pushButton_label_draw->isChecked()) { // direct draw pixel for new label
                    SaveUndo(); // for undo
                    if ((mouseButton == Qt::LeftButton) & (pos.x >= 0) & (pos.x < image.cols)
                            & (pos.y >= 0) & (pos.y < image.rows)) { // left mouse button = white pixel
                        //mask.at<Vec3b>(pos.y, pos.x) = Vec3b(255, 255, 255); // draw white pixel in mask
                        //labels_mask.at<int>(pos.y, pos.x) = GetCurrentLabelNumber(); // draw pixel in label mask pixel
                        Vec3b draw_color = DrawColor();
                        if (ui->horizontalSlider_draw_size->value() == 0)
                            mask.at<Vec3b>(pos.y, pos.x) = draw_color; // draw pixel in mask
                        else // circle
                            circle(mask, pos, ui->horizontalSlider_draw_size->value() + 1, draw_color, -1, LINE_8);
                    }
                    else if ((mouseButton == Qt::RightButton) & (pos.x >= 0) & (pos.x < image.cols)
                            & (pos.y >= 0) & (pos.y < image.rows)) { // right mouse button = transparent/black pixel
                        Mat1b mask_tmp = Mat::zeros(image.rows, image.cols, CV_8UC1);
                        if (ui->horizontalSlider_draw_size->value() == 0)
                            mask.at<Vec3b>(pos.y, pos.x) = Vec3b(255, 255, 255); // draw pixel in mask
                        else // circle
                            circle(mask_tmp, pos, ui->horizontalSlider_draw_size->value() + 1, Vec3b(255, 255, 255), -1, LINE_8);
                        draw_cell_mask_save.copyTo(mask, mask_tmp);
                    }

                    pos_save = pos; // this pixel can be the origin of a new line
                    mask.copyTo(mask_line_save); // for draw cell mode

                    ShowSegmentation(); // show result
                }
                else if ((pos.x >= 0) & (pos.x < image.cols)
                        & (pos.y >= 0) & (pos.y < image.rows)) {
                    // Color already set before ? saves CPU time
                    int pixel = labels_mask.at<int>(pos.y, pos.x); // get label mask value
                    if (mouseButton == Qt::MiddleButton) { // not for a zoom !
                        return;
                    }
                    else if ((mouseButton == Qt::LeftButton) & (pixel == GetCurrentLabelNumber())) // left button : compare pixel with current color
                            return; // color already set
                         else if ((mouseButton == Qt::RightButton) & (pixel == 0)) // right button : compare pixel with 0 (delete)
                                 return;  // color already set
                    SaveUndo(); // for undo
                    WhichCell(pos.x, pos.y); // Show which cell has been clicked
                }
            }
        }
            else if (mouseButton != Qt::MiddleButton) QMessageBox::warning(this, "Labels edition",
                                          "Labels edition is only possible in the ''Labels'' tab.\n\nYou must first:\n   - compute the segmentation in the ''Segmentation'' tab\n   - or load a previous session in the ''Labels'' tab");
    }  else {
        if (ui->label_thumbnail->underMouse()) { // if the mouse is over the thumbnail
            QPoint mouse_pos = ui->label_thumbnail->mapFromGlobal(QCursor::pos()); // current mouse position
            mouseButton = eventPress->button(); // mouse button value

            if (mouseButton == Qt::LeftButton) { // move view
                // convert middle of viewport from thumbnail to image coordinates
                int middleX = double(mouse_pos.x() - (ui->label_thumbnail->width() - ui->label_thumbnail->pixmap()->width()) / 2) / ui->label_thumbnail->pixmap()->width() * image.cols;
                int middleY = double(mouse_pos.y() - (ui->label_thumbnail->height() - ui->label_thumbnail->pixmap()->height()) / 2) / ui->label_thumbnail->pixmap()->height() * image.rows;
                SetViewportXY(middleX - viewport.width /2, middleY - viewport.height /2); // set viewport to new middle position

                ShowSegmentation(); // show result
            }
        }
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *eventMove) // first mouse click has already been treated and is holded down
{
    if (!loaded) return;// no image = get out

    if (ui->label_thumbnail->underMouse()) { // if the mouse is over the thumbnail
        QPoint mouse_pos = ui->label_thumbnail->mapFromGlobal(QCursor::pos()); // current mouse position

        if (mouseButton == Qt::LeftButton) { // move view
            // convert middle of viewport from thumbnail to image coordinates
            int middleX = double(mouse_pos.x() - (ui->label_thumbnail->width() - ui->label_thumbnail->pixmap()->width()) / 2) / ui->label_thumbnail->pixmap()->width() * image.cols;
            int middleY = double(mouse_pos.y() - (ui->label_thumbnail->height() - ui->label_thumbnail->pixmap()->height()) / 2) / ui->label_thumbnail->pixmap()->height() * image.rows;
            SetViewportXY(middleX - viewport.width /2, middleY - viewport.height /2); // set viewport to new middle position

            ShowSegmentation(); // display the result
            return;
        }
    }
    else if (ui->label_segmentation->underMouse()) { // if the mouse is over the viewport
        QPoint mouse_pos = ui->label_segmentation->mapFromGlobal(QCursor::pos()); // current mouse position

        if (mouseButton == Qt::MidButton) { // move view
            int decX = mouse_pos.x() - mouse_origin.x(); // distance from the first click
            int decY = mouse_pos.y() - mouse_origin.y();

            SetViewportXY(viewport.x - double(decX) / zoom, viewport.y - double(decY) / zoom); // update viewport

            ShowSegmentation(); // display the result

            mouse_origin = mouse_pos; // save mouse position
        }
        else if ((computed) & (ui->Tabs->currentIndex() == 2)) { // mouse over tab 2 ?
            cv::Point pos = Viewport2Image(cv::Point(mouse_pos.x(), mouse_pos.y())); // convert position from viewport to image coordinates

            if (ui->pushButton_label_draw->isChecked()) { // direct draw pixel for new label
                if ((mouseButton == Qt::LeftButton) & (pos.x >= 0) & (pos.x < image.cols)
                        & (pos.y >= 0) & (pos.y < image.rows)) { // left mouse button pixel = white
                    //mask.at<Vec3b>(pos.y, pos.x) = Vec3b(255, 255, 255); // white pixel
                    //labels_mask.at<int>(pos.y, pos.x) = GetCurrentLabelNumber(); // label mask pixel
                    Vec3b draw_color = DrawColor();
                    if (ui->horizontalSlider_draw_size->value() == 0)
                        mask.at<Vec3b>(pos.y, pos.x) = draw_color; // draw pixel in mask
                    else // circle
                        circle(mask, pos, ui->horizontalSlider_draw_size->value() + 1, draw_color, -1, LINE_8);
                }
                else if ((mouseButton == Qt::RightButton) & (pos.x >= 0) & (pos.x < image.cols)
                         & (pos.y >= 0) & (pos.y < image.rows)) { // right mouse button pixel unset
                    Mat1b mask_tmp = Mat::zeros(image.rows, image.cols, CV_8UC1);
                    if (ui->horizontalSlider_draw_size->value() == 0)
                        mask.at<Vec3b>(pos.y, pos.x) = Vec3b(255, 255, 255); // draw pixel in mask
                    else // circle
                        circle(mask_tmp, pos, ui->horizontalSlider_draw_size->value(), Vec3b(255, 255, 255), -1, LINE_8);
                    draw_cell_mask_save.copyTo(mask, mask_tmp);
                }

                pos_save = pos; // the pixel can be the origin of a new line
                mask.copyTo(mask_line_save); // mask cache for drawing a line

                ShowSegmentation(); // show result
            }
            else if ((pos.x >= 0) & (pos.x < image.cols) // define cell color
                     & (pos.y >= 0) & (pos.y < image.rows)){
                // Color already set before ? saves CPU time
                //Vec3b pixel = mask.at<Vec3b>(pos.y,pos.x); // find mask pixel color
                int pixel = labels_mask.at<int>(pos.y, pos.x); // value of pixel in labels mask
                if ((mouseButton == Qt::LeftButton) & (pixel == GetCurrentLabelNumber())) // left button : compare pixel with current color
                    return; // color already set
                else if ((mouseButton == Qt::RightButton) & (pixel == 0)) // right button : compare pixel with 0
                    return;  // color already set

                WhichCell(pos.x, pos.y); // Show which cell has been clicked
            }
        }
    }
}

void MainWindow::wheelEvent(QWheelEvent *wheelEvent) // mouse wheel turned
{
    if (!loaded) return;// no image = get out

    if (ui->label_segmentation->underMouse()) { // if the mouse is over the viewport
        mouse_origin = ui->label_segmentation->mapFromGlobal(QCursor::pos()); // mouse position
        cv::Point pos = Viewport2Image(cv::Point(mouse_origin.x(), mouse_origin.y())); // convert from viewport to image coordinates
        mouse_origin = QPoint(pos.x, pos.y); // zoom from mouse psoition
        int n = wheelEvent->delta(); // amount of wheel turn
        if (n < 0) { // positive = zoom out
            zoom_type = "wheel";
            ZoomMinus();
        }
        if (n > 0) { // negative = zoom in
            zoom_type = "wheel";
            ZoomPlus();
        }
        wheelEvent->accept(); // event accepted
    }
}

/////////////////// Core functions //////////////////////

cv::Point MainWindow::Viewport2Image(cv::Point p) // convert viewport to image coordinates
{
    int posX = double(p.x - (ui->label_segmentation->width() - ui->label_segmentation->pixmap()->width()) / 2) / zoom + viewport.x;
    int posY = double(p.y - (ui->label_segmentation->height() - ui->label_segmentation->pixmap()->height()) / 2) / zoom + viewport.y;
    return cv::Point(posX, posY);
}

void MainWindow::DisplayThumbnail() // image thumnail with transparent viewport indicator
{
    if ((viewport.width == image.cols) & (viewport.height == image.rows)) { // entire view = no rectangle to draw
        ui->label_thumbnail->setPixmap(Mat2QPixmap(thumbnail)); // update view
        return;
    }

    double p1x, p1y, p2x, p2y; // rectangle position
    if (viewport.width == image.cols) { // entire horizontal view ?
        p1x = 0;
        p2x = ui->label_thumbnail->pixmap()->width();
    }
    else { // partial view
        p1x = double(viewport.x) / image.cols;
        p2x = p1x + double(viewport.width) / image.cols;
        p1x *= double(ui->label_thumbnail->pixmap()->width());
        p2x *= double(ui->label_thumbnail->pixmap()->width());
    }

    if (viewport.height == image.rows) { // entire vertical view ?
        p1y = 0;
        p2y= ui->label_thumbnail->pixmap()->height();
    }
    else { // partial view
        p1y = double(viewport.y) / image.rows;
        p2y = p1y + double(viewport.height) / image.rows;
        p1y *= double(ui->label_thumbnail->pixmap()->height());
        p2y *= double(ui->label_thumbnail->pixmap()->height());
    }

    Mat thumbnail_temp = Mat::zeros(thumbnail.rows, thumbnail.cols, CV_8UC3);
    rectangle(thumbnail_temp, cv::Point(p1x, p1y), cv::Point(p2x-1, p2y-1), Scalar(92,92,92), -1); // draw filled rectangle representing the viewport position
    rectangle(thumbnail_temp, cv::Point(p1x, p1y), cv::Point(p2x-1, p2y-1), Scalar(255,255,255), 2); // draw rectangle with thick border
    cv::addWeighted(thumbnail, 1, thumbnail_temp, 0.25, 0, thumbnail_temp, -1); // with transparency
    ui->label_thumbnail->setPixmap(Mat2QPixmap(thumbnail_temp)); // update view
}

void MainWindow::UpdateViewportDimensions() // recomputes the viewport width and height
{
    viewport.width = double(ui->label_segmentation->width()) / zoom; // viewport width
    if (viewport.width > image.cols) {
        viewport.width = image.cols;
        viewport.x = 0;
    }
    viewport.height = double(ui->label_segmentation->height()) / zoom; // viewport height
    if (viewport.height > image.rows) {
        viewport.height = image.rows;
        viewport.y = 0;
    }
    if (viewport.x > image.cols - viewport.width) viewport.x = image.cols - viewport.width; // stay within image
    if (viewport.y > image.rows - viewport.height) viewport.y = image.rows - viewport.height;
}

void MainWindow::SetViewportXY(int x, int y) // set viewport top-left to (x,y) new coordinates
{
    viewport.x = x; // coordinates
    viewport.y = y;
    if (viewport.x < 0) viewport.x = 0;
    if (viewport.x > image.cols - viewport.width) viewport.x = image.cols - viewport.width;
    if (viewport.y < 0) viewport.y = 0;
    if (viewport.y > image.rows - viewport.height) viewport.y = image.rows - viewport.height;

    ui->horizontalScrollBar_segmentation->blockSignals(true); // the scrollbar must not trigger an action
    ui->horizontalScrollBar_segmentation->setValue(viewport.x); // new x value
    ui->horizontalScrollBar_segmentation->blockSignals(false); // return to normal state
    ui->verticalScrollBar_segmentation->blockSignals(true); // the scrollbar must not trigger an action
    ui->verticalScrollBar_segmentation->setValue(viewport.y); // new y value
    ui->verticalScrollBar_segmentation->blockSignals(false); // return to normal state
}

void MainWindow::ShowSegmentation() // show image + mask + grid
{
    if (!loaded) // no image to display
        return;

    int oldMiddleX, oldMiddleY;
    if (oldZoom != zoom) { // zoom has changed ?
        oldMiddleX = viewport.x + viewport.width / 2; // current middle of viewport for zooming to the center of image
        oldMiddleY = viewport.y + viewport.height / 2;
        if (zoom_type == "wheel") { // if zoomed with the wheel set origin of zoom under mouse
            oldMiddleX = mouse_origin.x() - oldZoom * (mouse_origin.x() - viewport.x) / zoom + double(ui->label_segmentation->width()) / zoom / 2;
            oldMiddleY = mouse_origin.y() - oldZoom * (mouse_origin.y() - viewport.y) / zoom + double(ui->label_segmentation->height()) / zoom / 2;
            zoom_type = "";
        }
        UpdateViewportDimensions(); // recompute viewport width and height
        double newPosX = oldMiddleX - viewport.width / 2; // compute new middle of viewport
        double newPosY = oldMiddleY - viewport.height / 2;
        ui->horizontalScrollBar_segmentation->setMaximum(image.cols-viewport.width); // update scrollbars range
        ui->verticalScrollBar_segmentation->setMaximum(image.rows-viewport.height);
        SetViewportXY(newPosX, newPosY); // set the top-left coordinates of viewport to new value
        ShowZoomValue(); // display new zoom value
        oldZoom = zoom; // backup zoom value
    }

    Mat image_temp = CopyFromImage(image, viewport);
    image_temp.copyTo(disp_color); // copy only the viewport part of image to display
    disp_color = 0;
    if (ui->checkBox_image->isChecked()) // image mask with transparency
        cv::addWeighted(disp_color, 1-double(ui->horizontalSlider_blend_image->value()) / 100,
                        image_temp, double(ui->horizontalSlider_blend_image->value()) / 100,
                        0, disp_color, -1);
    if (ui->checkBox_mask->isChecked() & (!mask.empty())) // mask with transparency
        cv::addWeighted(disp_color, 1,
                        CopyFromImage(mask, viewport), double(ui->horizontalSlider_blend_mask->value()) / 100,
                        0, disp_color, -1);
    if (ui->checkBox_grid->isChecked() & (!grid.empty())) // grid with transparency
        cv::addWeighted(disp_color, 1,
                        CopyFromImage(grid, viewport), double(ui->horizontalSlider_blend_grid->value()) / 100,
                        0, disp_color, -1);
    if (ui->checkBox_selection->isChecked() & (!selection.empty())) {
        Mat selection_temp = CopyFromImage(selection, viewport);
        if (zoom <= 1) selection_temp = DilatePixels(selection_temp, int(1/zoom)); // dilation
            else selection_temp = DilatePixels(selection_temp, 3-zoom);
        selection_temp.copyTo(disp_color, selection_temp);
        //bitwise_xor(disp_color, selection_temp, disp_color);
    }
    if (ui->checkBox_holes->isChecked() & (!mask.empty())) {// show holes in mask
        Mat holes = cv::Mat::zeros(mask.rows, mask.cols, CV_8UC3); // to draw the holes
        Mat1b holes_mask;
        cv::inRange(mask, Vec3b(0,0,0), Vec3b(0,0,0), holes_mask); // extract mask = unset label zones
        holes.setTo(Vec3b(255,255,255), holes_mask); // draw holes in white
        cv::addWeighted(disp_color, 1,
                        CopyFromImage(holes, viewport), 1,
                        0, disp_color, -1); // show the mask over the whole display
        // Count holes
        int nbHoles = countNonZero(holes_mask);
        ui->label_holes->setText(QString::number(nbHoles));
    }

    // display the viewport
    QPixmap D;
    D = Mat2QPixmapResized(disp_color, int(viewport.width*zoom), int(viewport.height*zoom), (zoom < 1)); // zoomed image
    ui->label_segmentation->setPixmap(D); // Set new image content to viewport

    DisplayThumbnail(); // update thumbnail view
}

void MainWindow::on_button_apply_clicked() // Preprocess image : equalize normalize gaussian blur etc
{
    if (!loaded) { // if image not loaded get out
        QMessageBox::warning(this, "Load an image first",
                                 "Not now!\n\nTry to load an image first");
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor); // Waiting cursor

    bool gaussian_blur = ui->checkBox_gaussian_blur->isChecked(); // Gaussian blur
    bool equalize = ui->checkBox_equalize->isChecked(); // Equalize
    bool normalize = ui->checkBox_normalize->isChecked(); // Normalize
    bool color_balance = ui->checkBox_color_balance->isChecked(); // Auto color balance
    int color_balance_percent = ui->spinBox_color_balance_percent->value(); //
    bool contours = ui->checkBox_contours->isChecked(); // Contours
    int contours_sigma = ui->spinBox_contours_sigma->value(); // contours threshold
    int contours_thickness = ui->spinBox_contours_thickness->value(); // contours ratio
    int contours_aperture = ui->comboBox_contours_aperture->currentIndex() * 2 + 3; // contours aperture
    bool denoise = ui->checkBox_denoise->isChecked(); // Denoise
    int luminance = ui->horizontalSlider_luminance->value();
    int chrominance = ui->horizontalSlider_chrominance->value();

    image_backup.copyTo(image); // restore original image

    if (denoise) { // denoise
        int template_window_size, search_window_size;
        double h;

        if (luminance <= 25) { // parameters change with luminance value
            h = luminance * 0.55;
            template_window_size = 3;
            search_window_size = 21;
        }
        else if (luminance <= 55) {
            h = luminance * 0.4;
            template_window_size = 5;
            search_window_size = 25;
        }
        else {
            h = luminance * 0.35;
            template_window_size = 7;
            search_window_size = 25;
        }

        fastNlMeansDenoisingColored(image, image, h, chrominance, template_window_size, search_window_size); // denoise image

        double psnr = PSNR(image_backup, image); // compute PSNR
        ui->label_psnr->setText(QString::number(psnr, 'f', 2)); // show PSNR
    }
    else ui->label_psnr->setText("");

    if (equalize) image = EqualizeHistogram(image); // equalize
    if (normalize) cv::normalize(image, image, 1, 255, NORM_MINMAX, -1); // normalize
    if (color_balance) image = SimplestColorBalance(image, color_balance_percent); // color balance
    if (gaussian_blur) cv::GaussianBlur(image, image, Size(3,3), 0, 0); // gaussian blur
    if (contours) { // contours
        Mat contours_mask;
        contours_mask = DrawColoredContours(image, double(contours_sigma) / 100, contours_aperture, contours_thickness); //compute contours
        contours_mask.copyTo(image, contours_mask); // copy contours to image view
    }

    cv::resize(image, thumbnail, Size(ui->label_thumbnail->pixmap()->width(),ui->label_thumbnail->pixmap()->height()),
               0, 0, INTER_AREA); // create thumbnail

    ShowSegmentation(); // show result

    QApplication::restoreOverrideCursor(); // Restore cursor
}

void MainWindow::on_button_original_clicked() // undo image pre-processing
{
    if (!loaded) { // if image not loaded get out
        QMessageBox::warning(this, "Load an image first",
                                 "Not now!\n\nTry to load an image and apply some filters first");
        return;
    }

    image_backup.copyTo(image); // revert to loaded image

    ShowSegmentation(); // show result
}

void MainWindow::on_button_compute_clicked() // compute segmentation
{
    if (this->image.empty()) // check image loaded
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor); // Waiting cursor

    // Initialize variables to compute
    int algorithm = ui->comboBox_algorithm->currentIndex(); // SLIC=0 SLICO=1 MSLIC=2 LSC=3 SEEDS=4
    int region_size = ui->horizontalSlider_region_size->value(); // SLIC SLICO MSLIC LSC average superpixel size
    int ruler = ui->horizontalSlider_ruler->value(); // SLIC MSLIC smoothness (spatial regularization)
    int min_element_size = ui->horizontalSlider_connectivity->value(); // SLIC SLICO MSLIC LSC minimum component size percentage
    int num_iterations = ui->horizontalSlider_iterations->value(); // Iterations
    int num_superpixels = ui->horizontalSlider_num_superpixels->value(); // SEEDS Number of Superpixels
    int num_levels = ui->horizontalSlider_num_levels->value(); // SEEDS Number of Levels
    int prior = ui->horizontalSlider_prior->value(); // SEEDS Smoothing Prior
    int num_histogram_bins = ui->horizontalSlider_num_histogram_bins->value(); // SEEDS histogram bins
    bool double_step = ui->checkBox_double_step->isChecked(); // SEEDS two steps
    double ratio = ui->doubleSpinBox_ratio->value(); // LSC compactness
    bool lab = ui->checkBox_lab_colors->isChecked(); // convert to LAB colorimetry
    cv::Vec3b maskColor; // BGR
    switch (ui->comboBox_grid_color->currentIndex()) { // color to set the grid to
        case 0: maskColor = Vec3b(0,0,255);break; // Red
        case 1: maskColor = Vec3b(0,255,0);break; // Green
        case 2: maskColor = Vec3b(255,0,0);break; // Blue
        case 3: maskColor = Vec3b(255,255,0);break; // Cyan
        case 4: maskColor = Vec3b(255,0,255);break; // Magenta
        case 5: maskColor = Vec3b(0,255,255);break; // Yellow
        case 6: maskColor = Vec3b(255,255,255);break; // White
    }

    Mat converted;
    image.copyTo(converted); // copy of image to convert to LAB or HSV

    if (lab) cv::cvtColor(converted, converted, COLOR_BGR2Lab); // seems to work better with LAB colors space
        else cv::cvtColor(converted, converted, COLOR_BGR2HSV); // HSV color space

    //// Compute

    Mat maskTemp;

    // Compute superpixels with connectivity and iterations
    if (algorithm <= 2) { // SLIC SLICO MSLIC
        slic = createSuperpixelSLIC(converted, algorithm+SLIC, region_size, float(ruler)); // Create superpixels container
        slic->iterate(num_iterations); // iterations
        if (min_element_size > 0) // connectivity
            slic->enforceLabelConnectivity(min_element_size);
        nb_cells = slic->getNumberOfSuperpixels(); // number of labels
        slic->getLabels(labels); // get labels
        slic->getLabelContourMask(maskTemp, !ui->checkBox_thick->isChecked()); // contours
    } else
    if (algorithm == 3) { // LSC
        lsc = createSuperpixelLSC(converted, region_size, ratio); // Create superpixels container
        lsc->iterate(num_iterations); // iterations
        if (min_element_size>0) // connectivity
            lsc->enforceLabelConnectivity(min_element_size);
        nb_cells = lsc->getNumberOfSuperpixels(); // number of labels
        lsc->getLabels(labels); // get labels
        lsc->getLabelContourMask(maskTemp, !ui->checkBox_thick->isChecked()); // contours
    } else
    if (algorithm == 4) { //SEEDS
        seeds = createSuperpixelSEEDS(image.cols, image.rows, converted.channels(), num_superpixels, num_levels, prior, num_histogram_bins, double_step); // Create superpixels container
        seeds->iterate(converted, num_iterations); // iterations
        nb_cells = seeds->getNumberOfSuperpixels(); // number of labels
        seeds->getLabels(labels); // get labels
        seeds->getLabelContourMask(maskTemp, !ui->checkBox_thick->isChecked()); // contours
    }

    labels_mask = Mat::zeros(labels.rows, labels.cols, CV_32SC1); // new labels mask

    // contours for displaying : grid & mask initialization
    cv::cvtColor(maskTemp, mask, COLOR_GRAY2RGB); // at first the mask contains the grid, need to convert labels from gray to RGB
    mask.setTo(0); // reinit mask
    cv::cvtColor(maskTemp, grid, COLOR_GRAY2RGB); // the same for the grid
    grid.setTo(maskColor, grid); // change grid color

    selection = Mat::zeros(image.rows, image.cols, CV_8UC3); // create selection mask

    // Visualization
    computed = true; // Indicate that a segmentation has been computed
    ui->lcd_cells->setPalette(Qt::green); // computed = green LCD
    ui->lcd_cells->display(nb_cells+1); // display count with LCD

    // convert the image to a QPixmap and display it
    ShowSegmentation(); // update display

    ui->Tabs->setTabEnabled(2, true); // computed = we can edit labels

    QApplication::restoreOverrideCursor(); // Restore cursor
}

void MainWindow::WhichCell(int posX, int posY) // for (posX,posY) in image, colorize the corresponding label
{
    if (labels.empty()) // of course if labels have been computed
        return;

    int label = labels.at<int>(posY,posX); // find pixel label
    Mat1b superpixel_mask = labels == label; // extract label

    mask.setTo(0, superpixel_mask); // erase label in the mask
    labels_mask.setTo(0, superpixel_mask); // delete label from labels mask

    if (mouseButton == Qt::LeftButton) {
        mask.setTo(color, superpixel_mask); // set color to cell in the mask
        labels_mask.setTo(GetCurrentLabelNumber(), superpixel_mask); // claim cell in labels mask
    }

    ShowSegmentation(); // show result
    return;
}
