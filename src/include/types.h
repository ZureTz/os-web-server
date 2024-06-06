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

// semaphores

// logger 写文件使用的信号量
extern sem_t *logging_semaphore;

// 回应请求所用信号量
extern sem_t *output_sempaphore;

// file extensions
extern struct file_extension extensions[];

#endif