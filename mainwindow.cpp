/*#-------------------------------------------------
#
#        OpenCV Superpixels Segmentation
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#                v1 - 2018/08/22
#
#-------------------------------------------------*/

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QMouseEvent>
#include <QScrollBar>
#include <QCursor>

#include "mat-image-tools.h"

/////////////////// Window init //////////////////////

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Populate combo lists
    ui->comboBox_algorithm->addItem(tr("SLIC")); // algorithms
    ui->comboBox_algorithm->addItem(tr("SLICO"));
    ui->comboBox_algorithm->addItem(tr("MSLIC"));
    ui->comboBox_algorithm->addItem(tr("LSC"));
    ui->comboBox_algorithm->addItem(tr("SEEDS"));

    ui->comboBox_grid_color->addItem(tr("Red")); // grid colors
    ui->comboBox_grid_color->addItem(tr("Green"));
    ui->comboBox_grid_color->addItem(tr("Blue"));
    ui->comboBox_grid_color->addItem(tr("Cyan"));
    ui->comboBox_grid_color->addItem(tr("Magenta"));
    ui->comboBox_grid_color->addItem(tr("Yellow"));
    ui->comboBox_grid_color->addItem(tr("White"));

    // LCD
    ui->lcd_cells->setPalette(Qt::red);

    // Contours
    ui->spinBox_contours_sigma->setVisible(false);
    ui->spinBox_contours_thickness->setVisible(false);
    ui->comboBox_contours_aperture->setVisible(false);

    // Global variables init
    computed = false; // segmentation not yet computed
    color = Vec3b(0,0,255); // pen color
    zoom = 1; // init zoom
    oldZoom = 1; // to detect a zoom change
    zoom_type = "";
}

MainWindow::~MainWindow()
{
    delete ui;
}

/////////////////// Save and load //////////////////////

void MainWindow::on_button_image_clicked() // Load main image
{
    QString filename = QFileDialog::getOpenFileName(this, "Select picture file", "/media/DataI5/Photos/Sabine", "Images (*.jpg *.JPG *.jpeg *.JPEG *.jp2 *.JP2 *.png *.PNG *.tif *.TIF *.tiff *.TIFF *.bmp *.BMP)");
    if (filename.isNull() || filename.isEmpty())
        return;

    basefile = filename.toUtf8().constData(); // base file name and dir are used after to save other files
    basefile = basefile.substr(0, basefile.size()-4);
    basedir = basefile;
    size_t found = basefile.find_last_of("/");
    basedir = basedir.substr(0,found) + "/";
    basefile = basefile.substr(found+1);
    ui->label_filename->setText(filename); // display file name

    std::string filename_s = filename.toUtf8().constData(); // convert filename from QString

    cv::Mat mat = cv::imread(filename_s, CV_LOAD_IMAGE_COLOR); // Load image
    ui->label_thumbnail->setPixmap(Mat2QPixmapResized(mat, ui->label_thumbnail->width(), ui->label_thumbnail->height())); // Display image

    image = mat; // stores the image for further use
    mat.copyTo(backup);

    computed = false; // depthmap not yet computed

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

    double zoomX = double(ui->label_segmentation->width()) / image.cols; // try vertical and horizontal ratios
    double zoomY = double(ui->label_segmentation->height()) / image.rows;
    if (zoomX < zoomY) zoom = zoomX; // the lowest fut the view
        else zoom = zoomY;
    oldZoom = zoom; // for center view
    ShowZoomValue(); // display current zoom
    viewport = Rect(0, 0, image.cols, image.rows); // update viewport

    cv::resize(image, thumbnail, Size(ui->label_thumbnail->pixmap()->width(),ui->label_thumbnail->pixmap()->height()),
               0, 0, INTER_AREA); // create thumbnail
}

