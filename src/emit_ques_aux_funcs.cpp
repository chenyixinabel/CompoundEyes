/*
 * emit_ques_aux_funcs.cpp
 *
 *  Created on: 2014-09-25
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
#include <libgen.h>
#include <omp.h>
#include "emit_ques_aux_funcs.h"

using namespace std;
using namespace cv;

char *func_name_array[] = {"hsv_color_dist", "color_coherence_dist", "spatial_pattern_dist", "edge_orient_dist"\
		, "bounding_box_dist"};
char *suffix_array[] = {"_vectors.txt", "_labels.txt"};
char feature_vector_folder[] = "/home/yixin/workspace/Hist_Nest_parallel_5/Vector_files/";
char label_vector_folder[] = "/home/yixin/workspace/Hist_Nest_parallel_5/Label_files/";
char* classes[CLASSNUM] = {"E", "S", "V", "M", "L", "X", "-1"};

int alg_ann(NestBuilder*, float**, int, char* [], int, float** &, int);
int record_results(int, char*, float**, int);
int create_names(int, char*, int, char* &);
int record_feature_vectors(int, char*, float**, int, int);
int read_feature_vectors(int, char*, float** &, int &, int &);

/* The learning algorithm of weak learners, ANN */
int alg_ann(NestBuilder* nest_builder_ptr, float** hist_array_ptr, int lines, char* classes[], int classes_num, float** &result, int dimension)
{
	omp_set_nested(1);

	/// Initialize the return value of ANN
	result = NULL;
	result = (float**) malloc(sizeof(float*)*lines);
	if (NULL == result){
		fprintf(stderr, "Memory allocate failed.\n");
		return -1;
	}

	for (int i = 0; i < lines; i++){
		result[i] = NULL;
		result[i] = (float*) malloc(sizeof(float)*classes_num);
		if (NULL == result[i]){
			fprintf(stderr, "Memory allocate failed.\n");
			return -1;
		}

		for (int j = 0; j < classes_num; j++){
			result[i][j] = 0;
		}
	}

	/// Compute the possibilities of a query belonging to each label
#pragma omp parallel num_threads(LOOP_THREAD_NUM)
	{
#pragma omp for schedule(dynamic)
		for (int i = 0; i < lines; i++){
			if (NULL == hist_array_ptr[i]){
				continue;
			}

			NNResult* res = nest_builder_ptr->nestGetNN(hist_array_ptr[i]);
			int zero_counter = 0;
			for (int j = 0; j < classes_num; j++){
				for (int k = 0; k < res->num; k++){
					if (*((unsigned**) res->info)[k] == j){
						result[i][j] += 1;
					}
					else{
						zero_counter++;
					}
				}
				if (res->num != 0){
					result[i][j] /= res->num;
				}
			}
			/// Set the query as X
			if (zero_counter == res->num*classes_num){
				result[i][5] = 1.0;
			}
			nest_builder_ptr->freeNNResult(res);
		}
	}

	return 0;
}

/* Write the labels into file system */
int record_results(int func_name_idx, char* query_vid_frame_folder, float** query_result, int query_num)
{
	/// Create the name of labels file
	char* labels_file_name;
	create_names(func_name_idx, query_vid_frame_folder, 1, labels_file_name);

	if (access(labels_file_name, F_OK) == -1){
		/// Check if the parental directory exists, if not, create a new one
		struct stat info;
		if (stat(label_vector_folder, &info) != 0){
			mkdir(label_vector_folder, S_IRWXU);
		}
		/// Create the file of labels
		FILE* fd;
		if ((fd = fopen(labels_file_name, "w")) == NULL){
			fprintf(stderr, "Can't create the file %s \n", labels_file_name);
			return -1;
		}

		/// Record the labels
		fprintf(fd, "%d ", query_num);
		for (int i = 0; i < query_num; i++){
			fprintf(fd, "%f %f %f %f %f %f %f ", i, query_result[i][0], query_result[i][1], query_result[i][2]\
					, query_result[i][3], query_result[i][4], query_result[i][5], query_result[i][6]);
		}
		fclose(fd);
	}
	free(labels_file_name);

	return 0;
}

/* Create the names of files to store computed feature vectors and labels.
 The index of function names:
 * 0: hsv_color_dist
 * 1: color_moments
 * 2: spatial_pattern_dist
 * 3: edge_orient_dist
 * 4: bounding_box_dist
 * 5: texture_dist
 * 6: optical_flow_dist
 */
