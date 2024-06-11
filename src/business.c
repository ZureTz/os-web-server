#include <bits/time.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "include/business.h"
#include "include/cache.h"
#include "include/logger.h"
#include "include/threadpool.h"
#include "include/types.h"

#if !defined(USE_LRU) && !defined(USE_LFU)
#error "Please select an page replacement algorithm(USE_LRU or USE_LFU)."
#endif

// 读消息
void *read_message(struct read_message_args *const args) {
  const int socketfd = args->socketfd;
  const int hit = args->hit;

  // 释放传进来的参数
  free(args);

  // ** 记得完成回应后释放 buffer **
  char *const buffer = (char *)calloc(BUFSIZE + 1, sizeof(char)); // 设置缓冲区

  const int socket_read_ret =
      read(socketfd, buffer, BUFSIZE); // 从连接通道中读取客户端的请求消息
  if (socket_read_ret == 0 || socket_read_ret == -1) {
    // 如果读取客户端消息失败，则向客户端发送 HTTP 失败响应信息
    logger(FORBIDDEN, "failed to read browser request", "", socketfd);
    close(socketfd);
    return NULL;
  }

  if (socket_read_ret > 0 && socket_read_ret < BUFSIZE) {
    // 设置有效字符串，即将字符串尾部表示为 0
    buffer[socket_read_ret] = 0;
  } else {
    buffer[0] = 0;
  }

  for (long i = 0; i < socket_read_ret; i++) {
    // 移除消息字符串中的“CF”和“LF”字符
    if (buffer[i] == '\r' || buffer[i] == '\n') {
      buffer[i] = '*';
    }
  }

  logger(LOG, "request", buffer, hit);

  // 判断客户端 HTTP 请求消息是否为 GET 类型，如果不是则给出相应的响应消息

  if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
    logger(FORBIDDEN, "Only simple GET operation supported", buffer, socketfd);
    close(socketfd);
    return NULL;
  }

  int buflen = 0;
  for (long i = 4; i < BUFSIZE; i++) {
    // null terminate after the second space to ignore extra stuff
    if (buffer[i] == ' ') { // string is "GET URL " + lots of other stuff
      buffer[i] = 0;
      // set length of the buffer to i
      buflen = i;
      break;
    }
  }

  for (long j = 0; j < buflen - 1; j++) {
    // 在消息中检测路径，不允许路径中出现 '.'
    if (buffer[j] == '.' && buffer[j + 1] == '.') {
      logger(FORBIDDEN, "Parent directory (..) path names not supported",
             buffer, socketfd);
      close(socketfd);
      return NULL;
    }
  }
  if (!strncmp(&buffer[0], "GET /\0", 6) ||
      !strncmp(&buffer[0], "get /\0", 6)) {
    // 如果请求消息中没有包含有效的文件名，则使用默认的文件名 index.html
    strcpy(buffer, "GET /index.html");
  }

  // 根据预定义在 extensions 中的文件类型，检查请求的文件类型是否本服务器支持

  buflen = strlen(buffer);
  const char *filetype = NULL;

  for (long i = 0; extensions[i].ext != 0; i++) {
    long len = strlen(extensions[i].ext);
    if (!strncmp(&buffer[buflen - len], extensions[i].ext, len)) {
      filetype = extensions[i].filetype;
      break;
    }
  }

  if (filetype == NULL) {
    logger(FORBIDDEN, "file extension type not supported", buffer, socketfd);
    close(socketfd);
    return NULL;
  }

  // 接下来，调用 read_file

  // 设定参数
  struct read_file_args *const next_args =
      (struct read_file_args *)malloc(sizeof(*next_args));
  // ** 记得完成回应后释放 buffer **
  next_args->buffer = buffer;
  next_args->socketfd = socketfd;
  next_args->filetype = filetype;
  next_args->hit = hit;

  // 创建 task
  task *const new_task = (task *)malloc(sizeof(task));
  new_task->next = NULL;
  new_task->function = (void *)read_file;
  new_task->arg = next_args;

  // 送进 filename queue
  add_task_to_thread_pool(read_file_pool, new_task);

  return NULL;
}