void MainWindow::on_button_save_session_clicked() // save session files
{
    if (mask.empty()) // no mask = nothing to save
            return;

    QString dirname = QFileDialog::getExistingDirectory(this, "Save segmentation session to directory...", "./" + QString::fromStdString(basedir), 0);

    if (dirname.isNull() || dirname.isEmpty()) // cancel ?
        return;

    dirname += "/";
    std::string dirname_s = dirname.toUtf8().constData(); // convert from QString

    cv::imwrite(dirname_s + basefile + "-segmentation-mask.png", mask); // save mask
    cv::imwrite(dirname_s + basefile + "-segmentation-grid.png", grid); // save grid
    cv::imwrite(dirname_s + basefile + "-segmentation-image.png", image); // save processed image
    cv::FileStorage fs(dirname_s + basefile + "-segmentation-labels.xml", cv::FileStorage::WRITE); // save labels
    fs << "Labels" << labels;

    QMessageBox::information(this, "Save segmentation session", "Session saved with base name:\n" + dirname + QString::fromStdString(basefile));
}

void MainWindow::on_button_load_session_clicked() // load previous session
{
    if (image.empty()) // image mandatory
            return;

    QString dirname = QFileDialog::getExistingDirectory(this, "Load session from directory...", "./" + QString::fromStdString(basedir), 0);

    if (dirname.isNull() || dirname.isEmpty()) // cancel ?
        return;

    dirname += "/";
    std::string dirname_s = dirname.toUtf8().constData(); // convert from QString

    mask = cv::imread(dirname_s + basefile + "-segmentation-mask.png", CV_LOAD_IMAGE_COLOR); // load mask
    grid = cv::imread(dirname_s + basefile + "-segmentation-grid.png"); // load grid
    image = cv::imread(dirname_s + basefile + "-segmentation-image.png"); // load processed image
    Mat labels_temp(image.rows, image.cols, CV_32SC1); // labels on 1 channel
    cv::FileStorage fs2(dirname_s + basefile + "-segmentation-labels.xml", FileStorage::READ); // load labels
    fs2["Labels"] >> labels_temp;
    labels_temp.copyTo(labels);

    computed = true; // not really computed but necessary
    ShowSegmentation(); // update display

    QMessageBox::information(this, "Load segmentation session", "Session loaded with base name:\n" + dirname + QString::fromStdString(basefile));
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
    bool contours = ui->checkBox_contours->isChecked();
    int contours_sigma = ui->spinBox_contours_sigma->value();
    int contours_thickness = ui->spinBox_contours_thickness->value();
    int contours_aperture = ui->comboBox_contours_aperture->currentIndex() * 2 + 3;

    FileStorage fs(filename_s, FileStorage::WRITE); // save to openCV XML file type
    fs << "Algorithm" << algorithm;
    fs << "RegionSize" << region_size;
    fs << "Ruler" << ruler;
    fs << "Connectivity" << min_element_size;
    fs << "NumIterations" << num_iterations;
    fs << "Color" << line_color;
    fs << "ThickLines" << thick_lines;
    fs << "Superpixels" << num_superpixels;
    fs << "Levels" << num_levels;
    fs << "Prior" << prior;
    fs << "Histograms" << num_histogram_bins;
    fs << "DoubleStep" << double_step;
    fs << "Ratio" << ratio;
    fs << "LabColors" << lab;
    fs << "GaussianBlur" << gaussian_blur;
    fs << "Normalize" << normalize;
    fs << "Equalize" << equalize;
    fs << "Contours" << contours;
    fs << "ContoursSigma" << contours_sigma;
    fs << "ContoursThickness" << contours_thickness;
    fs << "ContoursAperture" << contours_aperture;

    QMessageBox::information(this, "Save configuration", "Configuration saved to:\n" + filename);
}

