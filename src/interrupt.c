#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

#include "include/interrupt.h"
#include "include/timer.h"
#include "include/types.h"

// Interrupt handler
void interrupt_handler(int signal) {
  printf("\b\bCaught interrupt signal: %d\n", signal);

  // 打印时间总和
  printf("共使用 %.2fms 成功处理 %ld 个客户端请求: \n",
         timespec_to_double_in_ms(*global_process_timer), process_count);
  printf("\t平均每个客户端完成请求处理时间为%.2fms\n",
         timespec_to_double_in_ms(*global_process_timer) / process_count);
  printf("\t平均每个客户端完成读socket时间为%.2fms\n",
         timespec_to_double_in_ms(*global_rsocket_timer) / process_count);
  printf("\t平均每个客户端完成写socket时间为%.2fms\n",
         timespec_to_double_in_ms(*global_wsocket_timer) / process_count);
  printf("\t平均每个客户端完成读网页数据时间为%.2fms\n",
         timespec_to_double_in_ms(*global_rfile_timer) / process_count);
  printf("\t平均每个客户端完成写日志数据时间为%.2fms\n",
         timespec_to_double_in_ms(*global_logger_timer) / process_count);

  // destroy and munmap semaphores
  printf("Destroying semaphores...\n");

  if (sem_destroy(logging_semaphore) < 0) {
    perror("sem_destroy failed");
    exit(EXIT_FAILURE);
  }

  if (munmap(logging_semaphore, sizeof(*logging_semaphore)) < 0) {
    perror("munmap failed");
    exit(EXIT_FAILURE);
  }

  if (sem_destroy(timer_semaphore) < 0) {
    perror("sem_destroy failed");
    exit(EXIT_FAILURE);
  }

  if (munmap(timer_semaphore, sizeof(*timer_semaphore)) < 0) {
    perror("munmap failed");
    exit(EXIT_FAILURE);
  }

  if (munmap(global_process_timer, sizeof(*global_process_timer)) < 0) {
    perror("munmap failed");
    exit(EXIT_FAILURE);
  }

  if (munmap(global_wsocket_timer, sizeof(*global_wsocket_timer)) < 0) {
    perror("munmap failed");
    exit(EXIT_FAILURE);
  }

  if (munmap(global_rsocket_timer, sizeof(*global_rsocket_timer)) < 0) {
    perror("munmap failed");
    exit(EXIT_FAILURE);
  }

  if (munmap(global_rfile_timer, sizeof(*global_rfile_timer)) < 0) {
    perror("munmap failed");
    exit(EXIT_FAILURE);
  }

  if (munmap(global_logger_timer, sizeof(*global_logger_timer)) < 0) {
    perror("munmap failed");
    exit(EXIT_FAILURE);
  }

  printf("Exiting...\n");
  exit(EXIT_SUCCESS);
}