#include <fcntl.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "include/business.h"
#include "include/logger.h"
#include "include/threadpool.h"
#include "include/types.h"

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

  // 打开文件
  int filefd = -1;
  if ((filefd = open(&buffer[5], O_RDONLY)) == -1) { // 打开指定的文件名
    logger(NOTFOUND, "failed to open file", &buffer[5], socketfd);
    close(socketfd);
    return NULL;
  }

  logger(LOG, "SEND", &buffer[5], hit);

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

  // 释放传进来的参数
  free(args);

  // 使用信号量保证回应的连续性
  if (sem_wait(output_sempaphore) < 0) {
    perror("sem_wait");
    exit(EXIT_FAILURE);
  }

  // CRITICAL SECTION BEGINS

  // 写 header
  write(socketfd, buffer, strlen(buffer));

  // 写 body
  // 不停地从文件里读取文件内容，并通过 socket 通道向客户端返回文件内容
  int bytes_to_write = -1;
  while ((bytes_to_write = read(filefd, buffer, BUFSIZE)) > 0) {
    write(socketfd, buffer, bytes_to_write);
  }

  // CRITICAL SECTION ENDS
  if (sem_post(output_sempaphore) < 0) {
    perror("sem_post");
    exit(EXIT_FAILURE);
  }

  // 关闭文件，关闭 socket
  close(filefd);
  close(socketfd);
 
  // 释放 buffer
  free(buffer);

  return NULL;
}