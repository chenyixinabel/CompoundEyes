/*
 * cuckoo.cpp
 *
 *  Created on: 2014-09-29
 *      Author: yixin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>
#include "cuckoo.h"
#include "lsh.h"

CuckooBuilder::CuckooBuilder(LshBuilder* lsh_builder_ptr)
{
#ifdef _EVALUATION_
	insert_items_time = 0.0;
	insert_item_time = 0.0;
	insert_file_time = 0.0;
#endif
	this->lsh_builder_ptr = lsh_builder_ptr;
	global_lsh_param = this->lsh_builder_ptr->get_global_lsh_param();
}

CuckooBuilder::~CuckooBuilder(void)
{
	global_lsh_param = NULL;
}

/*
 * 创建哈希表，如果失败返回NULL，成功返回指向哈希表的指针
 * 传入参数为哈希表大小
 */
HashTable* CuckooBuilder::tableCreate(unsigned long size)
{
	HashTable *table;
	Item *items;

	if((table = (HashTable*)malloc(sizeof(HashTable))) == NULL) {
		fprintf(stderr, "memory allocates failed, %s:%d", __FILE__, __LINE__);
		return NULL;
	}

	if((items = (Item*)malloc(sizeof(Item)*size)) == NULL) {
		free(table);
		fprintf(stderr, "memory allocates failed, %s:%d", __FILE__, __LINE__);
		return NULL;
	}

	table->size = size;
	table->free_data = NULL;
	table->free_info = NULL;
	table->used = 0;
	table->load = 0.0;

	memset(items, 0, sizeof(Item)*size);
	table->items = items;
	return table;
}

/*
 * 释放哈希表，如果定义了free_data和free_info函数，调用它们来释放占用的内存；
 * 如果没有定义，则需要用户手动释放
 */
void CuckooBuilder::tableRelease(HashTable *table)
{
	int i = 0;
	int size = table->size;
	Item *items = table->items;
	if(table == NULL)
		return ;

	for(i = 0; i < size; i++) {
		if(items[i].data != NULL && table->free_data != NULL)
			table->free_data(items[i].data);
		if(items[i].info != NULL && table->free_info != NULL)
			table->free_info(items[i].info);
	}

	free(items);
	free(table);
}

/*
 * 向哈希表中插入元素
 * 成功则返回哈希表，失败返回NULL
 */
HashTable* CuckooBuilder::tableInsert(HashTable *table, unsigned long hv, void *data, void *info)
{
	Item *item;
	unsigned long size = table->size;
	hv = hv % size;
	item = table->items + hv;
	if(item->flag == USED) {
		return NULL;
	} else {
		item->data = data;
		item->info = info;
		item->flag = USED;
		table->used++;
	}
	return table;
}

/*
 * 移除哈希表中的元素
 */
void CuckooBuilder::tableRemove(HashTable *table, unsigned long hv)
{
	Item *item;
	unsigned long size = table->size;
	hv = hv % size;
	item = table->items + hv;
	if(item->flag == FREE) {
		return;
	} else {
		if(table->free_data && item->data != NULL) {
			table->free_data(item->data);
		}
		if(table->free_info && item->info != NULL) {
			table->free_info(item->info);
		}
		item->flag = FREE;
		table->used--;
	}
}

/*
 * 获取哈希表中指定桶中的数据
 */
void* CuckooBuilder::tableGetItemData(HashTable *table, unsigned long hv)
{
	Item *item;
	unsigned long size = table->size;
	hv = hv % size;
	item = table->items + hv;
	if(item->flag == FREE) {
		return NULL;
	} else {
		return item->data;
	}
}

/*
 * 获取哈希表中指定桶中的附加信息
 */
void* CuckooBuilder::tableGetItemInfo(HashTable *table, unsigned long hv)
{
	Item *item;
	unsigned long size = table->size;
	hv = hv % size;
	item = table->items + hv;
	if(item->flag == FREE) {
		return NULL;
	} else {
		return item->info;
	}
}

/*
 * 具体执行cuckoo hashing的kick out操作
 * 将新的data与info插入，将原data与info返回
 */
void CuckooBuilder::tableKickOut(HashTable *table, unsigned long hv ,void *data, void *info, void **old_data, void **old_info)
{
	Item *item;
	hv = hv % table->size;
	item = table->items + hv;
	assert(item->flag == USED);
	*old_data = item->data;
	*old_info = item->info;
	item->data = data;
	item->info = info;
	item->access_count++;
}

/*
 * 计算元素插入时的可选位置
 * 可选位置的数目由函数直接返回，可选位置的具体信息由HashValue数组返回
 * 如果返回值为负数,则表示所有的位置都被占用, 并且负数的绝对值可kick out的位置数目
 */
