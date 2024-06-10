#include <glib.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

#include "include/cache.h"
#include "include/interrupt.h"
#include "include/threadpool.h"
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

  // 释放 hash 表 和树的资源
  g_hash_table_destroy(cache_hash_table);
  pthread_mutex_destroy(cache_hash_table_mutex);
  g_tree_destroy(LRU_tree);
  pthread_mutex_destroy(LRU_tree_mutex);

  // 销毁线程池
  destroy_thread_pool(read_message_pool);
  destroy_thread_pool(read_file_pool);
  destroy_thread_pool(send_message_pool);
  destroy_thread_pool(free_memory_pool);

  // release semaphores

  if (sem_destroy(logging_semaphore) < 0) {
    perror("sem_destroy failed");
    exit(EXIT_FAILURE);
  }

  if (sem_destroy(output_sempaphore) < 0) {
    perror("sem_destroy failed");
    exit(EXIT_FAILURE);
  }

  if (sem_destroy(thread_active_time_sempaphore) < 0) {
    perror("sem_destroy failed");
    exit(EXIT_FAILURE);
  }

  if (sem_destroy(thread_block_time_sempaphore) < 0) {
    perror("sem_destroy failed");
    exit(EXIT_FAILURE);
  }

  // free semaphores
  free(logging_semaphore);
  free(output_sempaphore);
  free(thread_active_time_sempaphore);
  free(thread_block_time_sempaphore);

  exit(EXIT_SUCCESS);
}