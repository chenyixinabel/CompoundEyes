/*
 * pre_proc_aux_funcs.cpp
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
#include <vector>
#include <omp.h>
#include "pre_proc_aux_funcs.h"

using namespace std;
using namespace cv;

char conf_file[] = "/home/yixin/workspace/Hist_Nest_parallel_1/src/Nest/nest.conf";
int vector_lens[] = {HUESIZE+SATSIZE+VALSIZE};
float ranges[] = {0.05};
int lsh_func_nums[] = {4};

int load_dirs(char*, char*, char** &, unsigned* &, int &);
int create_feature_tab(NestBuilder* &, int);
int new_conf_file(char*, int, int, float, int, char*&);
int avg_vecs_comp(char**, int, float*** &);
int load_imgs(char*, vector<Mat> &, int &);
int gen_hists_comp(int (*hist_function)(Mat, Mat&), vector<Mat>, int, vector<Mat> &);
int optical_flow_hists_comp(int (*hist_function)(Mat, Mat, Mat&), vector<Mat>, int, vector<Mat> &);
int avg_hist_comp(vector<Mat>, int, float*&);
int insert_vec(NestBuilder*, float*, unsigned*);

/* Load names of directories into dirs */
int load_dirs(char* vid_frame_folder, char* vid_info_file, char** &dirs, unsigned* &vid_info, int &lines)
{
	dirs = NULL;
	char cmd[CMDLEN];
	lines = 0;
	int folder_num = 0;
	vid_info = NULL;
	FILE* in = NULL;
	char path[PATHLEN];
	char buf[BUFLEN];
	int dirs_idx = 0;
	char ground_truth[GTLEN];
	char name[VIDNAMELEN];
	int id;

	/// Get the number of lines in vid_info_file
	snprintf(cmd, sizeof(cmd), "cat %s | wc -l", vid_info_file);
	if ((in = popen(cmd, "r")) == NULL){
		fprintf(stderr, "Command execution failed!\n");
		return -1;
	}
	fscanf(in, "%d", &lines);
	pclose(in);

	/// Obtain the number of video folders
	snprintf(cmd, sizeof(cmd), "ls -1 %s | wc -l", vid_frame_folder);
	if ((in = popen(cmd, "r")) == NULL){
		fprintf(stderr, "Command execution failed!\n");
		return -1;
	}
	fscanf(in, "%d", &folder_num);
	pclose(in);

	/// Check whether lines == folder_num, if not, there is an error occurs
	if (lines != folder_num){
		fprintf(stderr, "The numbers of video frame folders and groundtruth labels do not match!\n");
		return -1;
	}

	/// Read in Video Information
	vid_info = (unsigned*) malloc(sizeof(unsigned)*lines);
	if (NULL == vid_info){
		fprintf(stderr, "Memory allocate failed.\n");
		return -1;
	}
	if ((in = fopen(vid_info_file, "r")) == NULL){
		fprintf(stderr, "Command execution failed!\n");
		return -1;
	}
	for (int i = 0; i < lines; i++){
		fscanf(in, "%d %s %s\n", &id, &ground_truth, &name);
		for (int j = 0; j < CLASSNUM; j++){
			if (strcmp(ground_truth, classes[j]) == 0){
				vid_info[i] = j;
				continue;
			}
		}
	}
	fclose(in);

	/// Read frame folder names into dirs
	dirs = (char**) malloc(sizeof(char*)*lines);
	if (NULL == dirs){
		fprintf(stderr, "Memory allocate failed!\n");
		return -1;
	}

	snprintf(cmd, sizeof(cmd), "ls -1 %s | sort -n -k 1 -t _ -k 2 -t _", vid_frame_folder);
	if ((in = popen(cmd, "r")) == NULL){
		fprintf(stderr, "Command execution failed!\n");
		return -1;
	}

	memset(&path[0], 0, PATHLEN);
	memset(&buf[0], 0, BUFLEN);
	while (fgets(buf, sizeof(buf), in) != NULL){
		strcpy(path, vid_frame_folder);
		strcat(path, buf);
		path[strlen(path)-1] = '/';

		dirs[dirs_idx] = NULL;
		dirs[dirs_idx] = (char*) malloc(sizeof(char)*(strlen(path)+1));
		if (NULL == dirs[dirs_idx]){
			fprintf(stderr, "Memory allocate failed!\n");
			return -1;
		}
		strcpy(dirs[dirs_idx++], path);

		memset(&path[0], 0, PATHLEN);
		memset(&buf[0], 0, BUFLEN);
	}
	pclose(in);

	return 0;
}

