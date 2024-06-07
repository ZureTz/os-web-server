#pragma once
#ifndef TYPE_H
#define TYPE_H

#include <semaphore.h>

#include "threadpool.h"

#define BUFSIZE 8096
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