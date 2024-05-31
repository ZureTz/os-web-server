#pragma once
#ifndef TYPE_H
#define TYPE_H

#include <semaphore.h>

#define VERSION 23
#define BUFSIZE 8096
#define ERROR 42
#define LOG 44
#define FORBIDDEN 403
#define NOTFOUND 404
#define LISTENQ 64

#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif

struct file_extension {
  const char *ext;
  const char *filetype;
};

// semaphores
extern sem_t *logging_semaphore;
extern sem_t *timer_semaphore;

// global timers
extern struct timespec *global_timer;

// file extensions
extern struct file_extension extensions[];

#endif