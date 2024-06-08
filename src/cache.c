#include <glib.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/cache.h"

// 初始化该 handle
struct cached_file_handle *cached_file_handle_init(const char *path_to_file) {
  struct cached_file_handle *handle =
      (struct cached_file_handle *)calloc(1, sizeof(*handle));
  // 复制 path_to_file的 string
  handle->path_to_file = strdup(path_to_file);
  handle->contents = NULL;
  handle->readers_count = 0;

  if (pthread_mutex_init(cache_hash_table_mutex, NULL) != 0) {
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
void cached_file_handle_free(void *handle_passed) {
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