void MainWindow::on_button_load_conf_clicked() // load configuration
{
    QString filename = QFileDialog::getOpenFileName(this, "Select XML configuration file", QString::fromStdString(basedir + basefile + "-segmentation-conf.xml"), "XML parameters (*.xml *.XML)");
    if (filename.isNull()) // cancel ?
        return;

    std::string filename_s = filename.toUtf8().constData(); // convert filename from QString to std::string

    // Initialize variables to save
    int algorithm, region_size, ruler, min_element_size, num_iterations, line_color,
        num_superpixels, num_levels, prior, num_histogram_bins, contours_sigma, contours_thickness, contours_aperture;
    double ratio;
    bool thick_lines, double_step, lab, gaussian_blur, equalize, normalize, contours;

    FileStorage fs(filename_s, FileStorage::READ); // load from openCV XML file
    fs["Algorithm"] >> algorithm;
    fs["RegionSize"] >> region_size;
    fs["Ruler"] >> ruler;
    fs["Connectivity"] >> min_element_size;
    fs["NumIterations"] >> num_iterations;
    fs["Color"] >> line_color;
    fs["ThickLines"] >> thick_lines;
    fs["Superpixels"] >> num_superpixels;
    fs["Levels"] >> num_levels;
    fs["Prior"] >> prior;
    fs["Histograms"] >> num_histogram_bins;
    fs["DoubleStep"] >> double_step;
    fs["Ratio"] >> ratio;
    fs["LabColors"] >> lab;
    fs["GaussianBlur"] >> gaussian_blur;
    fs["Normalize"] >> normalize;
    fs["Equalize"] >> equalize;
    fs["Contours"] >> contours;
    fs["ContoursSigma"] >> contours_sigma;
    fs["ContoursThickness"] >> contours_thickness;
    fs["ContoursAperture"] >> contours_aperture;

    ui->comboBox_algorithm->setCurrentIndex(algorithm); // update GUI values
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
    ui->checkBox_contours->setChecked(contours);
    ui->spinBox_contours_sigma->setValue(contours_sigma);
    ui->spinBox_contours_thickness->setValue(contours_thickness);
    ui->comboBox_contours_aperture->setCurrentIndex((contours_aperture - 3) / 2);

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

void MainWindow::on_horizontalSlider_blend_mask_valueChanged(int value) // Refresh image : blend mask transparency changed
{
    ShowSegmentation(); // update display
}

void MainWindow::on_horizontalSlider_blend_image_valueChanged(int value) // Refresh image : image mask transparency changed
{
    ShowSegmentation(); // update display
}

void MainWindow::on_horizontalScrollBar_segmentation_valueChanged(int value) // update viewport
{
    viewport.x = ui->horizontalScrollBar_segmentation->value(); // new value
    ShowSegmentation(); // update display
}

void MainWindow::on_verticalScrollBar_segmentation_valueChanged(int value)  // update viewport
{
    viewport.y = ui->verticalScrollBar_segmentation->value(); // new value
    ShowSegmentation(); // update display
}

void MainWindow::on_comboBox_grid_color_currentIndexChanged(int index) // Change grid color
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
    cv::cvtColor(grid, maskTemp, CV_RGB2GRAY); // convert grid to gray values
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

void MainWindow::ShowZoomValue()
{
    ui->label_zoom->setText(QString::number(int(zoom * 100)) +"%"); // show new zoom value in percent
}

/////////////////// Undo //////////////////////

void MainWindow::on_button_undo_clicked() // Undo last action
{
    undo.copyTo(mask); // get back !
    ShowSegmentation(); // update display
}

/////////////////// Algorithm parameters visibility //////////////////////

void MainWindow::on_checkBox_contours_clicked() // zoom 100%
{
    bool checked = ui->checkBox_contours->isChecked(); // checkbox state
    ui->spinBox_contours_sigma->setVisible(checked); // hide or reveal parameters
    ui->spinBox_contours_thickness->setVisible(checked);
    ui->comboBox_contours_aperture->setVisible(checked);
}

void MainWindow::on_comboBox_algorithm_currentIndexChanged(int index) // algorithm changed : hide/show options
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

/////////////////// Edit colors //////////////////////
// All following functions are the same, only the BGR colors change

void MainWindow::on_pushButton_color_red_clicked() //Red
{
    color = Vec3b(0,0,255); // chosen color
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(255,0,0);border: 3px solid black; }"); // update current color
    ui->label_color->setText("RED"); // show color name
}