int CuckooBuilder::cuckooFindPos(CuckooHash *cuckoo_hash, HashValue *hash_value, void *data)
{
	int num = 0, end = cuckoo_hash->opt_pos - 1;
	unsigned long hv;
	HashTable *hash_table;
	Item *item;
	int func_index, table_index;
	int i = 0;

	for(i = 0; i < cuckoo_hash->func_num; i++) {
		func_index = i;
		if(cuckoo_hash->table_num == 1) {
			hash_table = cuckoo_hash->hash_tables[0];
			table_index = 0;
		} else {
			hash_table = cuckoo_hash->hash_tables[i];
			table_index = i;
		}
		hv = (this->lsh_builder_ptr->*(cuckoo_hash->functions[i]))(data, NULL);
		hv = hv % hash_table->size;
		item = hash_table->items + hv;
		if(item->flag == USED) {
			//使用过的桶的哈希值也记录下来，后面可以直接使用
			hash_value[end].hv = hv;
			hash_value[end].func_index = func_index;
			hash_value[end].table_index = table_index;
			end--;
		} else {
			hash_value[num].hv = hv;
			hash_value[num].func_index = func_index;
			hash_value[num].table_index = table_index;
			num++;
		}
	}
	if(num == 0) return -(cuckoo_hash->func_num);
	return num;
}

/*
 * 在nest中查找可以插入元素的位置，这个函数将被cuckooInsertItem函数调用以确定可用的位置
 * @nest: 指向Nest的指针
 * @hash_value: 保存可用位置的数组
 * @data: 插入的数据，将传递给哈希函数计算哈希值
 * 成功返回正数，表示可用位置的数目; 失败返回负数，其绝对值为可kick out位置的数目
 * 可用位置的具体信息存储在hash_value数组中
 */
int CuckooBuilder::nestFindOptPos(CuckooHash *nest, HashValue *hash_value, void *data)
{
	int num = 0, end = nest->func_num - 1;
	int start = 0, used_num = 0;
	unsigned long hv;
	HashTable *hash_table;
	Item *item;
	int func_index, table_index;
	int i = 0, j = 0;
	int offset;
	long new_hv;
	int nest_offset = global_lsh_param->config->offset;

	for(i = 0; i < nest->func_num; i++) {
		hash_table = nest->hash_tables[i];
		table_index = i;
		func_index = i;
		hv = (this->lsh_builder_ptr->*(nest->functions[i]))(data, (void*)table_index);
		hv = hv % hash_table->size;
		item = hash_table->items + hv;
		if(item->flag == USED) {
			//使用过的桶的哈希值也记录下来，后面可以直接使用
			hash_value[end].hv = hv;
			hash_value[end].func_index = func_index;
			hash_value[end].table_index = table_index;
			hash_value[end].access_count = item->access_count;
			end--;
			used_num++;
		} else {
			hash_value[num].hv = hv;
			hash_value[num].func_index = func_index;
			hash_value[num].table_index = table_index;
			hash_value[num].access_count = item->access_count;
			num++;
		}
	}

	//如果num为0，表示在哈希的位置上没有空位可用的，这时检查左右扩展的位置是否可用
	start = nest->func_num;
	end = nest->opt_pos - 1;

	if(num == 0) {
		for(i = 0; i < nest->func_num; i++) {
			hv = hash_value[i].hv;
			table_index = hash_value[i].table_index;
			func_index = hash_value[i].func_index;
			hash_table = nest->hash_tables[table_index];
			//向左右扩展
			for(j = 0; j <= nest_offset * 2; j++) {
				offset = nest_offset - j;
				if(offset == 0) continue;
				//左右扩展后的hv值
				//假定hv不会在signed long中溢出
				new_hv = (signed long)hv + offset;
				if(new_hv < 0 || new_hv >= hash_table->size) continue;

				item = hash_table->items + new_hv;

				if(item->flag == USED) {
					hash_value[start].hv = new_hv;
					hash_value[start].func_index = func_index;
					hash_value[start].table_index = table_index;
					hash_value[start].access_count = item->access_count;
					start++;
					used_num++;
				} else {
					hash_value[end].hv = new_hv;
					hash_value[end].func_index = func_index;
					hash_value[end].table_index = table_index;
					hash_value[end].access_count = item->access_count;
					end--;
					num++;
				}
			}
		}

		//仍然没有找到空位
		if(num == 0) return -used_num;

		//找到空位, 将空位转移到hash_value的前面
		if(num > 0) {
			start = nest->opt_pos - num;
			for(i = 0; i < num; i++) {
				hash_value[i].hv = hash_value[start].hv;
				hash_value[i].func_index = hash_value[start].func_index;
				hash_value[i].table_index = hash_value[start].table_index;
				hash_value[i].access_count = item->access_count;
				start++;
			}
		}
	}

	return num;
}

