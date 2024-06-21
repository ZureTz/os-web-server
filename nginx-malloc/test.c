// to use POSIX features
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <bits/time.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "ngx_palloc.h"
#include "timer.h"

const int MALLOC_FREE_TIMES = 300000;
const int SMALL_MEM_SIZE = 1, LARGE_MEM_SIZE = 10000;

int main(void) {
  ngx_pool_t *const pool = ngx_create_pool(25 * sizeof(int), NULL);
  printf("%-15s %-15s %-15s %-15s %-15s %-15s\n", "allocSmallMem",
         "freeSmallMem", "totalSmallMem", "allocLargeMem", "freeLargeMem",
         "totalLargeMem");

  struct timespec small_mem_alloc_total_time, small_mem_free_total_time;
  struct timespec large_mem_alloc_total_time, large_mem_free_total_time;

  small_mem_alloc_total_time = small_mem_free_total_time =
      large_mem_alloc_total_time = large_mem_free_total_time =
          (struct timespec){
              .tv_sec = 0,
              .tv_nsec = 0,
          };

  for (int i = 0; i < MALLOC_FREE_TIMES; i++) {
    struct timespec start, end;

    clock_gettime(CLOCK_REALTIME, &start);
    int *const ngx_small =
        (int *)ngx_palloc(pool, SMALL_MEM_SIZE * sizeof(int));
    clock_gettime(CLOCK_REALTIME, &end);

    small_mem_alloc_total_time =
        timer_add(small_mem_alloc_total_time, timer_diff(start, end));

    clock_gettime(CLOCK_REALTIME, &start);
    ngx_pfree(pool, ngx_small);
    clock_gettime(CLOCK_REALTIME, &end);

    small_mem_free_total_time =
        timer_add(small_mem_free_total_time, timer_diff(start, end));

    clock_gettime(CLOCK_REALTIME, &start);
    int *const ngx_large =
        (int *)ngx_palloc(pool, LARGE_MEM_SIZE * sizeof(int));
    clock_gettime(CLOCK_REALTIME, &end);

    large_mem_alloc_total_time =
        timer_add(large_mem_alloc_total_time, timer_diff(start, end));

    clock_gettime(CLOCK_REALTIME, &start);
    ngx_pfree(pool, ngx_large);
    clock_gettime(CLOCK_REALTIME, &end);

    large_mem_free_total_time =
        timer_add(large_mem_free_total_time, timer_diff(start, end));
  }
  ngx_destroy_pool(pool);

  printf("%-15.2f %-15.2f %-15.2f %-15.2f %-15.2f %-15.2f\n",
         timespec_to_double_in_ms(small_mem_alloc_total_time),
         timespec_to_double_in_ms(small_mem_free_total_time),
         timespec_to_double_in_ms(
             timer_add(small_mem_alloc_total_time, small_mem_free_total_time)),
         timespec_to_double_in_ms(large_mem_alloc_total_time),
         timespec_to_double_in_ms(large_mem_free_total_time),
         timespec_to_double_in_ms(
             timer_add(large_mem_alloc_total_time, large_mem_free_total_time)));

  small_mem_alloc_total_time = small_mem_free_total_time =
      large_mem_alloc_total_time = large_mem_free_total_time =
          (struct timespec){
              .tv_sec = 0,
              .tv_nsec = 0,
          };

  for (int i = 0; i < MALLOC_FREE_TIMES; i++) {
    struct timespec start, end;

    clock_gettime(CLOCK_REALTIME, &start);
    int *const os_small = (int *)malloc(SMALL_MEM_SIZE * sizeof(int));
    clock_gettime(CLOCK_REALTIME, &end);

    small_mem_alloc_total_time =
        timer_add(small_mem_alloc_total_time, timer_diff(start, end));

    clock_gettime(CLOCK_REALTIME, &start);
    free(os_small);
    clock_gettime(CLOCK_REALTIME, &end);

    small_mem_free_total_time =
        timer_add(small_mem_free_total_time, timer_diff(start, end));

    clock_gettime(CLOCK_REALTIME, &start);
    int *const os_large = malloc(LARGE_MEM_SIZE * sizeof(int));
    clock_gettime(CLOCK_REALTIME, &end);

    large_mem_alloc_total_time =
        timer_add(large_mem_alloc_total_time, timer_diff(start, end));

    clock_gettime(CLOCK_REALTIME, &start);
    free(os_large);
    clock_gettime(CLOCK_REALTIME, &end);

    large_mem_free_total_time =
        timer_add(large_mem_free_total_time, timer_diff(start, end));
  }
  printf("%-15.2f %-15.2f %-15.2f %-15.2f %-15.2f %-15.2f\n",
         timespec_to_double_in_ms(small_mem_alloc_total_time),
         timespec_to_double_in_ms(small_mem_free_total_time),
         timespec_to_double_in_ms(
             timer_add(small_mem_alloc_total_time, small_mem_free_total_time)),
         timespec_to_double_in_ms(large_mem_alloc_total_time),
         timespec_to_double_in_ms(large_mem_free_total_time),
         timespec_to_double_in_ms(
             timer_add(large_mem_alloc_total_time, large_mem_free_total_time)));

  return 0;
}
