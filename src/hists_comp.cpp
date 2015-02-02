/*
 * hists_comp.cpp
 *
 *  Created on: 2014-09-24
 *      Author: yixin
 */

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/video/tracking.hpp>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <math.h>
#include <errno.h>
#include <boost/shared_array.hpp>
#include <boost/unordered_map.hpp>
#include <stdint.h>
#include "hists_comp.h"

#include "pre_proc_aux_funcs.h"

using namespace std;
using namespace cv;

int hsv_color_dist(Mat, Mat &);
int color_coherence_dist(Mat, Mat &);
void get_disz_color(const Mat &, int, int, rgb &);
uint32_t find_label(uint32_t, boost::unordered_map<uint32_t, label_info> &);
int color2idx(const rgb &);
void ccv(Mat, vector<float> &, vector<float> &);
int spatial_pattern_dist(Mat, Mat &);
int get_pattern(int, int, int, int);
int edge_orient_dist(Mat, Mat &);
int bounding_box_dist(Mat, Mat &);
int texture_dist(Mat, Mat &);
template <typename _Tp>
void OLBP(const Mat&, Mat&);
int optical_flow_dist(Mat, Mat, Mat &);

/* Color distribution of a frame in HSV space */
int hsv_color_dist(Mat img, Mat &hist)
{
	Mat img_hsv;
	int hue_size = HUESIZE;
	int sat_size = SATSIZE;
	int val_size = VALSIZE;
	Mat hue_hist, sat_hist, val_hist;
	vector<float> b;

	cvtColor(img, img_hsv, CV_BGR2HSV);
	vector<Mat> hsv_planes;
	split(img_hsv, hsv_planes);

	float hue_range[] = {0, 180};
	const float* hueHistRange = {hue_range};
	float sat_range[] = {0, 256};
	const float* satHistRange = {sat_range};
	float val_range[] = {0, 256};
	const float* valHistRange = {val_range};
	bool uniform = true; bool accumulate = false;

	/// Calculate hue, saturation and value histograms and normalize
	calcHist(&hsv_planes[0], 1, 0, Mat(), hue_hist, 1, &hue_size, &hueHistRange, uniform, accumulate);
	calcHist(&hsv_planes[1], 1, 0, Mat(), sat_hist, 1, &sat_size, &satHistRange, uniform, accumulate);
	calcHist(&hsv_planes[2], 1, 0, Mat(), val_hist, 1, &val_size, &valHistRange, uniform, accumulate);
	normalize(hue_hist, hue_hist, 1, 0, NORM_L2, -1, Mat());
	normalize(sat_hist, sat_hist, 1, 0, NORM_L2, -1, Mat());
	normalize(val_hist, val_hist, 1, 0, NORM_L2, -1, Mat());

	/// Concatenate histograms into hist
	hist.push_back(hue_hist);
	hist.push_back(sat_hist);
	hist.push_back(val_hist);

	return 0;
}

/* Color coherence of a frame */
int color_coherence_dist(Mat img, Mat &hist)
{
	Mat img_resize;
	vector<float> alpha_vec(NUM_CCV_COLOR);
	vector<float> beta_vec(NUM_CCV_COLOR);

	if (img.channels() != 3) {
		return -1;
	}

	/// Resize and blur img
	resize(img, img_resize, Size(CCVWIDTH, CCVHEIGHT));
	GaussianBlur(img_resize, img_resize, Size(3, 3), 0);

	/// Initialize alpha_vec and beta_vec
	for (int i = 0; i < NUM_CCV_COLOR; i++) {
		alpha_vec[i] = 0.0f;
		beta_vec[i] = 0.0f;
	}

	/// Compute the coherent and incoherent pixel vectors, alpha_vec and beta_vec
	ccv(img_resize, alpha_vec, beta_vec);

	/// Construct hist from alpha_vec and beta_vec
	Mat alpha_hist(alpha_vec, true);
	Mat beta_hist(beta_vec, true);

	/// Normalize hists and concatenate them into a hist
	normalize(alpha_hist, alpha_hist, 1, 0, NORM_L2, -1, Mat());
	normalize(beta_hist, beta_hist, 1, 0, NORM_L2, -1, Mat());
	hist.push_back(alpha_hist);
	hist.push_back(beta_hist);

	return 0;
}