/* Create a NEST hash table */
int create_feature_tab(NestBuilder* &nest_builder_ptr, int func_idx)
{
	nest_builder_ptr = NULL;
	char* conf_file_new_path;

	/// Create a new configuration file of NEST, with the dimension parameter modified
	new_conf_file(conf_file, func_idx, vector_lens[func_idx], ranges[func_idx], lsh_func_nums[func_idx], conf_file_new_path);

	nest_builder_ptr = new NestBuilder(conf_file_new_path);

	free(conf_file_new_path);

	return 0;
}

/* Generate the new NEST conf_file */
int new_conf_file(char* conf_file_path, int func_idx, int dimension, float range, int lsh_func_num, char* &conf_file_path_new)
{
	int base_name_len, new_name_len;
	FILE *fp_old, *fp_new;
	char buf[BUFLEN];
	conf_file_path_new = NULL;
	char* dot_loc, *key_word_loc;
	char dim[NUMLEN];
	char ran[NUMLEN];
	char lsh_f_num[NUMLEN];

	/// Construct the path for the new configuration file
	dot_loc = strchr(conf_file_path, '.');
	base_name_len = (dot_loc-&conf_file_path[0])/sizeof(char);
	new_name_len = base_name_len + 1 + strlen(func_name_array[func_idx]) + 6;

	conf_file_path_new = (char*) malloc(sizeof(char)*new_name_len);
	if (NULL == conf_file_path_new){
		fprintf(stderr, "Memory allocate failed!\n");
		return -1;
	}
	memset(conf_file_path_new, 0, new_name_len);

	strncpy(conf_file_path_new, conf_file_path, base_name_len);
	strcat(conf_file_path_new, "_");
	strcat(conf_file_path_new, func_name_array[func_idx]);
	strcat(conf_file_path_new, ".conf");

	/// Open files
	if (NULL == (fp_old=fopen(conf_file_path, "r"))){
		fprintf(stderr, "Cannot open the file: %s\n", conf_file_path);
		return -1;
	}
	if (NULL == (fp_new=fopen(conf_file_path_new, "w"))){
		fprintf(stderr, "Cannot open the file: %s\n", conf_file_path_new);
		return -1;
	}

	/// Modify the dimension of the old file and write the new value into the new file
	while (NULL != fgets(buf, BUFLEN, fp_old)){
		// Ignore comments
		if (buf[0] == '#' || buf[0] == '\0'){
			fputs(buf, fp_new);
			continue;
		}
		// Modify the value of r
		if (NULL != (key_word_loc = strstr(buf, "r="))){
			sprintf(ran, "%.4f", range);
			*(key_word_loc+2) = '\0';
			strcat(key_word_loc, ran);
			strcat(key_word_loc, "\n");
			fputs(buf, fp_new);
			key_word_loc = NULL;
		}
		// Modify the value of k
		else if (NULL != (key_word_loc = strstr(buf, "k="))){
			sprintf(lsh_f_num, "%d", lsh_func_num);
			*(key_word_loc+2) = '\0';
			strcat(key_word_loc, lsh_f_num);
			strcat(key_word_loc, "\n");
			fputs(buf, fp_new);
			key_word_loc = NULL;
		}
		// Modify the value of dimension
		else if (NULL != (key_word_loc = strstr(buf, "dimension="))){
			sprintf(dim, "%d", dimension);
			*(key_word_loc+10) = '\0';
			strcat(key_word_loc, dim);
			strcat(key_word_loc, "\n");
			fputs(buf, fp_new);
			key_word_loc = NULL;
		}
		// Ignore other configurations
		else{
			fputs(buf, fp_new);
			continue;
		}
	}

	fclose(fp_old);
	fclose(fp_new);
	return 0;
}

