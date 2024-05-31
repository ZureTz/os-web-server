#pragma once
#ifndef TIMER_H
#define TIMER_H

struct timespec timer_diff(struct timespec start, struct timespec end);
struct timespec timer_add(struct timespec time1, struct timespec time2);

#endif