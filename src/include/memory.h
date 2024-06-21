#ifndef MEMORY_H
#define MEMORY_H

#include <glib.h>
#include <semaphore.h>
#include <stddef.h>

#include "../../nginx-malloc/ngx_palloc.h"
#include "types.h"

#ifdef USE_POOL_ALLOC
// 任务队列中的任务数量
extern size_t global_task_count;
extern sem_t *global_task_count_semaphore;

// hash表, 存放每个分配了内存的指针信息
extern GHashTable *memory_hash_table;
// 动态数组, 存放 pool 的指针
extern GHashTable *pools;
// 保护的 mutex
extern pthread_mutex_t *memory_info_mutex;

// k 个任务使用一个 pool
#define BATCH_SIZE 8
#endif

void *malloc_impl(size_t size);
void *calloc_impl(size_t nmemb, size_t size);
void free_impl(void *ptr);

#endif