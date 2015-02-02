/*
 * nest.cpp
 *
 *  Created on: 2014-09-30
 *      Author: yixin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include "lsh.h"
#include "cuckoo.h"
#include "nest.h"

NestBuilder::NestBuilder(const char* config_file)
{
	skip_counter = 0;
	lsh_builder_ptr = new LshBuilder(config_file);
	cuckoo_builder_ptr = new CuckooBuilder(lsh_builder_ptr);
	global_lsh_param = lsh_builder_ptr->get_global_lsh_param();

	hash_tables = hashTablesCreate(free, NULL);
	if(hash_tables == NULL) {
		fprintf(stderr, "create hash tables failed.\n");
		return;
	}

	nest = nestCreate(hash_tables);
	if(nest == NULL) {
		fprintf(stderr, "create nest failed.\n");
		return;
	}
}

NestBuilder::~NestBuilder()
{
	global_lsh_param = NULL;
	delete cuckoo_builder_ptr;
	delete lsh_builder_ptr;
	nestDestroy();
}

Nest* NestBuilder::get_nest(void)
{
	return nest;
}

/*
 * 初始化Nest的相关参数
 * @config_file: 配置文件nest.conf
 * 成功则返回正数，失败返回负数
 */
/*int NestBuilder::initNestParam(const char *config_file)
{
	LshParam *param;
	param = initLshParam(config_file);
	if(param == NULL)
		return -1;

	return 1;
}*/

/*
 * 创建哈希表
 * @free_data: 用于哈希表中释放数据内存的函数，如果不指定，需要用户手动释放内存
 * @free_info: 用于哈希表中释放附加信息内存的函数，如果不指定，需要手动释放
 * 成功返回初始化的哈希表，失败返回NULL
 */
HashTable** NestBuilder::hashTablesCreate(void (*free_data)(void *ptr), void (*free_info)(void *ptr))
{
	HashTable **hash_tables;
	int i = 0, index;
	int num = global_lsh_param->config->l;
	int size = global_lsh_param->config->size;

	hash_tables = (HashTable**)malloc(sizeof(HashTable*)*num);
	if(NULL == hash_tables) {
		goto error;
	}

	for( i = 0; i < num; i++) {
		index++;
		hash_tables[i] = CuckooBuilder::tableCreate(size);
		if(NULL == hash_tables[i]) {
			goto error;
		}
		CuckooBuilder::TableSetFreeDataMethod(hash_tables[i], free_data);
		CuckooBuilder::TableSetFreeInfoMethod(hash_tables[i], free_info);
	}

	goto success;

	error:
	for(i = 0; i < index; i++) {
		CuckooBuilder::tableRelease(hash_tables[i]);
	}
	fprintf(stderr, "create hash tables failed.\n");
	return NULL;

	success:
	return hash_tables;
}

/*
 * 创建nest
 * @hash_tables: 哈希表
 * 成功返回Nest，失败返回NULL
 */
Nest* NestBuilder::nestCreate(HashTable **hash_tables)
{
	Nest *nest;
	hashfunc_t *hashfunc_list;
	int i = 0;
	int opt_pos;
	int table_num = global_lsh_param->config->l;
	int max_steps = global_lsh_param->config->max_steps;
	int func_num = table_num;
	int offset = global_lsh_param->config->offset;


	hashfunc_list = (hashfunc_t*)malloc(sizeof(hashfunc_t) * func_num);
	if(NULL == hashfunc_list)  goto error;
	for(i = 0; i < func_num; i++) {
		hashfunc_list[i] = &LshBuilder::computeLsh;
	}

	opt_pos = func_num * (2 * offset + 1);
	nest = CuckooBuilder::cuckooInit(hash_tables, table_num, hashfunc_list, func_num, max_steps, opt_pos);
	if(NULL == nest) goto error;
	CuckooBuilder::CuckooSetFindPosMethod(nest, &CuckooBuilder::nestFindOptPos);

	goto success;

	error:
	fprintf(stderr, "create nest failed.\n");
	return NULL;

	success:
	return nest;
}

/*
 * 向nest中插入元素
 * @nest: 指向nest的指针
 * @data: 插入的数据
 * @info: 插入的辅助信息
 * 这里的实现直接调用了cuckoo hashing的插入函数
 * 成功返回0，失败返回-1
 */
int NestBuilder::nestInsertItem(void *data, void *info)
{
	if (NULL == cuckoo_builder_ptr->cuckooInsertItem(nest, data, info)){
		return -1;
	}
	else{
		return 0;
	}
}


/*
 * 打印哈希表中的状态
 */
void NestBuilder::nestReport(void)
{
	printf("total %ld data skiped.\n", skip_counter);
	cuckoo_builder_ptr->cuckooReport(nest);
}

/*
 * 获得与data距离相近的元素
 * @nest: 指向Nest的指针
 * @data: 输入
 * 返回值为存储结果的结构体, 失败则返回NULL
 */