/* Discretize the color of each pixel */
void get_disz_color(const Mat &img, int x, int y, rgb &color)
{
	uint8_t r, b, g;
	int idx;

	/// Prepared for the boundary conditions, set the value of the color of pixels do not exist as 0xffffffff, which is an impossible value for any pixel.
	if (x < 0 || x >= CCVWIDTH || y < 0 || y >= CCVHEIGHT) {
		color.m_color32 = 0xffffffff;
	}
	/// For pixels inside the image, discretize their values of color
	else {
		idx = (x + y * CCVWIDTH) * 3;

		b = img.data[idx];
		g = img.data[idx + 1];
		r = img.data[idx + 2];

		b >>= COLORDIV;
		g >>= COLORDIV;
		r >>= COLORDIV;

		color.m_color8[0] = b;
		color.m_color8[1] = g;
		color.m_color8[2] = r;
		color.m_color8[3] = 0;
	}

	return;
}

/* Find out which component a pixel belongs to */
uint32_t find_label(uint32_t label, boost::unordered_map<uint32_t, label_info> &labels)
{
	boost::unordered_map<uint32_t, label_info>::iterator it;

	/// This label retrieved from labeled representing a component may be incorporated by another component, so
	/// the search should not end until label == m_alias of a component.
	for (it = labels.find(label); label != it->second.m_alias;) {
		label = it->second.m_alias;
		it = labels.find(label);
	}

	return label;
}

/* Calculate the index of a color in alpha_vec or beta_vec */
int color2idx(const rgb &color)
{
	return (int) color.m_color8[0] | (int) color.m_color8[1] << 2
			| (int) color.m_color8[2] << 4;
}

/* Compute the color coherence and incoherence vectors, alpha_vec and beta_vec */
void ccv(Mat img, vector<float> &alpha_vec, vector<float> &beta_vec)
{
	boost::shared_array<uint32_t> labeled;
	boost::unordered_map<uint32_t, label_info> labels;
	boost::unordered_map<uint32_t, label_info>::iterator it;
	labeled = boost::shared_array<uint32_t>(new uint32_t[CCVWIDTH * CCVHEIGHT]);
	label_info info;
	uint32_t label;
	int idx;

	for (int y = 0; y < CCVHEIGHT; y++) {
		for (int x = 0; x < CCVWIDTH; x++) {
			/// Obtain colors of a pixel and pixels from left, above, left above and right above
			rgb c_here, c_left, c_above, c_above_l, c_above_r;
			get_disz_color(img, x, y, c_here);
			get_disz_color(img, x - 1, y, c_left);
			get_disz_color(img, x, y - 1, c_above);
			get_disz_color(img, x - 1, y - 1, c_above_l);
			get_disz_color(img, x + 1, y - 1, c_above_r);

			/// Used for compute the number of components possess the same color
			uint32_t same[4];
			uint32_t l;
			int num_same = 0;

			/// Find components with the same color from different directions
			if (c_here == c_left) {
				l = labeled[x - 1 + y * CCVWIDTH];
				same[num_same] = find_label(l, labels);
				num_same++;
			}

			if (c_here == c_above) {
				l = labeled[x + (y - 1) * CCVWIDTH];
				same[num_same] = find_label(l, labels);
				num_same++;
			}

			if (c_here == c_above_l) {
				l = labeled[x - 1 + (y - 1) * CCVWIDTH];
				same[num_same] = find_label(l, labels);
				num_same++;
			}

			if (c_here == c_above_r) {
				l = labeled[x + 1 + (y - 1) * CCVWIDTH];
				same[num_same] = find_label(l, labels);
				num_same++;
			}

			/// If One or more components are found
			if (num_same > 0) {
				boost::unordered_map<uint32_t, label_info>::iterator it1, it2;
				uint32_t label_min = same[0];

				/// Find the component with the lowest label number
				for (int i = 1; i < num_same; i++) {
					if (same[i] < label_min) {
						label_min = same[i];
					}
				}

				/// Add the pixel into the component with the lowest label number
				it1 = labels.find(label_min);
				it1->second.m_count++;

				/// Incorporate other components into the component with the lowest number
				for (int i = 0; i < num_same; i++) {
					if (label_min < same[i]) {
						it2 = labels.find(same[i]);

						int count = it2->second.m_count;

						it2->second.m_alias = label_min;
						it2->second.m_count = 0;

						it1->second.m_count += count;
					}
				}

				/// Add the pixel into the component with the lowest label number
				labeled[x + y * CCVWIDTH] = label_min;
			}
			/// This pixel has a different color, increase label and update corresponding structures
			else {
				if (x == 0 && y == 0) {
					label = 0;
				} else {
					label++;
				}
				labeled[x + y * CCVWIDTH] = label;

				info.m_color = c_here;
				info.m_alias = label;
				info.m_count = 1;

				labels[label] = info;
			}
		}
	}

	for (it = labels.begin(); it != labels.end(); ++it) {
		idx = color2idx(it->second.m_color);

		if (it->second.m_count > TAU) {
			alpha_vec[idx] += it->second.m_count;
		} else {
			beta_vec[idx] += it->second.m_count;
		}
	}

	return;
}