void MainWindow::on_pushButton_color_green_clicked() // Green
{
    color = Vec3b(0,255,0);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(0,255,0);border: 3px solid black; }");
    ui->label_color->setText("GREEN");
}

void MainWindow::on_pushButton_color_blue_clicked() // Blue
{
    color = Vec3b(255,0,0);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(0,0,255);border: 3px solid black; }");
    ui->label_color->setText("BLUE");
}

void MainWindow::on_pushButton_color_cyan_clicked() // Cyan
{
    color = Vec3b(255,255,0);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(0,255,255);border: 3px solid black; }");
    ui->label_color->setText("CYAN");
}

void MainWindow::on_pushButton_color_magenta_clicked() // Magenta
{
    color = Vec3b(255,0,255);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(255,0,255);border: 3px solid black; }");
    ui->label_color->setText("MAGENTA");
}

void MainWindow::on_pushButton_color_yellow_clicked() // Yellow
{
    color = Vec3b(0,255,255);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(255,255,0);border: 3px solid black; }");
    ui->label_color->setText("YELLOW");
}

void MainWindow::on_pushButton_color_white_clicked() // White
{
    color = Vec3b(255,255,255);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(255,255,255);border: 3px solid black; }");
    ui->label_color->setText("WHITE");
}

void MainWindow::on_pushButton_color_orange_clicked() // Orange
{
    color = Vec3b(0,128,255);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(255,128,0);border: 3px solid black; }");
    ui->label_color->setText("ORANGE");
}

void MainWindow::on_pushButton_color_brown_clicked() // Brown
{
    color = Vec3b(0,0,128);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(128,0,0);border: 3px solid black; }");
    ui->label_color->setText("BROWN");
}

void MainWindow::on_pushButton_color_olive_clicked() // Olive = dark green
{
    color = Vec3b(0,128,0);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(0,128,0);border: 3px solid black; }");
    ui->label_color->setText("OLIVE");
}

void MainWindow::on_pushButton_color_navy_clicked() // Navy blue
{
    color = Vec3b(128,0,0);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(0,0,128);border: 3px solid black; }");
    ui->label_color->setText("NAVY");
}

void MainWindow::on_pushButton_color_emerald_clicked() // Emerald = blue / green
{
    color = Vec3b(128,128,0);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(0,128,128);border: 3px solid black; }");
    ui->label_color->setText("EMERALD");
}

void MainWindow::on_pushButton_color_purple_clicked() // Purple
{
    color = Vec3b(255,0,128);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(128,0,255);border: 3px solid black; }");
    ui->label_color->setText("PURPLE");
}

void MainWindow::on_pushButton_color_gray_clicked() // Gray
{
    color = Vec3b(128,128,128);
    ui->label_color->setStyleSheet("QLabel { background-color:rgb(128,128,128);border: 3px solid black; }");
    ui->label_color->setText("GRAY");
}

/////////////////// Mouse events //////////////////////

