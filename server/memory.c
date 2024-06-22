#include <glib.h>
#include <glibconfig.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../nginx-malloc/ngx_palloc.h"
#include "include/memory.h"

void *malloc_impl(size_t size) {
#if !defined(USE_POOL_ALLOC)
  return malloc(size);
#endif
#ifdef USE_POOL_ALLOC

  // 如果使用 pool alloc
  pthread_mutex_lock(memory_info_mutex);
  sem_wait(global_task_count_semaphore);
  const size_t current_task_count = global_task_count;
  sem_post(global_task_count_semaphore);
  // 获取 index
  size_t pool_index = current_task_count / BATCH_SIZE;
  ngx_pool_t *pool_ref =
      g_hash_table_lookup(pools, GINT_TO_POINTER(pool_index));

  // if pool exists
  if (pool_ref != NULL) {
    pool_ref->reference_count++;
    void *ret_val = ngx_palloc(pool_ref, size);
    // put into the mem hash
    g_hash_table_insert(memory_hash_table, ret_val, pool_ref);

    pthread_mutex_unlock(memory_info_mutex);
    return ret_val;
  }
  // if doesn't exist
  ngx_pool_t *new_pool = ngx_create_pool(sizeof(ngx_pool_t) + 8, NULL);
  new_pool->index = pool_index;
  // put into the pools hash
  g_hash_table_insert(pools, GINT_TO_POINTER(pool_index), new_pool);

  new_pool->reference_count++;
  void *ret_val = ngx_palloc(new_pool, size);
  // put into the mem hash
  g_hash_table_insert(memory_hash_table, ret_val, new_pool);

  pthread_mutex_unlock(memory_info_mutex);
  return ret_val;
#endif
}

void *calloc_impl(size_t nmemb, size_t size) {
#if !defined(USE_POOL_ALLOC)
  return calloc(nmemb, size);
#endif

#ifdef USE_POOL_ALLOC
  // 如果使用 pool alloc
  pthread_mutex_lock(memory_info_mutex);
  sem_wait(global_task_count_semaphore);
  const size_t current_task_count = global_task_count;
  sem_post(global_task_count_semaphore);
  // 获取 index
  size_t pool_index = current_task_count / BATCH_SIZE;
  ngx_pool_t *pool_ref =
      g_hash_table_lookup(pools, GINT_TO_POINTER(pool_index));
  // printf("lookup for %lu\n", pool_index);

  // if pool exists
  if (pool_ref != NULL) {
    pool_ref->reference_count++;
    void *ret_val = ngx_pcalloc(pool_ref, nmemb * size);
    // put into the mem hash
    g_hash_table_insert(memory_hash_table, ret_val, pool_ref);

    pthread_mutex_unlock(memory_info_mutex);
    return ret_val;
  }
  // if doesn't exist
  ngx_pool_t *new_pool = ngx_create_pool(sizeof(ngx_pool_t) + 8, NULL);
  new_pool->index = pool_index;
  // put into the pools hash
  g_hash_table_insert(pools, GINT_TO_POINTER(pool_index), new_pool);

  new_pool->reference_count++;
  void *ret_val = ngx_pcalloc(new_pool, nmemb * size);
  // put into the mem hash
  g_hash_table_insert(memory_hash_table, ret_val, new_pool);

  pthread_mutex_unlock(memory_info_mutex);
  return ret_val;
#endif
}

void free_impl(void *ptr) {
#if !defined(USE_POOL_ALLOC)
  free(ptr);
  return;
#endif
#ifdef USE_POOL_ALLOC
  // 如果使用 pool alloc
  pthread_mutex_lock(memory_info_mutex);

  // 释放需要的东西
  ngx_pool_t *pool_ref = g_hash_table_lookup(memory_hash_table, ptr);
  // 消除 mem_hash_table 的入口
  g_hash_table_remove(memory_hash_table, ptr);

  int stat = ngx_pfree(pool_ref, ptr);
  if (stat == NGX_DECLINED) {
    fprintf(stderr, "ngx_pfree error");
    exit(EXIT_FAILURE);
  }
  pool_ref->reference_count--;

  // 检测是否需要释放 pool
  // printf("%d: %zu\n", pool_ref->index, pool_ref->reference_count);
  // printf("%lu, pools: %u\n", global_task_count, g_hash_table_size(pools));
  if (pool_ref->reference_count == 0) {
    const int index = pool_ref->index;
    ngx_destroy_pool(pool_ref);
    // 消除 pools 中的入口
    if (g_hash_table_remove(pools, GINT_TO_POINTER(index)) == false) {
      printf("remove entry failed\n");
    }

    printf("successfully destroyed pool\n");
  }

  pthread_mutex_unlock(memory_info_mutex);
#endif
}