/* Spatially intensity distribution of a frame */
int spatial_pattern_dist(Mat img, Mat &hist)
{
	Mat img_gray;
	vector<int> patterns;
	int hozBlockSize, vetBlockSize;
	int m11, m12, m21, m22;
	Mat M11, M12, M21, M22;
	int pattern;

	int pat_hist_array[PATTERNCOUNT];
	for (int i = 0; i != PATTERNCOUNT; i++){
		pat_hist_array[i] = 0;
	}

	cvtColor(img, img_gray, CV_BGR2GRAY);
	/// Split a frame into blocks
	hozBlockSize = img_gray.rows / BLOCKCOUNT;
	vetBlockSize = img_gray.cols / BLOCKCOUNT;

	/// Calculate the sum of intensities of pixels in each block of a frame
	M11 = img_gray.colRange(0, vetBlockSize-1).rowRange(0, hozBlockSize-1);
	M12 = img_gray.colRange(vetBlockSize,img_gray.cols-1).rowRange(0, hozBlockSize-1);
	M21 = img_gray.colRange(0, vetBlockSize-1).rowRange(hozBlockSize, img_gray.rows-1);
	M22 = img_gray.colRange(vetBlockSize,img_gray.cols-1).rowRange(hozBlockSize, img_gray.rows-1);
	m11 = sum(M11).val[0];
	m12 = sum(M12).val[0];
	m21 = sum(M21).val[0];
	m22 = sum(M22).val[0];

	/// Get the pattern of the frame
	pattern = get_pattern(m11, m12, m21, m22);
	pat_hist_array[pattern-1]++;

	vector<double> pat_hist_vec(pat_hist_array, pat_hist_array+PATTERNCOUNT);
	hist = Mat(pat_hist_vec, true);

	return 0;
}

