#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

#include "include/interrupt.h"
#include "include/timer.h"
#include "include/types.h"

void sig_handler_init(void) {
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = interrupt_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  // 开始捕捉信号
  sigaction(SIGINT, &sigIntHandler, NULL);
}

// Interrupt handler
void interrupt_handler(int signal) {
  printf("\b\bCaught interrupt signal: %d\n", signal);

  // 打印时间总和
  printf("共使用 %.2fms 成功处理 %ld 个客户端请求: \n",
         timespec_to_double_in_ms(*global_thread_timer), thread_count);
  printf("\t平均每个客户端完成请求处理时间为%.2fms\n",
         timespec_to_double_in_ms(*global_thread_timer) / thread_count);
  printf("\t平均每个客户端完成读socket时间为%.2fms\n",
         timespec_to_double_in_ms(*global_rsocket_timer) / thread_count);
  printf("\t平均每个客户端完成写socket时间为%.2fms\n",
         timespec_to_double_in_ms(*global_wsocket_timer) / thread_count);
  printf("\t平均每个客户端完成读网页数据时间为%.2fms\n",
         timespec_to_double_in_ms(*global_rfile_timer) / thread_count);
  printf("\t平均每个客户端完成写日志数据时间为%.2fms\n",
         timespec_to_double_in_ms(*global_logger_timer) / thread_count);
  // destroy and munmap semaphores
  printf("Destroying semaphores...\n");

  if (sem_destroy(logging_semaphore) < 0) {
    perror("sem_destroy failed");
    exit(EXIT_FAILURE);
  }

  if (sem_destroy(timer_semaphore) < 0) {
    perror("sem_destroy failed");
    exit(EXIT_FAILURE);
  }

  // free semaphores
  free(logging_semaphore);
  free(timer_semaphore);

  // free timers
  free(global_thread_timer);
  free(global_rsocket_timer);
  free(global_wsocket_timer);
  free(global_rfile_timer);
  free(global_logger_timer);

  printf("Exiting...\n");
  exit(EXIT_SUCCESS);
}