#pragma once
#ifndef BUSINESS_H
#define BUSINESS_H

#include <sys/types.h>

#include "cache.h"

// 三种业务的不同参数

// 读消息的参数
struct read_message_args {
  int socketfd;
  int hit;
};

// 读文件的参数
struct read_file_args {
  // 缓冲区
  char *buffer;
  // socket file descriptor
  int socketfd;
  // 文件类型
  const char *filetype;
  // logger 要使用的 hit
  int hit;
};

// 发送回应的参数
struct send_mesage_args {
  int filefd;
  // socket file descriptor
  int socketfd;
  // 缓冲区
  char *buffer;
  // 新创建的 handle
  struct cached_file_handle *handle;
};

// 发送已经缓存的回应的参数
struct send_cached_message_args {
  // socket file descriptor
  int socketfd;
  struct cached_file_handle *handle;
};

// 读消息
void *read_message(struct read_message_args *const args);

// 读文件
void *read_file(struct read_file_args *const args);

// 发送消息
void *send_mesage(struct send_mesage_args *const args);

// 发送已经缓存的消息
void *send_cached_message(struct send_cached_message_args *const args);

#endif