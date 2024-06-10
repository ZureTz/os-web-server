#include <bits/time.h>
#include <glib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "include/cache.h"
#include "include/threadpool.h"

// 初始化该 handle
struct cached_file_handle *cached_file_handle_init(const char *path_to_file) {
  struct cached_file_handle *handle =
      (struct cached_file_handle *)calloc(1, sizeof(*handle));
  // 复制 path_to_file的 string
  handle->path_to_file = strdup(path_to_file);
  handle->contents = NULL;
  handle->readers_count = 0;
  handle->recent_used_time.tv_sec = handle->recent_used_time.tv_nsec = 0;

  // 初始化各个 mutex
  if (pthread_mutex_init(&handle->content_free_lock, NULL) != 0) {
    perror("pthread_mutex_init failed");
    exit(EXIT_FAILURE);
  }

  if (pthread_mutex_init(&handle->recent_used_time_mutex, NULL) != 0) {
    perror("pthread_mutex_init failed");
    exit(EXIT_FAILURE);
  }

  // 初始化条件变量属性
  pthread_condattr_t cattr;
  if (pthread_condattr_init(&cattr) != 0) {
    perror("pthread_condattr_init failed");
    exit(EXIT_FAILURE);
  }

  // 初始化条件变量
  if (pthread_cond_init(&handle->content_has_reader, &cattr) != 0) {
    perror("pthread_cond_init failed");
    exit(EXIT_FAILURE);
  }

  return handle;
}

// 给 cached_file_handle 加上 content
void cached_file_handle_add_content(struct cached_file_handle *const handle,
                                    char *content, const size_t content_size) {
  struct string_fragment *new_fragment =
      (struct string_fragment *)malloc(sizeof(*new_fragment));

  // strdup 会在数据为 0 的时候停止，不能使用（尤其针对 binary 例如图片）
  // "new_fragment->content = strndup(content, content_size + 8);" is wrong!
  // 所以改用 malloc 加 memcpy的形式
  new_fragment->content = (char *)malloc((content_size + 8) * sizeof(char));
  memcpy(new_fragment->content, content, content_size);

  new_fragment->size = content_size;

  handle->contents = g_list_append(handle->contents, new_fragment);
}

// 释放 handle
void cached_file_handle_free(gpointer handle_passed) {
  task *new_task = (task *)malloc(sizeof(task));
  new_task->next = NULL;
  new_task->function = cached_file_handle_free_thread_do;
  new_task->arg = handle_passed;

  add_task_to_thread_pool(free_memory_pool, new_task);
}

void *cached_file_handle_free_thread_do(gpointer handle_passed) {
  struct cached_file_handle *handle =
      (struct cached_file_handle *)handle_passed;
  // 避免释放的时候仍然有进程读取该 handle 的问题
  pthread_mutex_lock(&handle->content_free_lock);
  // 等待直到没有读者
  while (handle->readers_count > 0) {
    pthread_cond_wait(&handle->content_has_reader, &handle->content_free_lock);
  }

  // 1. 释放 contents
  g_list_free_full(handle->contents, string_fragment_free);

  // 2. 释放 path to file
  free(handle->path_to_file);

  pthread_mutex_unlock(&handle->content_free_lock);

  // 3. 释放 handle
  free(handle);

  return NULL;
}

// 释放字符串碎片
void string_fragment_free(void *string_fragment_passed) {
  struct string_fragment *string_fragment =
      (struct string_fragment *)string_fragment_passed;

  // 释放字符串
  free(string_fragment->content);

  // 释放自身
  free(string_fragment);
}

// 销毁 node
void LRU_tree_node_destroy(gpointer node) { free(node); }

// 插入 node 到 LRU tree 中
void LRU_tree_update(struct LRU_tree_node *new_node,
                     struct timespec last_used_time) {
  struct LRU_tree_node temp_node;
  temp_node.recent_used_time = last_used_time;
  temp_node.path_to_file = new_node->path_to_file;

  pthread_mutex_lock(LRU_tree_mutex);
  g_tree_remove(LRU_tree, &temp_node);
  g_tree_insert(LRU_tree, new_node, NULL);
  pthread_mutex_unlock(LRU_tree_mutex);
}

// Tree 中 node 的比较函数
gint LRU_tree_node_cmp(gconstpointer a, gconstpointer b, gpointer user_data) {
  struct LRU_tree_node *const a_node = (struct LRU_tree_node *)a;
  struct LRU_tree_node *const b_node = (struct LRU_tree_node *)b;
  // ignore user data
  (void)user_data;

  if (a_node->recent_used_time.tv_sec == b_node->recent_used_time.tv_sec) {
    return a_node->recent_used_time.tv_nsec - b_node->recent_used_time.tv_nsec;
  }
  return a_node->recent_used_time.tv_sec - b_node->recent_used_time.tv_sec;
}

// debugdebugdebugdebugdebugdebugdebug
// gboolean traverse(gpointer key, gpointer value, gpointer data) {
  // (void)value;
  // (void)data;
  // struct LRU_tree_node *node = key;
// 
  // printf("Traversed %s: @%ld, %ld\n", node->path_to_file,
        //  node->recent_used_time.tv_sec, node->recent_used_time.tv_nsec);
  // return false;
// }
// gubedgubedgubedgubedgubedgubedgubed

// LRU替换算法
void LRU_replace(struct cached_file_handle *handle) {
  // 记录最近使用时间
  pthread_mutex_lock(&handle->recent_used_time_mutex);
  struct timespec last_used_time = handle->recent_used_time;
  clock_gettime(CLOCK_REALTIME, &handle->recent_used_time);

  // 创建新 node 节点, 准备更新到 LRU tree 中
  struct LRU_tree_node *new_node =
      (struct LRU_tree_node *)malloc(sizeof(*new_node));
  new_node->recent_used_time = handle->recent_used_time;
  new_node->path_to_file = handle->path_to_file;
  pthread_mutex_unlock(&handle->recent_used_time_mutex);

  // 更新 LRU Tree
  LRU_tree_update(new_node, last_used_time);

  // 如果 hash 表未满，则直接加入
  if (g_hash_table_size(cache_hash_table) < MAX_HASH_TABLE_SIZE) {
    g_hash_table_insert(cache_hash_table, handle->path_to_file, handle);
    return;
  }

  // 否则, 替换最近未使用过的
  pthread_mutex_lock(LRU_tree_mutex);
  struct LRU_tree_node *least_recent_used_key =
      g_tree_node_key(g_tree_node_first(LRU_tree));
  char *const path_to_file = least_recent_used_key->path_to_file;
  // debugdebugdebugdebugdebugdebugdebug
  // g_tree_foreach(LRU_tree, traverse, NULL);
  // printf("Earliest used %s: @%ld, %ld\n\n", path_to_file,
        //  least_recent_used_key->recent_used_time.tv_sec,
        //  least_recent_used_key->recent_used_time.tv_nsec);
  // gubedgubedgubedgubedgubedgubedgubed

  // 删除 tree 中 节点
  g_tree_remove(LRU_tree, least_recent_used_key);
  pthread_mutex_unlock(LRU_tree_mutex);

  // 删除 hash 表中对应的内容
  g_hash_table_remove(cache_hash_table, path_to_file);

  // 加入新内容
  g_hash_table_insert(cache_hash_table, handle->path_to_file, handle);
}