void MainWindow::mousePressEvent(QMouseEvent *eventPress) // event triggered by a mouse click
{
    if (!computed) // nothing to do
        return;

    if (ui->label_segmentation->underMouse()) { // if the mouse is over the viewport
        mouse_origin = ui->label_segmentation->mapFromGlobal(QCursor::pos()); // mouse position
        Point pos = Viewport2Image(Point(mouse_origin.x(), mouse_origin.y())); // convert from viewport to image coordinates
        mouseButton = eventPress->button(); // mouse button value
        bool key_control = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier); // modifier keys pressed ? (shift control etc)
        bool key_shift = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier);
        bool key_alt = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier);
        bool key_meta = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::MetaModifier);

        if (key_alt) { // <alt> pressed at the same time = fill function
            mask.copyTo(undo); // for undo
            Vec3b temp_color;
            if (mouseButton == Qt::LeftButton) temp_color = color; else temp_color = 0; // left button=color / right button=delete
            floodFill(mask, Point(pos.x, pos.y), temp_color); // fill the area with the color
            ShowSegmentation(); // display the result
        }
        else {
            // Color already set before ? saves CPU time
            Vec3b pixel = mask.at<Vec3b>(pos.y,pos.x); // find mask pixel color
            if ((mouseButton == Qt::LeftButton) & (pixel == color)) // left button : compare pixel with current color
                return; // color already set
            else if ((mouseButton == Qt::RightButton) & (pixel == Vec3b(0,0,0))) // right button : compare pixel with 0 (delete)
                return;  // color already set

            WhichCell(pos.x, pos.y); // Show which cell has been clicked
        }
    }  else {
        if (ui->label_thumbnail->underMouse()) { // if the mouse is over the thumbnail
            QPoint mouse_pos = ui->label_thumbnail->mapFromGlobal(QCursor::pos()); // current mouse position
            mouseButton = eventPress->button(); // mouse button value

            if (mouseButton == Qt::LeftButton) { // move view
                // convert middle of viewport from thumbnail to image coordinates
                int middleX = double(mouse_pos.x() - (ui->label_thumbnail->width() - ui->label_thumbnail->pixmap()->width()) / 2) / ui->label_thumbnail->pixmap()->width() * image.cols;
                int middleY = double(mouse_pos.y() - (ui->label_thumbnail->height() - ui->label_thumbnail->pixmap()->height()) / 2) / ui->label_thumbnail->pixmap()->height() * image.rows;
                SetViewportXY(middleX - viewport.width /2, middleY - viewport.height /2); // set viewport to new middle position

                ShowSegmentation(); // display the result
                return;
            }
        }
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *eventMove) // one mouse click has already been treated and is holded down
{
    if (!computed) // nothing to do
        return;

    if (ui->label_segmentation->underMouse()) { // if the mouse is over the viewport
        QPoint mouse_pos = ui->label_segmentation->mapFromGlobal(QCursor::pos()); // current mouse position

        if (mouseButton == Qt::MidButton) { // move view
            int decX = mouse_pos.x() - mouse_origin.x(); // distance from the first click
            int decY = mouse_pos.y() - mouse_origin.y();
            SetViewportXY(viewport.x - decX, viewport.y - decY); // update viewport

            ShowSegmentation(); // display the result
        }
        else {
            Point pos = Viewport2Image(Point(mouse_pos.x(), mouse_pos.y())); // convert position from viewport to image coordinates

            // Color already set before ? saves CPU time
            Vec3b pixel = mask.at<Vec3b>(pos.y,pos.x); // find mask pixel color
            if ((mouseButton == Qt::LeftButton) & (pixel == color)) // left button : compare pixel with current color
                return; // color already set
            else if ((mouseButton == Qt::RightButton) & (pixel == Vec3b(0,0,0))) // right button : compare pixel with 0
                return;  // color already set

            WhichCell(pos.x, pos.y); // Show which cell has been clicked
        }
    }
}