/* Compute the average vectors of videos, using all the feature detection algorithms */
int avg_vecs_comp(char** vid_ptr, int vid_count, float*** &avg_vecs)
{
	vector<Mat> imgs;
	int frame_count;

	omp_set_nested(1);

	/// Allocate Memory for avg_vecs
	avg_vecs = NULL;
	avg_vecs = (float***) malloc(sizeof(float**)*FEATURENUM);
	if (NULL == avg_vecs){
		fprintf(stderr, "Memory allocate failed!\n");
		return -1;
	}
	for (int i = 0; i < FEATURENUM; i++){
		avg_vecs[i] = NULL;
		avg_vecs[i] = (float**) malloc(sizeof(float*)*vid_count);
		if (NULL == avg_vecs[i]){
			fprintf(stderr, "Memory allocate failed!\n");
			return -1;
		}
	}


	for (int i = 0; i < vid_count; i++){
		imgs.clear();
		frame_count = 0;

		// Load frames of a video into imgs
		load_imgs(vid_ptr[i], imgs, frame_count);

		// Compute vectors, could work in parallel
/*#pragma omp parallel num_threads(COM_THREAD_NUM)
		{
#pragma omp sections
			{
#pragma omp section
				{*/
		vector<Mat> hsv_hists(frame_count);
		gen_hists_comp(&hsv_color_dist, imgs, frame_count, hsv_hists);
		avg_hist_comp(hsv_hists, frame_count, avg_vecs[0][i]);
				/*}
#pragma omp section
				{
					vector<Mat> cc_hists(frame_count);
					gen_hists_comp(&color_coherence_dist, imgs, frame_count, cc_hists);
					avg_hist_comp(cc_hists, frame_count, avg_vecs[1][i]);
				}
#pragma omp section
				{
					vector<Mat> sp_hists(frame_count);
					gen_hists_comp(&spatial_pattern_dist, imgs, frame_count, sp_hists);
					avg_hist_comp(sp_hists, frame_count, avg_vecs[2][i]);
				}
#pragma omp section
				{
					vector<Mat> eo_hists(frame_count);
					gen_hists_comp(&edge_orient_dist, imgs, frame_count, eo_hists);
					avg_hist_comp(eo_hists, frame_count, avg_vecs[3][i]);
				}
#pragma omp section
				{
					vector<Mat> bb_hists(frame_count);
					gen_hists_comp(&bounding_box_dist, imgs, frame_count, bb_hists);
					avg_hist_comp(bb_hists, frame_count, avg_vecs[4][i]);
				}
#pragma omp section
				{
					vector<Mat> tex_hists(frame_count);
					gen_hists_comp(&texture_dist, imgs, frame_count, tex_hists);
					avg_hist_comp(tex_hists, frame_count, avg_vecs[5][i]);
				}
#pragma omp section
				{
					vector<Mat> of_hists(frame_count-1);
					optical_flow_hists_comp(&optical_flow_dist, imgs, frame_count, of_hists);
					avg_hist_comp(of_hists, frame_count-1, avg_vecs[6][i]);
				}
			}
		}*/
	}

	return 0;
}

