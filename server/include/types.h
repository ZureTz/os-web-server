#pragma once
#ifndef TYPES_H
#define TYPES_H

#include <semaphore.h>

#define BUFSIZE 8192
#define ERROR 42
#define FORBIDDEN 403
#define LISTENQ 64
#define LOG 44
#define NOTFOUND 404
#define VERSION 23

#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif

#define NUM_THREADS 8

#define MAX_HASH_TABLE_SIZE 16384

// 使用 LRU 或者 LFU
#define USE_LRU
// #define USE_LFU

// 使用传统或者内存池
// #define USE_POOL_ALLOC

// file extensions
struct file_extension {
  const char *ext;
  const char *filetype;
};
extern struct file_extension extensions[];

// semaphores

// logger 写文件使用的信号量
extern sem_t *logging_semaphore;

// 回应请求所用信号量
extern sem_t *output_sempaphore;

// 计算平均活跃和阻塞时间之和所用的信号量
extern sem_t *thread_active_time_sempaphore;
extern sem_t *thread_block_time_sempaphore;

#endif