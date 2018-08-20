/*
 * OpenCV image tools library
 * Author: AbsurdePhoton
 *
 * v1.3 - 2018/08/20
 *
 * Convert mat images to QPixmap or QImage
 * Set brightness and contrast
 * Equalize color image histograms
 * Erosion / dilation of pixels
 * Copy part of image
 * Contours using Canny algorithm
 *
 */

#include <QPixmap>

#include "opencv2/opencv.hpp"

using namespace std;
using namespace cv;

enum Direction{ShiftUp=1, ShiftRight, ShiftDown, ShiftLeft}; // directions for shift function

QImage Mat2QImage(cv::Mat const& src) // convert Mat to QImage
{
     cv::Mat temp;
     cv::cvtColor(src, temp,CV_BGR2RGB); // convert Mat BGR to QImage RGB
     QImage dest((const uchar *) temp.data, temp.cols, temp.rows, temp.step, QImage::Format_RGB888); // conversion
     dest.bits(); // enforce deep copy of QImage::QImage ( const uchar * data, int width, int height, Format format )
     return dest;
}

QPixmap Mat2QPixmap(cv::Mat source) // convert Mat to QPixmap
{
    QImage i = Mat2QImage(source); // first step: convert to QImage
    QPixmap p = QPixmap::fromImage(i, Qt::AutoColor); // then convert QImage to QPixmap
    return p;
}

QPixmap Mat2QPixmapResized(cv::Mat source, int max_width, int max_height) // convert Mat to resized QPixmap
{
    QImage i = Mat2QImage(source); // first step: convert to QImage
    QPixmap p = QPixmap::fromImage(i, Qt::AutoColor); // then convert QImage to QPixmap
    return p.scaled(max_width, max_height, Qt::KeepAspectRatio, Qt::SmoothTransformation); // resize with high quality
}

Mat BrightnessContrast(Mat source, double alpha, int beta) // set brightness and contrast
{
    Mat image = Mat::zeros(source.size(), source.type());

    // Do the operation new_image(i,j) = alpha*image(i,j) + beta
    for (int y = 0; y < source.rows; y++) // scan entire Mat
            for (int x = 0; x < source.cols; x++)
                for (int c = 0; c < 3; c++)
                    image.at<Vec3b>(y, x)[c] = saturate_cast<uchar>(alpha*(source.at<Vec3b>(y, x)[c]) + beta); // change individual values
    return image;
}

QImage cvMatToQImage(const cv::Mat &inMat) // another implementation
{
    switch (inMat.type()) {
        case CV_8UC4: { // 8-bit, 4 channel
            QImage image(   inMat.data,
                            inMat.cols, inMat.rows,
                            static_cast<int>(inMat.step),
                            QImage::Format_ARGB32);
            return image;
        }


        case CV_8UC3: { // 8-bit, 3 channel
            QImage image(   inMat.data,
                            inMat.cols, inMat.rows,
                            static_cast<int>(inMat.step),
                            QImage::Format_RGB888);
            return image.rgbSwapped();
        }


        case CV_8UC1: { // 8-bit, 1 channel
            #if QT_VERSION >= 0x050500
                QImage image(inMat.data, inMat.cols, inMat.rows,
                             static_cast<int>(inMat.step),
                             QImage::Format_Grayscale8);
            #else
                static QVector<QRgb>  sColorTable;
                if (sColorTable.isEmpty()) { // only create our color table the first time
                    sColorTable.resize(256);
                    for (int i = 0; i < 256; ++i) sColorTable[i] = qRgb(i, i, i);
                }
                QImage image(   inMat.data,
                                inMat.cols, inMat.rows,
                                static_cast<int>(inMat.step),
                                QImage::Format_Indexed8);
                image.setColorTable(sColorTable);
            #endif
        }
    }
    return QImage();
}