NNResult* NestBuilder::nestGetNN(void *data)
{
	NNResult *result;
	void **data_list, **info_list;
	HashTable **tables = nest->hash_tables;
	HashTable *table;
	Item *items, *item;
	int table_num = nest->table_num;
	int func_num = nest->func_num;
	int table_size;
	int i = 0, j = 0,  num = 0;
	int table_index;
	int offset;
	unsigned long hv;
	long new_hv;
	int nest_offset = global_lsh_param->config->offset;
	int opt_pos = func_num * (2 * nest_offset + 1);

	result = (NNResult*)malloc(sizeof(NNResult));
	if(NULL == result) {
		return NULL;
	}
	data_list = (void**)malloc(sizeof(void*)*opt_pos);
	if(NULL == data_list) {
		free(result);
		return NULL;
	}
	info_list = (void**)malloc(sizeof(void*)*opt_pos);
	if(NULL == info_list) {
		free(result);
		free(data_list);
		return NULL;
	}
	/*
	for(i = 0; i < table_num; i++) {
		table = tables[i];
		items = table->items;
		for(j = 0; j < table->size; j++) {
			if(items[j].data != NULL) {
				if(checkSuffix((char*)items[j].info) < 0)
				{
					printf("NestGetNN: framename = %s\n", (char*)items[j].info);
				}
			}
		}
	}
	 */
	for(i = 0; i < table_num; i++) {
		table = tables[i];
		table_index = i;
		table_size = table->size;
		items = table->items;
		hv = (this->lsh_builder_ptr->*(nest->functions[i]))(data, (void*)table_index);
		hv = hv % table_size;

		for(j = 0; j <= nest_offset*2; j++) {
			offset = nest_offset - j;
			new_hv = (signed long)hv + offset;
			if(new_hv < 0 || new_hv >= table_size) continue;
			item = items + new_hv;
			if(item->flag == USED) {
				data_list[num] = item->data;
				info_list[num] = item->info;
				//	printf("info = %s\n", (char*)item->info);
				num++;
			}
		}
	}
	result->num = num;
	result->data = data_list;
	result->info = info_list;

	return result;
}

void NestBuilder::freeNNResult(NNResult *res)
{
	if(res != NULL) {
		if(res->data != NULL) free(res->data);
		if(res->info != NULL) free(res->info);
		free(res);
	}
}

/*
 * 释放Nest的内存空间
 */
void NestBuilder::nestDestroy(void)
{
	assert(nest != NULL);
	CuckooBuilder::cuckooDestroy(nest);
}

/*
 * 释放Nest参数的内存空间
 */
/*void NestBuilder::freeNestParam()
{
	freeLshParam(global_lsh_param);
}*/

/*
 * 删除Nest中的元素
 * match函数用于进行匹配，由用户提供
 * 成功返回0，失败返回-1
 */
int NestBuilder::nestRemoveItem(void *data, int (*match)(void *data, void *ptr))
{
	HashTable **tables = nest->hash_tables;
	HashTable *table;
	Item *items, *item;
	int table_num = nest->table_num;
	int func_num = nest->func_num;
	int table_size;
	int nest_offset = global_lsh_param->config->offset;
	int opt_pos = func_num * (2 * nest_offset + 1);
	int i = 0, j = 0,  num = 0;
	int table_index;
	int offset;
	unsigned long hv;
	long new_hv;

	for(i = 0; i < table_num; i++) {
		table = tables[i];
		table_index = i;
		table_size = table->size;
		items = table->items;
		hv = (this->lsh_builder_ptr->*(nest->functions[i]))(data, (void*)table_index);
		hv = hv % table_size;

		for(j = 0; j <= nest_offset*2; j++) {
			offset = nest_offset - j;
			new_hv = (signed long)hv + offset;
			if(new_hv < 0 || new_hv >= table_size) continue;
			item = items + new_hv;
			if(item->flag == USED) {
				if(NULL == match) return NULL;
				if(match(data, item->data) > 0)
				{
					CuckooBuilder::tableRemove(table, new_hv);
				}
				return 0;
			}
		}
	}

	return -1;
}

/*
 * 从Nest中获取单个元素，成功返回元素中的数据，失败返回NULL
 */
void* NestBuilder::nestGetItem(void *data, int (*match)(void *data, void *ptr))
{
	HashTable **tables = nest->hash_tables;
	HashTable *table;
	Item *items, *item;
	int table_num = nest->table_num;
	int func_num = nest->func_num;
	int table_size;
	int nest_offset = global_lsh_param->config->offset;
	int opt_pos = func_num * (2 * nest_offset + 1);
	int i = 0, j = 0,  num = 0;
	int table_index;
	int offset;
	unsigned long hv;
	long new_hv;

	for(i = 0; i < table_num; i++) {
		table = tables[i];
		table_index = i;
		table_size = table->size;
		items = table->items;
		hv = (this->lsh_builder_ptr->*(nest->functions[i]))(data, (void*)table_index);
		hv = hv % table_size;

		for(j = 0; j <= nest_offset*2; j++) {
			offset = nest_offset - j;
			new_hv = (signed long)hv + offset;
			if(new_hv < 0 || new_hv >= table_size) continue;
			item = items + new_hv;
			if(item->flag == USED) {
				if(NULL == match) return NULL;
				if(match(data, item->data) > 0)
				{
					return item->data;
				}
			}
		}
	}
	return NULL;
}
