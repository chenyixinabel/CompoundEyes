/*
 * main_flow.cpp
 *
 *  Created on: 2014-09-23
 *      Author: yixin
 */

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/imgproc_c.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libgen.h>
#include <omp.h>
#include "params.h"
#include "pre_proc_aux_funcs.h"
#include "emit_ques_aux_funcs.h"
#include "Nest/nest.h"

int com_thread_num;
int hsv_loop_thread_num;
int cc_loop_thread_num;
int sp_loop_thread_num;
int eo_loop_thread_num;
int bb_loop_thread_num;
int tex_loop_thread_num;
int opt_loop_thread_num;
int loop_thread_num;

using namespace std;
using namespace cv;

int proc_flow(char*, char*, char*, char*);
int pre_proc(char*, char*, char*, char*, float*** &, int &, NestBuilder** &);
int emit_queries(char*, NestBuilder**, float***, int);

/* The process flow of the whole prototype */
int proc_flow(char* train_vid_frame_folder, char* train_vid_info_file, char* query_vid_frame_folder\
		, char* query_vid_info_file)
{
	float*** query_avg_vecs;
	int query_num;
	NestBuilder** nest_builders_ptr;

	/// The process flow
	pre_proc(train_vid_frame_folder, train_vid_info_file, query_vid_frame_folder, query_vid_info_file\
			, query_avg_vecs, query_num, nest_builders_ptr);
	emit_queries(query_vid_frame_folder, nest_builders_ptr, query_avg_vecs, query_num);

	/// Destroy all the Nest builders
	for (int i = 0; i < FEATURENUM; i++){
		delete nest_builders_ptr[i];
	}
	free(nest_builders_ptr);

	return 0;
}

/* Pre-processing, which includes the tasks of computing features vectors of all videos and inserting those of
 * the videos in the training set.*/
