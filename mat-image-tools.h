/*
 * OpenCV image tools library
 * Author: AbsurdePhoton
 *
 * v1.5 - 2018/09/02
 *
 * Convert mat images to QPixmap or QImage
 * Set brightness and contrast
 * Equalize color image histograms
 * Erosion / dilation of pixels
 * Copy part of image
 * Contours using Canny algorithm with auto min and max threshold
 * Noise reduction quality
 *
 */

#ifndef MAT2IMAGE_H
#define MAT2IMAGE_H

#include "opencv2/opencv.hpp"
#include <opencv2/ximgproc.hpp>

using namespace cv;

bool IsColorDark(int red, int green, int blue); // is the RGB value given dark or not ?

QImage Mat2QImage(cv::Mat const& src); // convert Mat to QImage
QPixmap Mat2QPixmap(cv::Mat source); // convert Mat to QPixmap
QPixmap Mat2QPixmapResized(cv::Mat source, int max_width, int max_height, bool smooth); // convert Mat to resized QPixmap
QImage cvMatToQImage(const cv::Mat &inMat); // another implementation Mat type wise

Mat BrightnessContrast(Mat source, double alpha, int beta); // set brightness and contrast
Mat EqualizeHistogram(cv::Mat image); // histogram equalization
Mat SimplestColorBalance(Mat& source, float percent); // color balance with histograms

Mat DilatePixels(cv::Mat image, int dilation_size); // dilate pixels
Mat ErodePixels(cv::Mat image, int erosion_size); // erode pixels

enum Direction{ShiftUp=1, ShiftRight, ShiftDown, ShiftLeft}; // for use in ShiftFrame
Mat ShiftFrame(cv::Mat frame, int pixels, Direction direction); // shift frame in one direction

Mat CopyFromImage (cv::Mat source, cv::Rect frame); // copy part of an image

Mat DrawColoredContours(cv::Mat source, double sigma, int apertureSize, int thickness); // draw colored contours of an image

double PSNR(const Mat& I1, const Mat& I2); // noise difference between 2 images

#endif // MAT2IMAGE_H
