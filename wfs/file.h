#pragma once
#ifndef FILE_H
#define FILE_H

#include <glib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

// 缓存文件的 handle，含有键值对，哈希表用
struct file_handle {
  char *path_to_file; // 代表文件的路径
  // 文件的起始链表节点
  // 对于大文件，单个的buffer不够储存，使用 list 存储内容
  GList *contents;
};

// 初始化该 handle
struct file_handle *file_handle_init(const char *path_to_file);

GList *find_offset_content(const struct file_handle *const handle,
                           off_t *const offset);

size_t read_offset_content(char *buffer, struct file_handle *handle, size_t size,
                         off_t offset);

// 给 file_handle 加上 content 的内容
void file_handle_add_content(struct file_handle *const handle,
                             const char *content, const size_t content_size,
                             off_t offset);

// 释放 handle
// hash 表使用，**不要直接调用**
void file_handle_free(gpointer handle);

// content 中指针指向的每个小字符串碎片
struct string_fragment {
  // 内容
  char *content;
  // 内容长度
  size_t size;
};

// 字符串碎片释放，**不要直接调用**
void string_fragment_free(gpointer string_fragment);

#endif