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

struct file_extension {
  const char *ext;
  const char *filetype;
};

struct thread_runner_arg {
  int socketfd;
  int hit;
};

// semaphores
extern sem_t *logging_semaphore;
extern sem_t *timer_semaphore;

// global timers
extern struct timespec *global_thread_timer;
extern struct timespec *global_rsocket_timer;
extern struct timespec *global_wsocket_timer;
extern struct timespec *global_rfile_timer;
extern struct timespec *global_logger_timer;

// 创建的子线程的数量
extern long thread_count;

// 创建的 thread pool 指针，销毁的时候使用
extern threadpool* global_pool;

// file extensions
extern struct file_extension extensions[];

#endif