void MainWindow::wheelEvent(QWheelEvent *wheelEvent) // mouse wheel turned
{
    if (!computed) // nothing to do
        return;

    if (ui->label_segmentation->underMouse()) { // if the mouse is over the viewport
        mouse_origin = ui->label_segmentation->mapFromGlobal(QCursor::pos()); // mouse position
        Point pos = Viewport2Image(Point(mouse_origin.x(), mouse_origin.y())); // convert from viewport to image coordinates
        mouse_origin = QPoint(pos.x, pos.y);
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

Point MainWindow::Viewport2Image(Point p) // convert viewport to image coordinates
{
    int posX = double(p.x - (ui->label_segmentation->width() - ui->label_segmentation->pixmap()->width()) / 2) / zoom + viewport.x;
    int posY = double(p.y - (ui->label_segmentation->height() - ui->label_segmentation->pixmap()->height()) / 2) / zoom + viewport.y;
    return Point(posX, posY);
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
    else {
        p1x = double(viewport.x) / image.cols;
        p2x = p1x + double(viewport.width) / image.cols;
        p1x *= double(ui->label_thumbnail->pixmap()->width());
        p2x *= double(ui->label_thumbnail->pixmap()->width());
    }

    if (viewport.height == image.rows) { // entire vertical view ?
        p1y = 0;
        p2y= ui->label_thumbnail->pixmap()->height();
    }
    else {
        p1y = double(viewport.y) / image.rows;
        p2y = p1y + double(viewport.height) / image.rows;
        p1y *= double(ui->label_thumbnail->pixmap()->height());
        p2y *= double(ui->label_thumbnail->pixmap()->height());
    }

    Mat thumbnail_temp;
    thumbnail.copyTo(thumbnail_temp);
    thumbnail_temp = 0;
    rectangle(thumbnail_temp, Point(p1x, p1y), Point(p2x-1, p2y-1), Scalar(92,92,92), -1); // draw filled rectangle representing the viewport position
    rectangle(thumbnail_temp, Point(p1x, p1y), Point(p2x-1, p2y-1), Scalar(255,255,255), 2); // draw rectangle with thick border
    cv::addWeighted(thumbnail, 1, thumbnail_temp, 0.25, 0, thumbnail_temp, -1); // with transparency
    ui->label_thumbnail->setPixmap(Mat2QPixmap(thumbnail_temp)); // update view
}

void MainWindow::UpdateViewportDimensions() // recomputes the viewport width and height
{
    viewport.width = double(ui->label_segmentation->width()) / zoom;
    if (viewport.width > image.cols) {
        viewport.width = image.cols;
        viewport.x = 0;
    }
    viewport.height = double(ui->label_segmentation->height()) / zoom;
    if (viewport.height > image.rows) {
        viewport.height = image.rows;
        viewport.y = 0;
    }
    if (viewport.x > image.cols - viewport.width) viewport.x = image.cols - viewport.width;
    if (viewport.y > image.rows - viewport.height) viewport.y = image.rows - viewport.height;
}

void MainWindow::SetViewportXY(int x, int y) // set viewport top-left to (x,y) new coordinates
{
    viewport.x = x;
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
    if (!computed) // nah !
        return;

    int oldMiddleX, oldMiddleY;
    if (oldZoom != zoom) { // zoom has changed ?
        oldMiddleX = viewport.x + viewport.width / 2; // current middle of viewport for zooming to the center of image
        oldMiddleY = viewport.y + viewport.height / 2;
        if (zoom_type == "wheel") {
            oldMiddleX = mouse_origin.x();
            oldMiddleY = mouse_origin.y();
            QCursor cursor;
            cursor.setPos(ui->label_segmentation->mapToGlobal(ui->label_segmentation->rect().center()));
            setCursor(cursor);
            zoom_type = "";
        }
        UpdateViewportDimensions(); // recompute viewport width and height
        double newPosX = oldMiddleX - viewport.width / 2; // compute new middle of viewport
        double newPosY = oldMiddleY - viewport.height / 2;
        ui->horizontalScrollBar_segmentation->setMaximum(image.cols-viewport.width); // update scrollbars range
        ui->verticalScrollBar_segmentation->setMaximum(image.rows-viewport.height);
        SetViewportXY(newPosX, newPosY); // set the top-left coordinates of viewport to new value
        ShowZoomValue(); // display new zoom value
        oldZoom = zoom;
    }

    // Combine image + mask + grid
    CopyFromImage(image, viewport).copyTo(disp_color); // copy only the viewport part of image
    if (!ui->checkBox_image->isChecked()) disp_color = 0; // image
    if (ui->checkBox_mask->isChecked() & !mask.empty()) { // mask
        cv::addWeighted(disp_color, double(ui->horizontalSlider_blend_image->value()) / 100, CopyFromImage(mask, viewport),
                        double(ui->horizontalSlider_blend_mask->value()) / 100, 0, disp_color, -1); // with transparency
    }
    if (ui->checkBox_grid->isChecked() & !grid.empty()) { // grid
        if (zoom <= 1) { // dilate grid lines only if zoom out so they won't disappear when we zoom out
            Mat dilate_grid;
            grid.copyTo(dilate_grid);
            dilate_grid = DilatePixels(dilate_grid, int(1/zoom)-2); // dilation
            disp_color += CopyFromImage(dilate_grid, viewport); // update view
        }
        else disp_color += CopyFromImage(grid, viewport); // update image displayed
    }

    QPixmap D;
    D = Mat2QPixmapResized(disp_color, int(viewport.width*zoom), int(viewport.height*zoom)); // zoomed image
    ui->label_segmentation->setPixmap(D); // Set new image content to viewport
    DisplayThumbnail(); // update thumbnail view

    //qApp->processEvents(); // Force repaint;
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
    bool gaussian_blur = ui->checkBox_gaussian_blur->isChecked(); // Gaussian blur image before computing
    bool equalize = ui->checkBox_equalize->isChecked(); // Equalize image before computing
    bool normalize = ui->checkBox_normalize->isChecked(); // Normalize image before computing
    bool contours = ui->checkBox_contours->isChecked(); // Find image edges before computing
    int contours_sigma = ui->spinBox_contours_sigma->value(); // contours threshold
    int contours_thickness = ui->spinBox_contours_thickness->value(); // contours ratio
    int contours_aperture = ui->comboBox_contours_aperture->currentIndex() * 2 + 3; // contours aperture
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

    backup.copyTo(image); // restore original image

    //// Preprocess image : equalize normalize gaussian blur

    if (equalize) image = EqualizeHistogram(image); // equalize ?
    if (normalize) cv::normalize(image, image, 1, 255, NORM_MINMAX, -1); // normalize ?
    if (gaussian_blur) cv::GaussianBlur(image, image, Size(3,3), 0, 0); // gaussian blue ?
    if (contours) { // contours ?
        Mat contours_mask;
        contours_mask = DrawColoredContours(image, double(contours_sigma) / 100, contours_aperture, contours_thickness); //compute contours
        contours_mask.copyTo(image, contours_mask); // copy contours to image view
    }

    Mat converted;
    image.copyTo(converted); // copy of image to convert to LAB or HSV

    if (lab) cv::cvtColor(converted, converted, CV_BGR2Lab); // seems to work better with LAB colors space
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

    // contours for displaying : grid & mask initialization
    cv::cvtColor(maskTemp, mask, CV_GRAY2RGB); // at first the mask contains the grid, need to convert labels from gray to RGB
    mask.setTo(0); // reinit mask
    cv::cvtColor(maskTemp, grid, CV_GRAY2RGB); // the same for the grid
    grid.setTo(maskColor, grid); // change grid color

    // Visualization
    computed = true; // Indicate that a segmentation has been computed
    ui->lcd_cells->setPalette(Qt::green); // computed = green LCD
    ui->lcd_cells->display(nb_cells+1); // display count with LCD

    // convert the image to a QPixmap and display it
    ShowSegmentation(); // update display

    QApplication::restoreOverrideCursor(); // Restore cursor
}

void MainWindow::WhichCell(int posX, int posY) // for (posX,posY) in image, colorize the corresponding label
{
    if (labels.empty()) // of course if labels have been computed
        return;

    mask.copyTo(undo); // save the current mask for undo function

    int label = labels.at<int>(posY,posX); // find pixel label
    Mat1b superpixel_mask = labels == label; // extract label

    mask.setTo(0, superpixel_mask); // erase label in the mask
    if (mouseButton == Qt::LeftButton) mask.setTo(color, superpixel_mask); // set color to cell in the mask

    ShowSegmentation(); // display the result
    return;
}