// 打开文件，读文件，调用 send message
void *read_file(struct read_file_args *const args) {
  // 获得参数内容
  char *const buffer = args->buffer;
  const int socketfd = args->socketfd;
  const char *const filetype = args->filetype;
  const int hit = args->hit;

  // 释放参数
  free(args);

  pthread_mutex_lock(cache_hash_table_mutex);
  // 从 hash table 中寻找文件名
  struct cached_file_handle *found_handle =
      g_hash_table_lookup(cache_hash_table, &buffer[5]);

  // 如果文件名找到，无需读文件，直接进行 send_cached_message
  if (found_handle != NULL) {
    // cache hit 的次数增加
    cache_hit_times++;
    // 将要使用该 handle 的任务增加一个
    pthread_mutex_lock(&found_handle->content_free_lock);
    found_handle->readers_count++;
    pthread_mutex_unlock(&found_handle->content_free_lock);
    pthread_mutex_unlock(cache_hash_table_mutex);

    // log，然后释放当前 buffer
    logger(LOG, "SEND_CACHED", &buffer[5], hit);
    free(buffer);

    // 创建新任务执行 send_cached_message
    struct send_cached_message_args *const next_args =
        (struct send_cached_message_args *)malloc(sizeof(*next_args));
    next_args->socketfd = socketfd;
    next_args->handle = found_handle;

    task *new_task = (task *)malloc(sizeof(task));
    new_task->next = NULL;
    new_task->function = (void *)send_cached_message;
    new_task->arg = next_args;

    add_task_to_thread_pool(send_message_pool, new_task);

    return NULL;
  }

  // 没有找到需要打开文件，读文件，再进行 发送消息：(cache miss)
  cache_miss_times++;
  pthread_mutex_unlock(cache_hash_table_mutex);

  // 打开文件
  int filefd = -1;
  if ((filefd = open(&buffer[5], O_RDONLY)) == -1) { // 打开指定的文件名
    logger(NOTFOUND, "failed to open file", &buffer[5], socketfd);
    close(socketfd);
    return NULL;
  }

  logger(LOG, "SEND", &buffer[5], hit);

  // 文件存在，创建新的 handle，准备放入
  struct cached_file_handle *new_handle = cached_file_handle_init(&buffer[5]);

  off_t len = lseek(filefd, (off_t)0, SEEK_END); // 通过 lseek 获取文件长度
  lseek(filefd, (off_t)0, SEEK_SET); // 将文件指针移到文件首位置

  sprintf(buffer,
          "HTTP/1.1 200 OK\n"
          "Server: nweb/%d.0\n"
          "Content-Length: %ld\n"
          "Connection: close\n"
          "Content-Type: %s",
          VERSION, len, filetype); // Header without a blank line

  logger(LOG, "Header", buffer, hit);

  sprintf(buffer,
          "HTTP/1.1 200 OK\n"
          "Server: nweb/%d.0\n"
          "Content-Length: %ld\n"
          "Connection: close\n"
          "Content-Type: %s\n\n",
          VERSION, len, filetype); // Header + a blank line

  // 准备调用 send_message
  struct send_mesage_args *const next_args =
      (struct send_mesage_args *)malloc(sizeof(*next_args));
  next_args->filefd = filefd;
  next_args->socketfd = socketfd;
  next_args->buffer = buffer;
  next_args->handle = new_handle;

  // 创建 task
  task *const new_task = (task *)malloc(sizeof(task));
  new_task->next = NULL;
  new_task->function = (void *)send_mesage;
  new_task->arg = next_args;

  // write to socketfd;
  add_task_to_thread_pool(send_message_pool, new_task);

  return NULL;
}