/*
 * 用于cuckoo hashing的初始化工作
 * 输入包括哈希表，以及哈希函数
 * 哈希表用于存储数据，哈希表用于计算存储位置
 * 其中哈希表的数目只有两种情况，一种情况数目为1,另一种情况数据为n
 * 如果只有一个哈希表，则对哈希函数没有要求，所有的kick out操作将在一个哈希表上进行
 * 如果有n个哈希表，则哈希函数的数目也必须为n，所有的kick out操作将在多个哈希表间进行
 * max_steps表示最大的kick out次数， opt_pos表示可选位置的数目，opt_pos一般就等于func_num
 * 失败时返回NULL，成功时返回指向CuckooHash的指针
 */
CuckooHash* CuckooBuilder::cuckooInit(HashTable **hash_tables, int table_num, hashfunc_t *funcs, int func_num, int max_steps, int opt_pos)
{
	CuckooHash *cuckoo_hash;

	if(table_num !=1 && table_num != func_num) {
		return NULL;
	}

	if((cuckoo_hash = (CuckooHash*)malloc(sizeof(CuckooHash))) == NULL) {
		return NULL;
	}

	cuckoo_hash->hash_tables = hash_tables;
	cuckoo_hash->functions = funcs;
	cuckoo_hash->table_num = table_num;
	cuckoo_hash->func_num = func_num;
	cuckoo_hash->max_steps = max_steps;
	cuckoo_hash->hv_arr = (HashValue*)malloc(sizeof(HashValue)*opt_pos);
	if(NULL == cuckoo_hash->hv_arr) {
		free(cuckoo_hash);
		return NULL;
	}
	memset(cuckoo_hash->hv_arr, 0, sizeof(HashValue)*opt_pos);
	cuckoo_hash->opt_pos = opt_pos;
	cuckoo_hash->kickout_counter = 0;

	return cuckoo_hash;
}

/*
 * 释放cuckoo hashing内存
 */
void CuckooBuilder::cuckooDestroy(CuckooHash *cuckoo_hash)
{
	assert(cuckoo_hash != NULL);
	int table_num = cuckoo_hash->table_num;
	HashTable **tables = cuckoo_hash->hash_tables;
	hashfunc_t *functions = cuckoo_hash->functions;
	HashValue *hv_arr = cuckoo_hash->hv_arr;
	int i = 0;

	for(i = 0; i < table_num; i++) {
		if(NULL != tables[i])
			tableRelease(tables[i]);
	}

	if(NULL != functions) free(functions);
	if(NULL != hv_arr) free(hv_arr);

	free(cuckoo_hash);
}

int CuckooBuilder::ch_comp(const void *a, const void *b)
{
	HashValue hv1 = *(HashValue*)a;
	HashValue hv2 = *(HashValue*)b;
	return hv1.access_count - hv2.access_count;
}

/*
 * 向cuckoo hashing中插入数据
 * 成功则返回插入后的哈希表， 否则返回NULL
 */
