/*
 * pre_proc_aux_funcs.h
 *
 *  Created on: 2014-09-25
 *      Author: yixin
*/

#ifndef PRE_PROC_AUX_FUNCS_H_
#define PRE_PROC_AUX_FUNCS_H_

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/imgproc_c.h>
#include "params.h"
#include "hists_comp.h"
#include "Nest/nest.h"

using namespace std;
using namespace cv;

/* Load names of directories into dirs */
//int load_dirs(char*, char*, char** &, Vid_info* &, int &);
int load_dirs(char*, char*, char** &, unsigned* &, int &);
/* Create a NEST hash table */
int create_feature_tab(NestBuilder* &, int);
/* Compute the average vectors of videos, using all the feature detection algorithms */
int avg_vecs_comp(char**, int, float*** &);
/* Insert a feature vector into a NEST table */
//int insert_vec(NestBuilder*, float*, Vid_info);
int insert_vec(NestBuilder*, float*, unsigned*);
/* Print the feature vectors on the screen */
int report_avg_vecs(float***, int);

int load_imgs(char*, vector<Mat> &, int &);
int gen_hists_comp(int (*hist_function)(Mat, Mat&), vector<Mat>, int, vector<Mat> &);
int optical_flow_hists_comp(int (*hist_function)(Mat, Mat, Mat&), vector<Mat>, int, vector<Mat> &);
int avg_hist_comp(vector<Mat> hists, int hist_num, float* &avg_arr);

#endif  PRE_PROC_AUX_FUNCS_H_
