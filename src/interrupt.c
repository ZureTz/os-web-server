#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "interrupt.h"
#include "types.h"

// Interrupt handler
void interrupt_handler(int signal) {
  printf("\b\bCaught interrupt signal: %d\n", signal);

  // 打印时间总和
  printf("The sum of the cost by all child proccesses is: %lds, %ldns\n",
         global_timer->tv_sec, global_timer->tv_nsec);

  // destroy semaphores
  printf("Destroying semaphores...\n");
  if (sem_destroy(logging_semaphore) < 0) {
    perror("sem_destroy failed");
    exit(EXIT_FAILURE);
  }

  if (munmap(logging_semaphore, sizeof(*logging_semaphore)) < 0) {
    perror("munmap failed");
    exit(EXIT_FAILURE);
  }
  if (munmap(timer_semaphore, sizeof(*timer_semaphore)) < 0) {
    perror("munmap failed");
    exit(EXIT_FAILURE);
  }

  printf("Exiting...\n");
  exit(EXIT_SUCCESS);
}