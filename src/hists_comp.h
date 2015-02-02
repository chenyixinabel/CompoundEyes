/*
 * hists_comp.h
 *
 *  Created on: 2014-09-24
 *      Author: yixin
*/

#ifndef HISTS_COMP_H_
#define HISTS_COMP_H_

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/imgproc_c.h>

#define HUESIZE 18
#define SATSIZE 3
#define VALSIZE 3
#define EDGEHISTSIZE 16
#define WIDTHHISTSIZE 16
#define HEIGHTHISTSIZE 16
#define OPFLOWHISTSIZE 16

#define BLOCKCOUNT 2
#define PATTERNCOUNT 24
#define LBPBINCOUNT 16

#define VARTHRESH 100

#define CCVWIDTH 240
#define CCVHEIGHT 160
#define COLORDIV 6
#define TAU 25
#define NUM_CCV_COLOR (4 * 4 * 4)

using namespace std;
using namespace cv;

union rgb {
	uint32_t m_color32;
	uint8_t m_color8[4];

	bool operator==(const rgb &rhs) {
		return m_color32 == rhs.m_color32;
	}
};

struct label_info {
	rgb m_color;
	int m_count;
	uint32_t m_alias;
};

/* Color distribution of a frame in HSV space */
int hsv_color_dist(Mat, Mat &);
/* Color coherence of a frame */
int color_coherence_dist(Mat, Mat &);
/* Spatially intensity distribution of a frame */
int spatial_pattern_dist(Mat, Mat &);
/* Edge Orientation distribution of a frame */
int edge_orient_dist(Mat, Mat &);
/* Bounding box histogram for a frame */
int bounding_box_dist(Mat, Mat &);
/* LBP texture distribution of a frame */
int texture_dist(Mat, Mat &);
/* The optical flow vector angle distribution of a frame */
int optical_flow_dist(Mat, Mat, Mat &);

#endif  HISTS_COMP_H_