// 发送消息
void *send_mesage(struct send_mesage_args *const args) {
  const int filefd = args->filefd;
  const int socketfd = args->socketfd;
  char *const buffer = args->buffer;
  struct cached_file_handle *const handle = args->handle;

  // 释放传进来的参数
  free(args);

  // 使用信号量保证回应的连续性
  if (sem_wait(output_sempaphore) < 0) {
    perror("sem_wait");
    exit(EXIT_FAILURE);
  }

  // CRITICAL SECTION BEGINS

  const size_t header_length = strlen(buffer);
  // 写 header
  write(socketfd, buffer, header_length);
  // 同时放进 handle 内
  cached_file_handle_add_content(handle, buffer, header_length);

  // 不停地从文件里读取文件内容，并通过 socket 通道向客户端返回文件内容
  int bytes_to_write = -1;
  while ((bytes_to_write = read(filefd, buffer, BUFSIZE)) > 0) {
    // 写 body
    write(socketfd, buffer, bytes_to_write);
    // 同时放进 handle 内
    cached_file_handle_add_content(handle, buffer, bytes_to_write);
  }

  // 关闭文件，关闭 socket
  close(filefd);
  close(socketfd);

  // 释放 buffer
  free(buffer);

  // 将新的 handle 放入 hash 表中
  pthread_mutex_lock(cache_hash_table_mutex);

  // 检查是否已经存在该 entry
  if (g_hash_table_contains(cache_hash_table, handle->path_to_file)) {
    // 丢弃当前 handle
    cached_file_handle_free(handle);
  } else {
    // 否则，执行 LRU / LFU 操作
#ifdef USE_LRU
    LRU_replace(handle);
#endif

#ifdef USE_LFU
    LFU_replace(handle);
#endif
  }

  pthread_mutex_unlock(cache_hash_table_mutex);

  // CRITICAL SECTION ENDS
  if (sem_post(output_sempaphore) < 0) {
    perror("sem_post");
    exit(EXIT_FAILURE);
  }

  return NULL;
}

// 发送已经缓存的消息
void *send_cached_message(struct send_cached_message_args *const args) {
  // 获得 handle 和 socket fd
  const int socketfd = args->socketfd;
  struct cached_file_handle *handle = args->handle;
  free(args);

#ifdef USE_LRU
  // 更新 handle 的使用时间
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
#endif

#ifdef USE_LFU
  // 更新 handle 的使用次数
  pthread_mutex_lock(&handle->used_times_mutex);
  unsigned long last_used_times = handle->used_times;
  handle->used_times++;

  // 创建新 node 节点, 准备更新到 LFU tree 中
  struct LFU_tree_node *new_node =
      (struct LFU_tree_node *)malloc(sizeof(*new_node));
  new_node->used_times = handle->used_times;
  new_node->path_to_file = handle->path_to_file;
  pthread_mutex_unlock(&handle->used_times_mutex);

  // 更新 LFU Tree
  LFU_tree_update(new_node, last_used_times);
#endif

  // 使用信号量保证回应的连续性
  if (sem_wait(output_sempaphore) < 0) {
    perror("sem_wait");
    exit(EXIT_FAILURE);
  }

  // CRITICAL SECTION BEGINS
  GList *ptr = handle->contents;

  // 不停地 content list 读取文件内容，并通过 socket 通道向客户端返回文件内容
  while (ptr != NULL) {
    // printf("ptr: %p\n", ptr);

    // 写 body
    write(socketfd, ((struct string_fragment *)(ptr->data))->content,
          ((struct string_fragment *)(ptr->data))->size);
    ptr = ptr->next;
  }

  // 关闭 socketfd 这个很重要！！！不加就是在吃系统资源
  close(socketfd);

  // 将要使用该 handle 的任务减少一个
  pthread_mutex_lock(&handle->content_free_lock);
  handle->readers_count--;
  // 如果为 0，即可释放
  if (handle->readers_count == 0) {
    pthread_cond_signal(&handle->content_has_reader);
  }
  pthread_mutex_unlock(&handle->content_free_lock);

  // CRITICAL SECTION ENDS
  if (sem_post(output_sempaphore) < 0) {
    perror("sem_post");
    exit(EXIT_FAILURE);
  }

  return NULL;
}