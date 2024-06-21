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

#endif