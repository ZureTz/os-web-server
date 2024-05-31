#include <bits/time.h>
#include <time.h>

#include "include/timer.h"

// 计时函数, 用来计算时间差
struct timespec timer_diff(struct timespec start, struct timespec end) {
  struct timespec result;
  if ((end.tv_nsec - start.tv_nsec) < 0) {
    result.tv_sec = end.tv_sec - start.tv_sec - 1;
    result.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    return result;
  }

  result.tv_sec = end.tv_sec - start.tv_sec;
  result.tv_nsec = end.tv_nsec - start.tv_nsec;
  return result;
}

struct timespec timer_add(struct timespec time1, struct timespec time2) {
  struct timespec result;
  if ((time1.tv_nsec + time2.tv_nsec) >= 1000000000) {
    result.tv_sec = time1.tv_sec + time2.tv_sec + 1;
    result.tv_nsec = time1.tv_nsec + time2.tv_nsec - 1000000000;
    return result;
  }

  result.tv_sec = time1.tv_sec + time2.tv_sec;
  result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
  return result;
}