CuckooHash* CuckooBuilder::cuckooInsertItem(CuckooHash *cuckoo_hash, void *data, void *info)
{
	int func_num = cuckoo_hash->func_num;
	int max_steps = cuckoo_hash->max_steps;
	int mid = max_steps / 2;
	int free_pos, pos, table_index;
	void *old_data, *old_info;
	int opt_pos;

	/* 标示是否当前kick out的上次插入操作是否也有kick out */
	int pre_kickout_flag = 0;
	/* 记录下kick out时的选择 */
	HashValue pre_hash_value;

	HashValue *hv_arr;

	int optimize_flag = global_lsh_param->config->optimize_kickout;

	hv_arr = cuckoo_hash->hv_arr;

#ifdef  _EVALUATION_
	struct timeval start, end;
	gettimeofday(&start, NULL);
#endif

	while(max_steps >= 0) {
		free_pos = (this->*(cuckoo_hash->find_opt_pos))(cuckoo_hash, hv_arr, data);
		//有空位可用
		if(free_pos > 0) {
			pre_kickout_flag = 0;

			if(optimize_flag == OPTIMIZE_ON && max_steps < mid) {
				qsort(hv_arr, free_pos, sizeof(HashValue), CuckooBuilder::ch_comp);
				pos = 0;
			} else {
				//随机选位置
				pos = rand() % free_pos;
			}
			table_index = hv_arr[pos].table_index;
			if(tableInsert(cuckoo_hash->hash_tables[table_index], hv_arr[pos].hv, data, info) == NULL) {
				fprintf(stderr, "Error: insert used position, have to exit!\n");
				exit(-1);
			}
			//成功则跳出循环
			break;
		} else if(max_steps != 0 && free_pos != 0) { // 当max_steps为0时，禁止kick out

			opt_pos = -free_pos;

			if(optimize_flag == OPTIMIZE_ON && max_steps < mid) {
				qsort(hv_arr, opt_pos, sizeof(HashValue), ch_comp);
				pos = 0;
			} else {
				//无空位，需要kick out
				//同样，随机选择kick out的位置
				pos = rand() % opt_pos;
			}

			table_index = hv_arr[pos].table_index;

			//判断是否选择的位置是上次刚被kick out的位置
			//如果是，將位置加1,以避开该位置
			if(pre_kickout_flag == 1) {
				if(table_index == pre_hash_value.table_index && \
						hv_arr[pos].hv == pre_hash_value.hv) {
					pos++;
					pos = pos % opt_pos;
					table_index = hv_arr[pos].table_index;
				}
			}

			tableKickOut(cuckoo_hash->hash_tables[table_index], hv_arr[pos].hv, data, info, &old_data, &old_info);
			data = old_data;
			info = old_info;

			pre_kickout_flag = 1;
			pre_hash_value.hv = hv_arr[pos].hv;
			pre_hash_value.func_index = hv_arr[pos].func_index;
			pre_hash_value.table_index = hv_arr[pos].table_index;
		}

		max_steps--;
	}

	//把kick out次数记录下来
	cuckoo_hash->kickout_counter += (cuckoo_hash->max_steps - max_steps);

#ifdef _EVALUATION_
	gettimeofday(&end, NULL);
	insert_item_time = 1000000*(end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
	insert_items_time += insert_item_time;
#endif

	//超过kick out上限或者禁止了kick out
	if(max_steps < 0) {
		return NULL;
	}
	return cuckoo_hash;
}

/*
 * 将一个文件中的数据插入到哈希表中
 * 文件要求每条数据占用一行
 */
CuckooHash* CuckooBuilder::cuckooInsertFile(CuckooHash *cuckoo_hash, const char * filename)
{
	FILE *fd;
	char buf[BUFSIZE];
	char *data;
	int len;

	if((fd = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "can't open file %s\n", filename);
		return NULL;
	}
#ifdef _EVALUATION_
	struct timeval start, end;
	gettimeofday(&start, NULL);
	insert_items_time = 0.0;
#endif
	while(fgets(buf, BUFSIZE, fd)) {
		len = strlen(buf);
		data = (char*)malloc(len);
		if(data == NULL) {
			fprintf(stderr, "Allocate memory failed: exit!\n");
			exit(-1);
		}
		memcpy(data, buf, len);
		data[len-1] = '\0';
		if(cuckooInsertItem(cuckoo_hash, data, NULL) == NULL) {
			break;
		}
	}

#ifdef _EVALUATION_
	gettimeofday(&end, NULL);
	insert_file_time = 1000000*(end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
#endif

	fclose(fd);
	return cuckoo_hash;
}

/*
 * 输出整个cuckoo hashing的统计信息
 */
void CuckooBuilder::cuckooReport(CuckooHash *cuckoo_hash)
{
	HashTable *hash_table;
	int num = cuckoo_hash->table_num;
	unsigned long  size_sum = 0, used_sum = 0;
	int i = 0;
	float load;
	printf("\t size \t used \t load\n");
	for(i = 0; i < num; i++) {
		hash_table = cuckoo_hash->hash_tables[i];
		load = (float)hash_table->used / (float)hash_table->size;
		size_sum += hash_table->size;
		used_sum += hash_table->used;
		printf("table%d\t%ld\t%ld\t%f\n", i, hash_table->size, hash_table->used, load);
	}

	printf("SUM\t%ld\t%ld\t%f\n", size_sum, used_sum, (float)used_sum / (float)size_sum);
	printf("Kick out Times : %lld\n", cuckoo_hash->kickout_counter);
#ifdef _EVALUATION_
	printf("Insert Items Time: %.2lfus\n", insert_items_time);
	printf("Insert Item Time(last one) : %.2lfns\n", insert_item_time * 1000);
#endif
}

void CuckooBuilder::TableSetFreeDataMethod(HashTable* table, void (*m)(void*)){
		table->free_data = m;
	}

void CuckooBuilder::TableSetFreeInfoMethod(HashTable* table, void (*m)(void*)){
		table->free_info = m;
	}

void CuckooBuilder::CuckooSetFindPosMethod(CuckooHash* cuckoo, findOptPos_t m){
		cuckoo->find_opt_pos = m;
	}
