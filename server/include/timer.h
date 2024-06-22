#pragma once
#ifndef TIMER_H
#define TIMER_H

#include <time.h>

// 计时函数, 用来计算时间差
struct timespec timer_diff(const struct timespec start,
                           const struct timespec end);

// 用来计算时间之和
struct timespec timer_add(const struct timespec time1,
                          const struct timespec time2);

// 用来将 timespec 转换为以毫秒为单位的 double
double timespec_to_double_in_ms(const struct timespec time);

// 监测性能用的 monitor
void *monitor(void);

// 线程活跃时间相加（会在 thread_do 中调用）
// source: given argument
// destination: thread_active_time
void thread_active_time_add(const struct timespec source);

// 线程阻塞时间相加（会在 thread_do 中调用)
// source: given argument
// destination: thread_block_time
void thread_block_time_add(const struct timespec source);

#endif