Mat EqualizeHistogram(cv::Mat image) // histogram equalization
{
    Mat hist_equalized_image;
    cvtColor(image, hist_equalized_image, COLOR_BGR2YCrCb); // Convert the image from BGR to YCrCb color space

    // Split image into 3 channels: Y, Cr and Cb channels respectively and store it in a std::vector
    vector<Mat> vec_channels;
    split(hist_equalized_image, vec_channels);

    equalizeHist(vec_channels[0], vec_channels[0]); // Equalize the histogram of only the Y channel

    merge(vec_channels, hist_equalized_image); // Merge 3 channels in the vector to form the color image in YCrCB color space.

    cvtColor(hist_equalized_image, hist_equalized_image, COLOR_YCrCb2BGR); // Convert the histogram equalized image from YCrCb to BGR color space again

    return hist_equalized_image;
}

Mat DilatePixels(cv::Mat image, int dilation_size) // dilate pixels
{
    if (dilation_size < 0)
        return image;

    // Create a structuring element
    Mat element = cv::getStructuringElement(  cv::MORPH_RECT, // MORPH_CROSS MORPH_RECT & MORPH_ELLIPSE possible
                                              cv::Size(2 * dilation_size + 1, 2 * dilation_size + 1),
                                              cv::Point(-1,-1));
    Mat dst;
    cv::dilate(image, dst, element); // Apply dilation on the image

    return dst;
}

Mat ErodePixels(cv::Mat image, int erosion_size) // erode pixels
{
    if (erosion_size < 0)
        return image;

    // Create a structuring element
    Mat element = cv::getStructuringElement(  cv::MORPH_RECT, // MORPH_CROSS MORPH_RECT & MORPH_ELLIPSE
                                              cv::Size(2 * erosion_size + 1, 2 * erosion_size + 1),
                                              cv::Point(-1,-1));
    Mat dst;
    cv::erode(image, dst, element); // Apply erosion on the image

    return dst;
}

Mat ShiftFrame(cv::Mat frame, int pixels, Direction direction) // shift frame in one direction
{
    Mat temp = Mat::zeros(frame.size(), frame.type()); //create a same sized temporary Mat with all the pixels flagged as invalid (-1)

    switch (direction) {
        case(ShiftUp) :
            frame(cv::Rect(0, pixels, frame.cols, frame.rows - pixels)).copyTo(temp(cv::Rect(0, 0, temp.cols, temp.rows - pixels)));
            break;
        case(ShiftRight) :
            frame(cv::Rect(0, 0, frame.cols - pixels, frame.rows)).copyTo(temp(cv::Rect(pixels, 0, frame.cols - pixels, frame.rows)));
            break;
        case(ShiftDown) :
            frame(cv::Rect(0, 0, frame.cols, frame.rows - pixels)).copyTo(temp(cv::Rect(0, pixels, frame.cols, frame.rows - pixels)));
            break;
        case(ShiftLeft) :
            frame(cv::Rect(pixels, 0, frame.cols - pixels, frame.rows)).copyTo(temp(cv::Rect(0, 0, frame.cols - pixels, frame.rows)));
            break;
        default:
            std::cout << "Shift direction is not set properly" << std::endl;
    }
    return temp;
}

Mat CopyFromImage (cv::Mat source, cv::Rect frame) // copy part of an image
{
    return source(frame);
}

Mat DrawColoredContours(cv::Mat source, int threshold_min, int ratio, int apertureSize) // Canny edge detection
{
    RNG rng(123); // controled randomization
    Mat canny_output, gray, blur;
    GaussianBlur(source, blur, Size(3,3), 0, 0); // better with a gaussian blur first
    cvtColor(blur, gray, COLOR_BGR2GRAY ); // convert to gray
    vector<vector<Point> > contours; // create structure to compute
    vector<Vec4i> hierarchy;
    Canny(gray, canny_output, threshold_min, threshold_min*ratio, apertureSize, true ); // apply Canny
    findContours(canny_output, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0)); // find the edges
    Mat drawing = Mat::zeros(canny_output.size(), CV_8UC3);
    for(size_t i = 0; i< contours.size(); i++) { // scan the image
        Scalar color = Scalar(rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255)); // apply colors
        drawContours( drawing, contours, (int)i, color, 2, 8, hierarchy, 0, Point() ); // draw the lines
    }
    return drawing;
}