int create_names(int func_name_idx, char* vid_frame_folder, int option, char* &hist_file_name)
{
	int suffix_idx = 0;
	char* func_name = NULL;
	char* suffix = NULL;
	char* directory = NULL;
	char* vid_frame_folder_copy;
	int hist_file_name_len;

	/// Allocate memory for vid_frame_folder_copy
	vid_frame_folder_copy = NULL;
	vid_frame_folder_copy = (char*) malloc(sizeof(char)*(strlen(vid_frame_folder)+1));
	if (NULL == vid_frame_folder_copy){
		fprintf(stderr, "Memory allocate failed.\n");
		return -1;
	}
	strcpy(vid_frame_folder_copy, vid_frame_folder);

	/// Create general file name
	func_name = (char*) malloc(sizeof(char)*(strlen(func_name_array[func_name_idx])+1));
	if (NULL == func_name){
		fprintf(stderr, "Memory allocate failed.\n");
		return -1;
	}
	strcpy(func_name, func_name_array[func_name_idx]);

	/// Create names of suffix and directory
	switch (option){
	case 0:
		directory = feature_vector_folder;
		suffix_idx = 0;
		break;
	case 1:
		directory = label_vector_folder;
		suffix_idx = 1;
		break;
	}
	suffix = (char*) malloc(sizeof(char)*(strlen(suffix_array[suffix_idx])+1));
	if (NULL == suffix){
		fprintf(stderr, "Memory allocate failed.\n");
		return -1;
	}
	strcpy(suffix, suffix_array[suffix_idx]);

	/// Concatenate all the parts of the name together
	hist_file_name_len = strlen(directory) + strlen(basename(vid_frame_folder_copy)) + 1\
			+ strlen(func_name) + strlen(suffix);
	hist_file_name = NULL;
	hist_file_name = (char*) malloc(sizeof(char)*(hist_file_name_len+1));
	if (NULL == hist_file_name){
		fprintf(stderr, "Memory allocate failed!\n");
		return -1;
	}

	strcpy(hist_file_name, directory);
	strcat(hist_file_name, basename(vid_frame_folder_copy));
	strcat(hist_file_name, "_");
	strcat(hist_file_name, func_name);
	strcat(hist_file_name, suffix);

	/// Release the dynamically allocated memory
	free(func_name);
	free(suffix);
	free(vid_frame_folder_copy);

	return 0;
}

/* Write the feature vectors into file system */
int record_feature_vectors(int func_name_idx, char* vid_frame_folder, float** ind_avg_vecs, int num, int dimension)
{
	char* vectors_file_name;
	struct stat info;
	FILE* fd;

	/// Create the name of vectors file
	create_names(func_name_idx, vid_frame_folder, 0, vectors_file_name);

	if (access(vectors_file_name, F_OK) == -1){
		/// Check if the parental directory exists, if not, create a new one
		if (stat(feature_vector_folder, &info) != 0){
			mkdir(feature_vector_folder, S_IRWXU);
		}
		/// Create the file of vectors
		if ((fd = fopen(vectors_file_name, "w")) == NULL){
			fprintf(stderr, "Can't create the file %s \n", vectors_file_name);
			return -1;
		}

		/// Record the labels
		fprintf(fd, "%d ", num);
		fprintf(fd, "%d ", dimension);
		for (int i = 0; i < num; i++){
			for (int j = 0; j < dimension; j++){
				fprintf(fd, " %f", ind_avg_vecs[i][j]);
			}
		}
		fclose(fd);
	}
	free(vectors_file_name);

	return 0;
}

/* Read in feature vectors from file system */
int read_feature_vectors(int func_name_idx, char* vid_frame_folder, float** &ind_avg_vecs, int &num, int &dimension)
{
	ind_avg_vecs = NULL;
	num = 0;
	dimension = 0;
	char* vectors_file_name;
	FILE* fd;

	/// Create the name of vectors file
	create_names(func_name_idx, vid_frame_folder, 0, vectors_file_name);

	if (access(vectors_file_name, F_OK) != -1){
		/// Open the file of vectors
		if ((fd = fopen(vectors_file_name, "r")) == NULL){
			fprintf(stderr, "Can't open the file %s \n", vectors_file_name);
			return -1;
		}

		/// Read in the number and dimension of records
		fscanf(fd, "%d %d ", &num, & dimension);

		/// Allocate memory to store the vectors
		ind_avg_vecs = (float**) malloc(sizeof(float*)*num);
		if (NULL == ind_avg_vecs){
			fprintf(stderr, "Memory Allocation failed!\n");
			return -1;
		}
		for (int i = 0; i < num; i++){
			ind_avg_vecs[i] = NULL;
			ind_avg_vecs[i] = (float*) malloc(sizeof(float)*dimension);
			if (NULL == ind_avg_vecs[i]){
				fprintf(stderr, "Memory Allocation failed!\n");
				return -1;
			}
		}

		/// Read in feature vectors
		for (int i = 0; i < num; i++){
			for (int j = 0; j < dimension; j++){
				fscanf(fd, " %f", &ind_avg_vecs[i][j]);
			}
		}
		fclose(fd);
	}
	free(vectors_file_name);

	return 0;
}