int pre_proc(char* train_vid_frame_folder, char* train_vid_info_file, char* query_vid_frame_folder\
		, char* query_vid_info_file, float*** &query_avg_vecs, int &query_num, NestBuilder** &nest_builders_ptr)
{
	nest_builders_ptr = NULL;
	char** train_vid_ptr;
	char** query_vid_ptr;
	unsigned* train_vid_info_ptr;
	unsigned* query_vid_info_ptr;
	int train_num;
	float*** train_avg_vecs;
	int train_dim, query_dim;
	struct timeval start, end;
	double comp_train_set, comp_query_set, insertion;

	omp_set_nested(1);

	/// Load directories of videos in the training video set into train_vid_ptr
	load_dirs(train_vid_frame_folder, train_vid_info_file, train_vid_ptr, train_vid_info_ptr, train_num);

	/// Create NEST tables for all the features in the training video set
	nest_builders_ptr = (NestBuilder**) malloc(sizeof(NestBuilder*) * FEATURENUM);
	if (NULL == nest_builders_ptr){
			fprintf(stderr, "Allocate Memory failed!\n");
			return -1;
		}

	for(int i = 0; i < FEATURENUM; i++){
		create_feature_tab(nest_builders_ptr[i], i);
	}

	/// Compute all the vectors of videos in the training video set into train_avg_vecs
	gettimeofday(&start, NULL);
	avg_vecs_comp(train_vid_ptr, train_num, train_avg_vecs);
	gettimeofday(&end, NULL);
	comp_train_set = 1000000*(end.tv_sec-start.tv_sec)+(end.tv_usec-start.tv_usec);

/*	/// Read in the feature vectors of the training set
	train_avg_vecs = NULL;
	train_avg_vecs = (float***) malloc(sizeof(float**)*FEATURENUM);
	if (NULL == train_avg_vecs){
		fprintf(stderr, "Memory Allocation failed!\n");
		return -1;
	}
	for (int i = 0; i < FEATURENUM; i++){
		read_feature_vectors(i, train_vid_frame_folder, train_avg_vecs[i], train_num, train_dim);
	}*/

	/// Insert train_avg_vecs and corresponding video information into NEST
	gettimeofday(&start, NULL);
	for (int i = 0; i < FEATURENUM; i++){
		for (int j = 0; j < train_num; j++){
			insert_vec(nest_builders_ptr[i], train_avg_vecs[i][j], &train_vid_info_ptr[j]);
		}
	}
/*#pragma omp parallel num_threads(com_thread_num)
	{
#pragma omp sections
		{
#pragma omp section
			{
#pragma omp parallel num_threads(loop_thread_num)
				{
#pragma omp for schedule(dynamic)
					for (int j = 0; j < train_num; j++){
						insert_vec(nest_builders_ptr[0], train_avg_vecs[0][j], &train_vid_info_ptr[j]);
					}
				}
			}
#pragma omp section
			{
#pragma omp parallel num_threads(loop_thread_num)
				{
#pragma omp for schedule(dynamic)
					for (int j = 0; j < train_num; j++){
						insert_vec(nest_builders_ptr[1], train_avg_vecs[1][j], &train_vid_info_ptr[j]);
					}
				}
			}
#pragma omp section
			{
#pragma omp parallel num_threads(loop_thread_num)
				{
#pragma omp for schedule(dynamic)
					for (int j = 0; j < train_num; j++){
						insert_vec(nest_builders_ptr[2], train_avg_vecs[2][j], &train_vid_info_ptr[j]);
					}
				}
			}
#pragma omp section
			{
#pragma omp parallel num_threads(loop_thread_num)
				{
#pragma omp for schedule(dynamic)
					for (int j = 0; j < train_num; j++){
						insert_vec(nest_builders_ptr[3], train_avg_vecs[3][j], &train_vid_info_ptr[j]);
					}
				}
			}
#pragma omp section
			{
#pragma omp parallel num_threads(loop_thread_num)
				{
#pragma omp for schedule(dynamic)
					for (int j = 0; j < train_num; j++){
						insert_vec(nest_builders_ptr[4], train_avg_vecs[4][j], &train_vid_info_ptr[j]);
					}
				}
			}
#pragma omp section
			{
#pragma omp parallel num_threads(loop_thread_num)
				{
#pragma omp for schedule(dynamic)
					for (int j = 0; j < train_num; j++){
						insert_vec(nest_builders_ptr[5], train_avg_vecs[5][j], &train_vid_info_ptr[j]);
					}
				}
			}
#pragma omp section
			{
#pragma omp parallel num_threads(loop_thread_num)
				{
#pragma omp for schedule(dynamic)
					for (int j = 0; j < train_num; j++){
						insert_vec(nest_builders_ptr[6], train_avg_vecs[6][j], &train_vid_info_ptr[j]);
					}
				}
			}
		}
	}*/
	gettimeofday(&end, NULL);
	insertion = 1000000*(end.tv_sec-start.tv_sec)+(end.tv_usec-start.tv_usec);

/*	/// Record the feature vectors of the training set
	for (int i = 0; i < FEATURENUM; i++){
		record_feature_vectors(i, train_vid_frame_folder, train_avg_vecs[i], train_num, vector_lens[i]);
	}*/

	/// Load directories of videos in the query video set into query_vid_ptr
	load_dirs(query_vid_frame_folder, query_vid_info_file, query_vid_ptr, query_vid_info_ptr, query_num);

	/// Compute all the vectors of videos in the query video set into query_avg_vecs
	gettimeofday(&start, NULL);
	avg_vecs_comp(query_vid_ptr, query_num, query_avg_vecs);
	gettimeofday(&end, NULL);
	comp_query_set = 1000000*(end.tv_sec-start.tv_sec)+(end.tv_usec-start.tv_usec);

/*	/// Record the feature vectors of the testing set
	for (int i = 0; i < FEATURENUM; i++){
		record_feature_vectors(i, query_vid_frame_folder, query_avg_vecs[i], query_num, vector_lens[i]);
	}*/

/*	/// Read in the feature vectors of the testing set
	query_avg_vecs = NULL;
	query_avg_vecs = (float***) malloc(sizeof(float**)*FEATURENUM);
	if (NULL == query_avg_vecs){
		fprintf(stderr, "Memory Allocation failed!\n");
		return -1;
	}
	for (int i = 0; i < FEATURENUM; i++){
		read_feature_vectors(i, query_vid_frame_folder, query_avg_vecs[i], query_num, query_dim);
	}*/

	/// Release allocated Memory
	for (int i = 0; i < train_num; i++){
		free(train_vid_ptr[i]);
	}
	free(train_vid_ptr);
	/// The value of the pointer train_vid_info_ptr has been passed to a nest builder, the content should not be freed
	/// but the pointer should be set to empty.
	train_vid_info_ptr = NULL;

	for (int i = 0; i < query_num; i++){
		free(query_vid_ptr[i]);
	}
	free (query_vid_ptr);
	free(query_vid_info_ptr);

	/// The value of the pointer train_avg_vecs has been passed to a nest builder, the content should not be freed
	/// but the pointer should be set to empty.
	for (int i = 0; i < FEATURENUM; i++){
		for (int j = 0; j < train_num; j++){
			train_avg_vecs[i][j] = NULL;
		}
		train_avg_vecs[i] = NULL;
	}
	train_avg_vecs = NULL;

	printf("The time cost of computing feature vectors of the training set is %lf, of computing those of the query set is: %lf, of insertion into NEST is %lf\n"\
				, comp_train_set/1000000, comp_query_set/1000000, insertion/1000000);

	return 0;
}

