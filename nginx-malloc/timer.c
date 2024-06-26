#include <time.h>

#include "timer.h"

// 计时函数, 用来计算时间差
struct timespec timer_diff(const struct timespec start,
                           const struct timespec end) {
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

// 用来计算时间之和
struct timespec timer_add(const struct timespec time1,
                          const struct timespec time2) {
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

// 用来将 timespec 转换为以毫秒为单位的 double
double timespec_to_double_in_ms(const struct timespec time) {
  return time.tv_sec * 1000.0 + time.tv_nsec / 1000000.0;
}

// timer variables initialization
struct timespec thread_active_time = (struct timespec){
    .tv_sec = 0,
    .tv_nsec = 0,
};

struct timespec thread_block_time = (struct timespec){
    .tv_sec = 0,
    .tv_nsec = 0,
};