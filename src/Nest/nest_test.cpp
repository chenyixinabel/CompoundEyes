#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "nest.h"

float computeDistance(float *point1, float *point2, int dimension);
int nestInsertFile(NestBuilder* nest_builder_ptr, const char *filename);

/*int main(int argc, char *argv[])
{
	char *config_file;
	char *trace_file;
	Nest* nest;
	NNResult *res;

	//用于测试的查询输入
	int dimension = 7;
	float point[] = {84.0188, 39.4383, 78.3099, 79.8440, 91.1647, 67.1983, 85.7923};

	float *p;
	int i, j;

	if(argc != 2) {
		printf("Usage: %s trace_file\n", argv[0]); 
		return -1;
	}
	config_file = "/home/yixin/workspace/Nest_new/src/nest.conf";
	trace_file = argv[1];
	
	NestBuilder* nest_builder_ptr = new NestBuilder(config_file);
	//nest = nest_builder_ptr->get_nest();
	
	//这个函数需要根据要传入的trace的文件进行实现
	nestInsertFile(nest_builder_ptr, trace_file);

	nest_builder_ptr->nestReport();

	res = nest_builder_ptr->nestGetNN(point);

	printf("\n Input vector:\n");
	for(i = 0; i < dimension; i++) {
		printf("%.4f  ", point[i]);
	}
	
	printf("\n ANN result:\n");
	for(i = 0; i < res->num; i++) {

		p = ((float**)res->data)[i];	
		printf("\nDistance : %.4f\n", computeDistance(point, p, dimension));

		for(j = 0; j < dimension; j++) {
			printf("%.4f  ", p[j]);
		}
		printf("\n");
	}
	nest_builder_ptr->freeNNResult(res);
	
	return 0;	
}*/

float computeDistance(float *point1, float *point2, int dimension)
{
	float res = 0.0;	
	int i = 0;
	for(i = 0; i < dimension; i++) {
		res += (point1[i] - point2[i]) * (point1[i] - point2[i]);
	}
	return sqrt(res);
}

int nestInsertFile(NestBuilder* nest_builder_ptr, const char *filename)
{
        FILE *fd;
        int counter = 0;
        int dimension;
        float *point = NULL, f;
        int i = 0;

        //dimension = global_lsh_param->config->dimension;
        dimension = nest_builder_ptr->NestGetDimension();

        if((fd = fopen(filename, "r")) == NULL) {
                fprintf(stderr, "can't open file %s\n", filename);
                return -1;
        }

	//开始读取文件中数据
        while(fscanf(fd, "%f", &f) != EOF) {
                if(NULL == point) {
                        point = (float*)malloc(sizeof(float)*dimension);
                        if(NULL == point) {
                                fprintf(stderr, "nestInsertFile: allocate memory failed.\n");
                                return counter;
                        }
                }
                point[i++] = f;
                if(i % dimension == 0) {
                        i = 0;
                        assert(point != NULL);
			//将读取的数据point插入nest索引结构中
                        if(-1 == nest_builder_ptr->nestInsertItem(point, NULL))
                                break;
                        counter++;
                        point = NULL;
                }
        }

        fclose(fd);
        return counter;
}