/* Emit queries to NEST tables, and record results to file system */
int emit_queries(char* query_vid_frame_folder, NestBuilder** nest_builders_ptr, float*** query_avg_vecs, int query_num)
{
	omp_set_nested(1);

	float*** query_results_all = NULL;
	query_results_all = (float***) malloc(sizeof(float**)*FEATURENUM);
	if(NULL == query_results_all){
		fprintf(stderr, "Memory allocate failed!\n");
		return -1;
	}

	/// Apply the ANN algorithm of each weak learner to compute the labels for queries
	struct timeval start, end;
	double resp_time;

	gettimeofday(&start, NULL);
/*	for (int i = 0; i < FEATURENUM; i++){
		(*alg_ann)(nest_builders_ptr[i], query_avg_vecs[i], query_num, classes, CLASSNUM, query_results_all[i], vector_lens[i]);
	}*/
#pragma omp parallel num_threads(com_thread_num)
	{
#pragma omp sections
		{
#pragma omp section
			{
				(*alg_ann)(nest_builders_ptr[0], query_avg_vecs[0], query_num, classes, CLASSNUM, query_results_all[0], vector_lens[0]);
			}
#pragma omp section
			{
				(*alg_ann)(nest_builders_ptr[1], query_avg_vecs[1], query_num, classes, CLASSNUM, query_results_all[1], vector_lens[1]);
			}
#pragma omp section
			{
				(*alg_ann)(nest_builders_ptr[2], query_avg_vecs[2], query_num, classes, CLASSNUM, query_results_all[2], vector_lens[2]);
			}
#pragma omp section
			{
				(*alg_ann)(nest_builders_ptr[3], query_avg_vecs[3], query_num, classes, CLASSNUM, query_results_all[3], vector_lens[3]);
			}
#pragma omp section
			{
				(*alg_ann)(nest_builders_ptr[4], query_avg_vecs[4], query_num, classes, CLASSNUM, query_results_all[4], vector_lens[4]);
			}
#pragma omp section
			{
				(*alg_ann)(nest_builders_ptr[5], query_avg_vecs[5], query_num, classes, CLASSNUM, query_results_all[5], vector_lens[5]);
			}
#pragma omp section
			{
				(*alg_ann)(nest_builders_ptr[6], query_avg_vecs[6], query_num, classes, CLASSNUM, query_results_all[6], vector_lens[6]);
			}
		}
	}
	gettimeofday(&end, NULL);
	resp_time = 1000000*(end.tv_sec-start.tv_sec)+(end.tv_usec-start.tv_usec);

	/// Record the labels into file system
	for (int i = 0; i < FEATURENUM; i++){
		record_results(i, query_vid_frame_folder, query_results_all[i], query_num);
	}

	/// Release allocated memory
	for (int i = 0; i < FEATURENUM; i++){
		for (int j = 0; j < query_num; j++){
			free(query_avg_vecs[i][j]);
			free(query_results_all[i][j]);
		}
		free(query_avg_vecs[i]);
		free(query_results_all[i]);
	}
	free(query_avg_vecs);
	free(query_results_all);

	printf("The response time is %lf\n", resp_time/1000000);

	return 0;
}

int main(int argc, char* argv[])
{
	com_thread_num = atoi(argv[5]);
	hsv_loop_thread_num = atoi(argv[6]);
	cc_loop_thread_num = atoi(argv[7]);
	sp_loop_thread_num = atoi(argv[8]);
	eo_loop_thread_num = atoi(argv[9]);
	bb_loop_thread_num = atoi(argv[10]);
	tex_loop_thread_num = atoi(argv[11]);
	opt_loop_thread_num = atoi(argv[12]);
	loop_thread_num = atoi(argv[13]);

	proc_flow(argv[1], argv[2], argv[3], argv[4]);
	return 0;
}