/* Map the order of block intensities into a number */
int get_pattern(int v1, int v2, int v3, int v4)
{
	int p1, p2, p3, pattern;
	p1 = 0; p2 = 0; p3 = 0;
	int spaces[] = {6, 2, 1};

	/// Get position of v1 in sorted {v1, v2, v3, v4}
	if (v1 > v2){
		p1++;
	}
	if (v1 > v3){
		p1++;
	}
	if (v1 > v4){
		p1++;
	}

	/// Get position of v2 in sorted {v2, v3, v4}
	if (v2 > v3){
		p2++;
	}
	if (v2 > v4){
		p2++;
	}

	/// Get position of v3 in sorted {v3, v4}
	if (v3 > v4){
		p3++;
	}

	pattern = p1*spaces[0] + p2*spaces[1] + p3*spaces[2] + 1;
	return pattern;
}

/* Edge Orientation distribution of a frame */
int edge_orient_dist(Mat img, Mat &hist)
{
	int scale = 1;
	int delta = 0;
	int ddepth = CV_32F;
	Mat img_gray;
	Mat grad_x, grad_y;
	Mat abs_grad_x, abs_grad_y;

	GaussianBlur( img, img, Size(3,3), 0, 0, BORDER_DEFAULT );
	/// Convert it to gray
	cvtColor( img, img_gray, CV_RGB2GRAY );

	/// Gradients X and Y
	Sobel( img_gray, grad_x, ddepth, 1, 0, 3, scale, delta, BORDER_DEFAULT );
	Sobel( img_gray, grad_y, ddepth, 0, 1, 3, scale, delta, BORDER_DEFAULT );

	Mat orient = Mat(img_gray.rows, img_gray.cols, CV_32F);
	for (int i = 0; i < img_gray.rows; i++){
		for (int j = 0; j < img_gray.cols; j++){
			orient.at<float>(i,j) = (float) atan2(grad_y.at<float>(i,j), grad_x.at<float>(i,j));
		}
	}

	/// Calculate histogram and normalize
	float range[] = {-CV_PI, CV_PI};
	const float* histRange = {range};
	bool uniform = true; bool accumulate = false;
	int hist_size = EDGEHISTSIZE;

	calcHist(&orient, 1, 0, Mat(), hist, 1, &hist_size, &histRange, uniform, accumulate);
	normalize(hist, hist, 1, 0, NORM_L2, -1, Mat());

	return 0;
}

/* Bounding box histogram for a frame */
int bounding_box_dist(Mat img, Mat &hist)
{
	Mat img_gray, width_hist, height_hist;
	Mat width_mat_tmp, height_mat_tmp, width_mat, height_mat;
	Mat threshold_output;
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;

	cvtColor(img, img_gray, CV_BGR2GRAY);
	blur(img_gray, img_gray, Size(3,3));

	/// Detect edges using Threshold
	threshold( img_gray, threshold_output, VARTHRESH, 255, THRESH_BINARY );
	/// Find contours
	findContours( threshold_output, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );

	/// Approximate contours to polygons + get bounding rects and circles
	vector<vector<Point> > contours_poly( contours.size() );
	vector<Rect> boundRect( contours.size() );
	vector<double> widthVec(contours.size());
	vector<double> heightVec(contours.size());

	for( int i = 0; i < contours.size(); i++ ){
		approxPolyDP( Mat(contours[i]), contours_poly[i], 3, true );
	    boundRect[i] = boundingRect( Mat(contours_poly[i]) );
	    widthVec[i] = boundRect[i].size().width;
	    heightVec[i] = boundRect[i].size().height;
	}

	/// Convert width and height matrix for the ease of computing histograms
	width_mat_tmp = Mat(widthVec, true);
	height_mat_tmp = Mat(heightVec, true);
	width_mat_tmp.convertTo(width_mat, CV_8U);
	height_mat_tmp.convertTo(height_mat, CV_8U);

	/// Calculate histograms and normalize
	float range[] = {0, 255};
	const float* histRange = {range};
	bool uniform = true; bool accumulate = false;
	int width_size = WIDTHHISTSIZE;
	int height_size = HEIGHTHISTSIZE;

	calcHist(&width_mat, 1, 0, Mat(), width_hist, 1, &width_size, &histRange, uniform, accumulate);
	calcHist(&height_mat, 1, 0, Mat(), height_hist, 1, &height_size, &histRange, uniform, accumulate);
	normalize(width_hist, width_hist, 1, 0, NORM_L2, -1, Mat());
	normalize(height_hist, height_hist, 1, 0, NORM_L2, -1, Mat());

	/// Concatenate width and height histograms into hist
	hist.push_back(width_hist);
	hist.push_back(height_hist);

	return 0;
}

