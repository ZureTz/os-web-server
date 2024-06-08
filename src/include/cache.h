#pragma once
#ifndef CACHE_H
#define CACHE_H

#include <glib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_HASH_TABLE_SIZE 64

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
};

// 初始化该 handle
struct cached_file_handle *cached_file_handle_init(const char *path_to_file);

// 给 cached_file_handle 加上 content 的内容
void cached_file_handle_add_content(struct cached_file_handle *const handle,
                                    char *content, const size_t content_size);

// 释放 handle
// hash 表使用，**不要直接调用**
void cached_file_handle_free(gpointer handle);

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

// LRU utilities

// LRU 算法所用的 tree
extern GTree *LRU_tree;

// LRU 算法所用的 tree node
struct LRU_tree_node {};

// 插入 node
void LRU_tree_insert(struct LRU_tree_node *node);

// Tree 中 node 的比较函数
gint LRU_tree_node_cmp(gconstpointer a, gconstpointer b, gpointer user_data);

// 销毁 node
void LRU_tree_node_destroy(gpointer key);

// LRU替换算法
void LRU_replace(GHashTable *const hash_table,
                 struct cached_file_handle *handle);

// LFU utilities

// LFU 算法所用的 tree
extern GTree *LFU_tree;

// LRU 算法所用的 tree node
struct LFU_tree_node {};

// 插入 node
void LFU_tree_insert(struct LFU_tree_node *node);

// Tree 中 node 的比较函数
gint LFU_tree_node_cmp(gconstpointer a, gconstpointer b, gpointer user_data);

// 销毁 node
void LFU_tree_node_destroy(gpointer key);

// LFU 替换算法
void LFU_replace(GHashTable *const hash_table,
                 struct cached_file_handle *handle);

#endif