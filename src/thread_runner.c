#define _GNU_SOURCE

#include <bits/time.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "include/thread_runner.h"
#include "include/timer.h"
#include "include/types.h"
#include "include/web.h"

// 子线程进行的任务
void *thread_runner(void *args) {
  struct thread_runner_arg *processed_args = (struct thread_runner_arg *)args;
  const int socketfd = processed_args->socketfd;
  const int hit = processed_args->hit;

  // 计时器开始
  struct timespec thread_start_time;
  clock_gettime(CLOCK_REALTIME, &thread_start_time);

  // 回应请求
  web(socketfd, hit);

  // 计时器结束
  struct timespec thread_end_time;
  clock_gettime(CLOCK_REALTIME, &thread_end_time);

  // 计算时间差 diff
  const struct timespec diff = timer_diff(thread_start_time, thread_end_time);

  // 获取 tid
  const pid_t tid = syscall(__NR_gettid);
  // 输出当前进程时间
  printf("Thread %d costs: %lds %ldns\n", tid, diff.tv_sec, diff.tv_nsec);

  // 等待 semaphore
  if (sem_wait(timer_semaphore) < 0) {
    perror("sem_wait failed");
    exit(EXIT_FAILURE);
  }

  // 计时器增加
  *global_timer = timer_add(*global_timer, diff);

  // 释放 semaphore
  if (sem_post(timer_semaphore) < 0) {
    perror("sem_post failed");
    exit(EXIT_FAILURE);
  }

  // 释放传递的参数
  free(processed_args);

  // 退出
  pthread_exit(NULL);
}
