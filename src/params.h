/*
 * params.h
 *
 *  Created on: 2014-09-24
 *      Author: yixin
*/

#ifndef PARAMS_H_
#define PARAMS_H_

#define CMDLEN 1024
#define PATHLEN 1024
#define BUFLEN 512
#define GTLEN 2
#define VIDNAMELEN 24
#define CLASSNUM 7
#define FEATURENUM 7
#define FNLEN 24
#define NUMLEN 10
#define VID_LOOP_THREAD_NUM 2
#define COM_THREAD_NUM 7
#define LOOP_THREAD_NUM 3
#define HSV_LOOP_THREAD_NUM 4
#define CC_LOOP_THREAD_NUM 20
#define SP_LOOP_THREAD_NUM 1
#define EO_LOOP_THREAD_NUM 4
#define BB_LOOP_THREAD_NUM 1
#define TEX_LOOP_THREAD_NUM 4
#define OPT_LOOP_THREAD_NUM 25

extern char *func_name_array[];
extern char *suffix_array[];
extern char conf_file[];
extern char feature_vector_folder[];
extern char label_vector_folder[];
extern char* classes[];
extern int vector_lens[];
extern float ranges[];
extern int lsh_func_nums[];

#endif  PARAMS_H_
