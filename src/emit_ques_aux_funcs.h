/*
 * emit_ques_aux_funcs.h
 *
 *  Created on: 2014-09-25
 *      Author: yixin
 */


#ifndef EMIT_QUES_AUX_FUNCS_H_
#define EMIT_QUES_AUX_FUNCS_H_

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/imgproc_c.h>
#include "params.h"
#include "Nest/nest.h"

using namespace std;
using namespace cv;

/* The learning algorithm of weak learners, ANN */
int alg_ann(NestBuilder*, float**, int, char* [], int, float** &, int);
/* Write the labels into file system */
int record_results(int, char*, float**, int);
/* Write the feature vectors into file system */
int record_feature_vectors(int, char*, float**, int, int);
/* Read in feature vectors from file system */
int read_feature_vectors(int, char*, float** &, int &, int &);

#endif  EMIT_QUES_AUX_FUNCS_H_
