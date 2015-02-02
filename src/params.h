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
#define MULFACTOR 100
#define NUMLEN 10

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