/* Load all the frames of a video into imgs */
int load_imgs(char* folder, vector<Mat> &imgs, int &frame_count)
{
	frame_count = 0;
	DIR* dirp;
	struct dirent* entry;
	char frame_name[FNLEN];
	char path[PATHLEN];

	memset(&frame_name[0], 0, FNLEN);
	memset(&path[0], 0, PATHLEN);

	/// Count the number of frames of a folder
	if ((dirp = opendir(folder)) == NULL){
		printf("opendir failure for %s --> %s\n", folder, strerror(errno));
		exit(-1);
	}

	while ((entry = readdir(dirp)) != NULL){
		if (entry->d_type == DT_REG){
			frame_count++;
		}
	}

	for (int i = 1; i <= frame_count; i++){
		/// Construct the name of a frame
		snprintf(frame_name, sizeof(frame_name), "I-Frames%d.jpeg", i);
		strcpy(path, folder);
		strcat(path, frame_name);

		imgs.push_back(imread(path, 1));
		if(!imgs[i-1].data){
			fprintf(stderr, "Read in a frame failed!\n");
			return -1;
		}

		memset(&frame_name[0], 0, FNLEN);
		memset(&path[0], 0, PATHLEN);
	}

	closedir(dirp);
	return 0;
}

/* Compute histograms from a video, using algorithm need only one frame each time */
int gen_hists_comp(int (*hist_function)(Mat, Mat&), vector<Mat> imgs, int frame_count, vector<Mat> &hists)
{
	int loop_thread_num = -1;
	if (hist_function == &hsv_color_dist){
		loop_thread_num = HSV_LOOP_THREAD_NUM;
	}
	else if (hist_function == &color_coherence_dist){
		loop_thread_num = CC_LOOP_THREAD_NUM;
	}
	else if (hist_function == &spatial_pattern_dist){
		loop_thread_num = SP_LOOP_THREAD_NUM;
	}
	else if (hist_function == &edge_orient_dist){
		loop_thread_num = EO_LOOP_THREAD_NUM;
	}
	else if (hist_function == &bounding_box_dist){
		loop_thread_num = BB_LOOP_THREAD_NUM;
	}
	else if (hist_function == &texture_dist){
		loop_thread_num = TEX_LOOP_THREAD_NUM;
	}

#pragma omp parallel num_threads(loop_thread_num)
	{
#pragma omp for schedule(dynamic)
		for (int i = 0; i < frame_count; i++){
			(*hist_function)(imgs[i], hists[i]);
		}
	}

	return 0;
}

/* Computer histograms from a video, using the optical flow algorithm */
int optical_flow_hists_comp(int (*hist_function)(Mat, Mat, Mat&), vector<Mat> imgs, int frame_count, vector<Mat> &hists)
{
#pragma omp parallel num_threads(OPT_LOOP_THREAD_NUM)
	{
#pragma omp for schedule(dynamic)
		for (int i = 0; i < frame_count-1; i++){
			(*hist_function)(imgs[i],imgs[i+1],hists[i]);
		}
	}
	return 0;
}

/* Compute the average histogram of a video and convert it into a float array */
int avg_hist_comp(vector<Mat> hists, int hist_num, float* &avg_arr)
{
	Mat avg_hist;
	avg_arr = NULL;
	vector<float> avg_vec;

	/// Calculate the average histogram
	for (int i = 0; i < hist_num; i++){
		if (i == 0){
			hists[i].copyTo(avg_hist);
		}
		else{
			avg_hist += hists[i];
		}
	}
	avg_hist /= hist_num;

	/// Convert the histogram of the Mat type to avg_vector of the float array type
	avg_hist.copyTo(avg_vec);
	avg_arr = (float*) malloc(sizeof(float)*avg_hist.rows);
	if (NULL == avg_arr){
		fprintf(stderr, "Memory allocate failed.\n");
		return -1;
	}
	for (int i = 0; i < avg_hist.rows; i++){
		//avg_vector[i] = avg_hist.at<float>(i,1)*MULFACTOR;
		avg_arr[i] = avg_vec[i];
	}

	return 0;
}

/* Insert a feature vector into a NEST table */
int insert_vec(NestBuilder* nest_builder_ptr, float* hist_array_ptr, unsigned* vid_info)
{
	///Insert the feature vector into the NEST table
	if (hist_array_ptr != NULL){
		if (-1 == nest_builder_ptr->nestInsertItem(hist_array_ptr, vid_info)){
			fprintf(stderr, "Insert item failed.\n");
			return -1;
		}
	}

	return 0;
}
