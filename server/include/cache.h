#pragma once
#ifndef CACHE_H
#define CACHE_H

#include <glib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "types.h"

// 缓存文件的 handle，含有键值对，哈希表用
struct cached_file_handle {
  char *path_to_file; // 代表文件的路径
  // 文件的起始链表节点
  // 对于大文件，单个的buffer不够储存，使用 list 存储内容
  GList *contents;

  // 读者数量
  int readers_count;
  // 释放 content 时的锁
  pthread_mutex_t content_free_lock;
  pthread_cond_t content_has_reader;

#ifdef USE_LRU
  // LRU: 最近使用时间
  struct timespec recent_used_time;
  // 读写最近使用时间的锁
  pthread_mutex_t recent_used_time_mutex;
#endif

#ifdef USE_LFU
  // LRU: 最近使用次数
  unsigned long used_times;
  // 读写最近使用时间的锁
  pthread_mutex_t used_times_mutex;
#endif
};

// 初始化该 handle
struct cached_file_handle *cached_file_handle_init(const char *path_to_file);

// 给 cached_file_handle 加上 content 的内容
void cached_file_handle_add_content(struct cached_file_handle *const handle,
                                    char *content, const size_t content_size);

// 释放 handle
// hash 表使用，**不要直接调用**
void cached_file_handle_free(gpointer handle);
// 释放 handle 的线程池里面的线程做的事
void *cached_file_handle_free_thread_do(gpointer args);

// content 中指针指向的每个小字符串碎片
struct string_fragment {
  // 内容
  char *content;
  // 内容长度
  size_t size;
};

// 字符串碎片释放，**不要直接调用**
void string_fragment_free(gpointer string_fragment);

// hash 表，用来储存不同的 handle
// key: char * path_to_file
// value: struct cached_file_handle *handle
extern GHashTable *cache_hash_table;

// hash 表的 read write mutex
// 不仅保护 hash 表，而且保护 LRU Tree / LFU Tree, 取决于所用的替换算法
extern pthread_mutex_t *cache_hash_table_mutex;

// 缓存hit/miss次数, protected by cache_hash_table_mutex
extern unsigned long cache_hit_times;
extern unsigned long cache_miss_times;

// LRU utilities
#ifdef USE_LRU

// LRU 算法所用的 tree
extern GTree *LRU_tree;
// LRU 算法所用的 tree 的互斥锁
extern pthread_mutex_t *LRU_tree_mutex;

// LRU 算法所用的 tree node
struct LRU_tree_node {
  struct timespec recent_used_time;
  char *path_to_file;
};

// 销毁 node
void LRU_tree_node_destroy(gpointer node);

// 插入 node 到 LRU tree 中
void LRU_tree_update(struct LRU_tree_node *new_node,
                     struct timespec last_used_time);

// Tree 中 node 的比较函数
gint LRU_tree_node_cmp(gconstpointer a, gconstpointer b, gpointer user_data);

// LRU替换算法
void LRU_replace(struct cached_file_handle *handle);

#endif

#ifdef USE_LFU

// LFU utilities

// LFU 算法所用的 tree
extern GTree *LFU_tree;
// LFU 算法所用的 tree 的互斥锁
extern pthread_mutex_t *LFU_tree_mutex;

// LRU 算法所用的 tree node
struct LFU_tree_node {
  unsigned long used_times;
  char *path_to_file;
};

// 销毁 node
void LFU_tree_node_destroy(gpointer node);

// 插入 node 到 LRU tree 中
void LFU_tree_update(struct LFU_tree_node *new_node,
                     unsigned long last_used_times);

// Tree 中 node 的比较函数
gint LFU_tree_node_cmp(gconstpointer a, gconstpointer b, gpointer user_data);

// LRU替换算法
void LFU_replace(struct cached_file_handle *handle);

#endif

#endif