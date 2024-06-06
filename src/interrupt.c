#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

#include "include/interrupt.h"
#include "include/threadpool.h"
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

  // 销毁线程池
  destroy_thread_pool(read_message_pool);

  printf("Destroying semaphores...\n");

  if (sem_destroy(logging_semaphore) < 0) {
    perror("sem_destroy failed");
    exit(EXIT_FAILURE);
  }

  // free semaphores
  free(logging_semaphore);

  printf("Exiting...\n");
  exit(EXIT_SUCCESS);
}