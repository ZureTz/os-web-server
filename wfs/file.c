#include <bits/time.h>
#include <glib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "file.h"

// 初始化该 handle
struct file_handle *file_handle_init(const char *path_to_file) {
  struct file_handle *handle = (struct file_handle *)calloc(1, sizeof(*handle));
  // 复制 path_to_file的 string
  handle->path_to_file = strdup(path_to_file);
  handle->contents = NULL;

  return handle;
}

GList *find_offset_content(const struct file_handle *const handle,
                           off_t *const offset) {
  if (*offset == 0) {
    return handle->contents;
  }
  GList *ptr = handle->contents;
  GList *prev = ptr;
  for (; ptr && ((*offset - ((struct string_fragment *)(ptr->data))->size) > 0);
       ptr = ptr->next) {
    *offset -= ((struct string_fragment *)(ptr->data))->size;
    prev = ptr;
  }

  if (ptr == NULL) {
    return prev;
  }

  return ptr;
}

size_t read_offset_content(char *buffer, struct file_handle *handle,
                           size_t size, off_t offset) {
  GList *frag_pos = find_offset_content(handle, &offset);
  if (frag_pos == NULL) {
    return 0;
  }
  // printf("File: %s, Size: %zu\n", handle->path_to_file, ((struct string_fragment *)(frag_pos->data))->size);

  size_t counter = 0, prev = 0;
  for (; size-- > 0; counter++) {
    if (offset + counter ==
        ((struct string_fragment *)(frag_pos->data))->size) {
      offset = 0;
      if (frag_pos->next == NULL) {
        break;
      }
      frag_pos = frag_pos->next;
      prev += counter;
      counter = 0;
    }

    buffer[prev + counter] =
        ((struct string_fragment *)(frag_pos->data))->content[offset + counter];
  }
  return prev + counter;
}

// 给 file_handle 加上 content
void file_handle_add_content(struct file_handle *const handle,
                             const char *content, const size_t content_size,
                             off_t offset) {
  off_t new_fragment_offset = offset;
  GList *frag_pos = find_offset_content(handle, &new_fragment_offset);

  struct string_fragment *new_fragment =
      (struct string_fragment *)malloc(sizeof(*new_fragment));
  // strdup 会在数据为 0 的时候停止，不能使用（尤其针对 binary 例如图片）
  // "new_fragment->content = strndup(content, content_size + 8);" is wrong!
  // 所以改用 malloc 加 memcpy的形式
  new_fragment->content =
      (char *)malloc((content_size + new_fragment_offset + 8) * sizeof(char));
  // 拷贝以前的内容
  // 当原来的 list 不为空的时候
  if (frag_pos != NULL) {
    memcpy(new_fragment->content,
           ((struct string_fragment *)(frag_pos->data))->content,
           new_fragment_offset);
  }

  // 拷贝新内容
  memcpy(new_fragment->content + new_fragment_offset, content, content_size);
  new_fragment->size = new_fragment_offset + content_size;
  // printf("new_fragment->size = %zu\n", new_fragment->size);

  // list 不为空的时候
  if (frag_pos != NULL) {
    string_fragment_free(frag_pos->data);
  }

  handle->contents = g_list_remove(handle->contents, frag_pos);
  handle->contents = g_list_append(handle->contents, new_fragment);
}

// 释放 handle
void file_handle_free(gpointer handle_passed) {
  struct file_handle *handle = (struct file_handle *)handle_passed;

  // 1. 释放 contents
  g_list_free_full(handle->contents, string_fragment_free);

  // 2. 释放 path to file
  free(handle->path_to_file);

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
