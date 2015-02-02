/*
 * nest.h
 *
 *  Created on: 2014-09-30
 *      Author: yixin
 */

#ifndef NEST_H_
#define NEST_H_

#include "cuckoo.h"
#include "lsh.h"

/*
 * 在每个哈希表中左右扩展的数目， 在Nest中为1
 */

typedef struct NNResult {
	int num;
	void **data;
	void **info;
} NNResult;

typedef CuckooHash Nest;

//虽然没有用到Nest，不过这可以保证不会在没有初始化配置时调用这个宏
//#define NestGetDimension(nest) (global_lsh_param->config->dimension)

//#define NestSetFindPosMethod(nest, m) ((nest)->find_opt_pos = (m))
//#define NestSetHashMethods(nest, m) ((nest)->functions = (m))

class NestBuilder{
private:
	//跳过的全零位向量计数
	long skip_counter;
	LshBuilder* lsh_builder_ptr;
	CuckooBuilder* cuckoo_builder_ptr;
	LshParam* global_lsh_param;
	HashTable **hash_tables;
	Nest *nest;

	HashTable **hashTablesCreate(void (*free_data)(void *ptr), void (*free_info)(void *ptr));
	Nest *nestCreate(HashTable **hash_tables);
	void nestDestroy(void);
public:
	NestBuilder(const char* config_file);
	~NestBuilder(void);
	Nest* get_nest(void);
	int nestInsertItem(void *data, void *info);
	int nestRemoveItem(void *data, int (*match)(void *data, void *ptr));
	void nestReport(void);
	NNResult *nestGetNN(void *data);
	void *nestGetItem(void *data, int (*match)(void *data, void *ptr));
	void freeNNResult(NNResult *res);
	int NestGetDimension(void){
		return lsh_builder_ptr->get_global_lsh_param()->config->dimension;
	}
	static void NestSetFindPosMethod(Nest* nest, findOptPos_t m){
		nest->find_opt_pos = m;
	}
	static void NestSetHashMethods(Nest* nest, hashfunc_t* m){
		nest->functions = m;
	}
};

#endif /* NEST_H_ */