/* LBP texture distribution of a frame */
int texture_dist(Mat img, Mat &hist)
{
	Mat img_gray, lbp;

	cvtColor(img, img_gray, CV_BGR2GRAY);

	/// Compute LBP texture matrix
	OLBP<unsigned char>(img_gray, lbp);

	/// Calculate histogram and normalize
	float range[] = {0, 255};
	const float* histRange = {range};
	bool uniform = true; bool accumulate = false;
	int hist_size = LBPBINCOUNT;

	calcHist(&lbp, 1, 0, Mat(), hist, 1, &hist_size, &histRange, uniform, accumulate);
	normalize(hist, hist, 1, 0, NORM_L2, -1, Mat());

	return 0;
}

/* Compute the LBP matrix an image */
template <typename _Tp>
void OLBP(const Mat& src, Mat& dst)
{
	dst = Mat::zeros(src.rows-2, src.cols-2, CV_8UC1);
	for(int i=1;i<src.rows-1;i++) {
		for(int j=1;j<src.cols-1;j++) {
			_Tp center = src.at<_Tp>(i,j);
        	unsigned char code = 0;
        	code |= (src.at<_Tp>(i-1,j-1) > center) << 7;
        	code |= (src.at<_Tp>(i-1,j) > center) << 6;
        	code |= (src.at<_Tp>(i-1,j+1) > center) << 5;
        	code |= (src.at<_Tp>(i,j+1) > center) << 4;
        	code |= (src.at<_Tp>(i+1,j+1) > center) << 3;
        	code |= (src.at<_Tp>(i+1,j) > center) << 2;
        	code |= (src.at<_Tp>(i+1,j-1) > center) << 1;
        	code |= (src.at<_Tp>(i,j-1) > center) << 0;
        	dst.at<unsigned char>(i-1,j-1) = code;
		}
	}
}

/* The optical flow vector angle distribution of a frame */
int optical_flow_dist(Mat prev_img, Mat next_img, Mat &hist)
{
	Mat prev_img_gray, next_img_gray, flow;
	double pyr_scale = 0.5;
	int levels = 3;
	int winsize = 15;
	int iterations = 3;
	int poly_n = 5;
	double poly_sigma = 1.2;

	cvtColor(prev_img, prev_img_gray, CV_BGR2GRAY);
	cvtColor(next_img, next_img_gray, CV_BGR2GRAY);

	/// Compute the optical flow from two consecutive frames, and filter out some ones
	calcOpticalFlowFarneback(prev_img_gray, next_img_gray, flow, pyr_scale, levels, winsize, iterations, poly_n, poly_sigma, 0);
	Mat vecs_angle(flow.size(), CV_32F);
	for (int i = 0; i < flow.rows; i++){
		for (int j = 0; j < flow.cols; j++){
			vecs_angle.at<float>(i,j) = (float) atan2(flow.at<Point2f>(i,j).y, flow.at<Point2f>(i,j).x);
		}
	}

	/// Calculate histogram and normalize
	float range[] = {-CV_PI, CV_PI};
	const float* histRange = {range};
	bool uniform = true; bool accumulate = false;
	int hist_size = OPFLOWHISTSIZE;

	calcHist(&vecs_angle, 1, 0, Mat(), hist, 1, &hist_size, &histRange, uniform, accumulate);
	normalize(hist, hist, 1, 0, NORM_L2, -1, Mat());

	return 0;
}
