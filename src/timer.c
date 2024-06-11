#include <bits/time.h>
#include <glib.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "include/cache.h"
#include "include/threadpool.h"
#include "include/timer.h"
#include "include/types.h"

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

// 监测性能用的 monitor (单独新增一个线程用于计时)
void *monitor(void) {
  // 持续运行，其中每次睡眠 3s
  for (unsigned long count = 1;; count++) {
    sleep(3);
    printf("\n第 %lu 次性能监测结果如下:\n", count);
    // 计算时间关系 (总时间 -> 平均时间)
    const double average_thread_active_time =
        timespec_to_double_in_ms(thread_active_time) / (3.0 * NUM_THREADS);
    const double average_thread_block_time =
        timespec_to_double_in_ms(thread_block_time) / (3.0 * NUM_THREADS);
    const double average_thread_running_time =
        average_thread_active_time + average_thread_block_time;
    // 输出线程的平均活跃时间，阻塞时间
    printf("线程平均运行时间为: %fms\n", average_thread_running_time);
    printf("线程平均活跃时间为: %fms\n", average_thread_active_time);
    printf("线程平均阻塞时间为: %fms\n", average_thread_block_time);

    // 计算每个线程池的最高活跃线程数和最低活跃线程数
    pthread_mutex_lock(&read_message_pool->thread_count_lock);
    const int max_read_message_thread_working =
        read_message_pool->max_num_working;
    const int min_read_message_thread_working =
        read_message_pool->min_num_working;
    pthread_mutex_unlock(&read_message_pool->thread_count_lock);

    // 输出各个线程池的最高活跃线程数和最低活跃线程数
    printf("read_message_pool:\n\t"
           "最高活跃: %d threads, 最低活跃：%d threads\n",
           max_read_message_thread_working, min_read_message_thread_working);

    pthread_mutex_lock(&read_file_pool->thread_count_lock);
    const int max_read_file_thread_working = read_file_pool->max_num_working;
    const int min_read_file_thread_working = read_file_pool->min_num_working;
    pthread_mutex_unlock(&read_file_pool->thread_count_lock);

    printf("read_file_pool:\n\t"
           "最高活跃: %d threads, 最低活跃：%d threads\n",
           max_read_file_thread_working, min_read_file_thread_working);

    pthread_mutex_lock(&send_message_pool->thread_count_lock);
    const int max_send_message_thread_working =
        send_message_pool->max_num_working;
    const int min_send_message_thread_working =
        send_message_pool->min_num_working;
    pthread_mutex_unlock(&send_message_pool->thread_count_lock);

    printf("send_message_pool:\n\t"
           "最高活跃: %d threads, 最低活跃：%d threads\n",
           max_send_message_thread_working, min_send_message_thread_working);

    pthread_mutex_lock(&free_memory_pool->thread_count_lock);
    const int max_free_memory_thread_working =
        free_memory_pool->max_num_working;
    const int min_free_memory_thread_working =
        free_memory_pool->min_num_working;
    pthread_mutex_unlock(&free_memory_pool->thread_count_lock);

    printf("free_memory_pool:\n\t"
           "最高活跃: %d threads, 最低活跃：%d threads\n",
           max_free_memory_thread_working, min_free_memory_thread_working);

    printf("缓存命中次数为: %lu\n", cache_hit_times);
    printf("缓存未命中次数为: %lu\n", cache_miss_times);
    printf("缓存命中率为: %%%.2f\n",
           100.0 * ((double)cache_hit_times /
                    (double)(cache_hit_times + cache_miss_times)));
  }

  return NULL;
}

// 线程活跃时间相加（会在 thread_do 中调用）
// source: given argument
// destination: thread_active_time
void thread_active_time_add(const struct timespec source) {
  if (sem_wait(thread_active_time_sempaphore) < 0) {
    perror("sem_wait");
    exit(EXIT_FAILURE);
  }
  thread_active_time = timer_add(thread_active_time, source);
  if (sem_post(thread_active_time_sempaphore) < 0) {
    perror("sem_post");
    exit(EXIT_FAILURE);
  }
}

// 线程阻塞时间相加（会在 thread_do 中调用)
// source: given argument
// destination: thread_block_time
void thread_block_time_add(const struct timespec source) {
  if (sem_wait(thread_block_time_sempaphore) < 0) {
    perror("sem_wait");
    exit(EXIT_FAILURE);
  }
  thread_block_time = timer_add(thread_block_time, source);
  if (sem_post(thread_block_time_sempaphore) < 0) {
    perror("sem_post");
    exit(EXIT_FAILURE);
  }
}
