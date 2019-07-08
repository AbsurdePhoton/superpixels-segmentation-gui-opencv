/*#-------------------------------------------------
 *
 * OpenCV image tools library
 * Author: AbsurdePhoton
 *
 * v1.9 - 2019/07/08
 *
 * Convert mat images to QPixmap or QImage and vice-versa
 * Brightness, Contrast, Gamma, Equalize, Color Balance
 * Erosion / dilation of pixels
 * Copy part of image
 * Resize image keeping aspect ration
 * Contours using Canny algorithm with auto min and max threshold
 * Noise reduction quality
 * Gray gradients
 * Red-cyan anaglyph tints
 *
#-------------------------------------------------*/

#ifndef MAT2IMAGE_H
#define MAT2IMAGE_H

#include "opencv2/opencv.hpp"
#include <opencv2/ximgproc.hpp>

const double Pi = atan(1)*4;
enum shift_direction{shift_up=1, shift_right, shift_down, shift_left}; // directions for shift function
enum gradientType {gradient_flat, gradient_linear, gradient_doubleLinear, gradient_radial}; // gradient types
enum curveType {curve_linear, curve_cosinus2, curve_sigmoid, curve_cosinus, curve_cos2sqrt,
                curve_power2, curve_cos2power2, curve_power3, curve_undulate, curve_undulate2, curve_undulate3}; // gray curve types
enum anaglyphTint {tint_color, tint_gray, tint_true, tint_half, tint_optimized, tint_dubois}; // red/cyan anaglyph tints

bool IsRGBColorDark(int red, int green, int blue); // is the RGB value given dark or not ?

cv::Mat QImage2Mat(const QImage &source); // convert QImage to Mat
cv::Mat QPixmap2Mat(const QPixmap &source); // convert QPixmap to Mat

QImage Mat2QImage(const cv::Mat &source); // convert Mat to QImage
QPixmap Mat2QPixmap(const cv::Mat &source); // convert Mat to QPixmap
QPixmap Mat2QPixmapResized(const cv::Mat &source, const int &width, const int &height, const bool &smooth); // convert Mat to resized QPixmap
QImage cvMatToQImage(const cv::Mat &source); // another implementation Mat type wise

cv::Mat BrightnessContrast(const cv::Mat &source, const double &alpha, const int &beta); // brightness and contrast
cv::Mat GammaCorrection(const cv::Mat &source, const double gamma); // gamma correction
cv::Mat EqualizeHistogram(const cv::Mat &source); // histogram equalization
cv::Mat SimplestColorBalance(const cv::Mat &source, const float &percent); // color balance with histograms

cv::Mat DilatePixels(const cv::Mat &source, const int &dilation_size); // dilate pixels
cv::Mat ErodePixels(const cv::Mat &source, const int &erosion_size); // erode pixels

cv::Mat ShiftFrame(const cv::Mat &source, const int &nb_pixels, const shift_direction &direction); // shift frame in one direction

cv::Mat CopyFromImage (cv::Mat source, const cv::Rect &frame); // copy part of an image
cv::Mat ResizeImageAspectRatio(const cv::Mat &source, const cv::Size &frame); // Resize image keeping aspect ratio

cv::Mat DrawColoredContours(const cv::Mat &source, const double &sigma, const int &apertureSize, const int &thickness); // draw colored contours of an image

double PSNR(const cv::Mat &source1, const cv::Mat &source2); // noise difference between 2 images

void GradientFillGray(const int &gradient_type, cv::Mat &img, const cv::Mat &msk,
                      const cv::Point &beginPoint, const cv::Point &endPoint,
                      const int &beginColor, const int &endColor,
                      const int &curve, cv::Rect area = cv::Rect(0, 0, 0, 0)); // fill a 1-channel image with the mask converted to gray gradients

cv::Mat AnaglyphTint(const cv::Mat & source, const int &tint); // change tint of image to avoid disturbing colors in red-cyan anaglyph mode

#endif // MAT2IMAGE_H
