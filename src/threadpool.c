#include <bits/time.h>
#include <glib.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>

#include "include/memory.h"
#include "include/threadpool.h"
#include "include/timer.h"
#include "include/types.h"

threadpool *init_thread_pool(int num_threads) {
  // 创建线程池空间
  threadpool *pool = (threadpool *)calloc(1, sizeof(threadpool));
  pool->num_threads = 0;
  pool->num_working = 0;
  pool->max_num_working = 0;
  pool->min_num_working = INT_MAX;
  pool->is_alive = true;

  // 初始化互斥量
  if (pthread_mutex_init(&pool->thread_count_lock, NULL) != 0) {
    perror("pthread_mutex_init init failed");
    exit(EXIT_FAILURE);
  }

  // 初始化条件变量属性
  pthread_condattr_t cattr;
  if (pthread_condattr_init(&cattr) != 0) {
    perror("pthread_condattr_init failed");
    exit(EXIT_FAILURE);
  }

  // 初始化条件变量
  if (pthread_cond_init(&pool->threads_all_idle, &cattr) != 0) {
    perror("pthread_cond_init failed");
    exit(EXIT_FAILURE);
  }
  // 初始化任务队列
  init_taskqueue(&pool->queue);

  // 初始化线程的 attribute
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) != 0) {
    perror("pthread_attr_init");
    exit(EXIT_FAILURE);
  }
  // 创建线程数组
  pool->threads = (thread **)calloc(num_threads, sizeof(thread *));
  // 创建线程
  for (int i = 0; i < num_threads; i++) {
    create_thread(pool, &pool->threads[i], i, attr);
  }

  // 等所有的线程创建完毕，在每个线程运行函数中将进行 pool->num_threads++操作
  // 因此，此处为忙等待，直到所有的线程创建完毕，并马上运行阻塞代码时才返回
  while (pool->num_threads != num_threads) {
    continue;
  }
  return pool;
}

// 向线程池中添加任务
void add_task_to_thread_pool(threadpool *pool, task *curtask) {
// 任务增加
#ifdef USE_POOL_ALLOC
  if (sem_wait(global_task_count_semaphore) < 0) {
    perror("sem_wait error");
    exit(EXIT_FAILURE);
  }

  global_task_count++;

  if (sem_post(global_task_count_semaphore) < 0) {
    perror("sem_wait error");
    exit(EXIT_FAILURE);
  }
#endif
  push_taskqueue(&pool->queue, curtask);
}

// Wait until all working and all blocking task finished
// 等待当前任务全部运行完
void wait_thread_pool(threadpool *pool) {
  pthread_mutex_lock(&pool->queue.has_jobs->mutex);
  pool->queue.has_jobs->status = false;
  pthread_cond_broadcast(&pool->queue.has_jobs->cond);
  pthread_mutex_unlock(&pool->queue.has_jobs->mutex);

  pthread_mutex_lock(&pool->thread_count_lock);
  while (pool->queue.length || pool->num_working) {
    // 该条件获得信号之前，该函数一直被阻塞。该函数会在被阻塞之前以原子方式释放相关的互斥锁
    // 并在返回之前以原子方式再次获取该互斥锁
    // note:
    // 所以，相应的，子线程需要在 pool->queue.length 或者 pool->num_working
    // 变化的时候进行 signal操作
    pthread_cond_wait(&pool->threads_all_idle, &pool->thread_count_lock);
  }
  pthread_mutex_unlock(&pool->thread_count_lock);
}

// 获取线程池中线程数量
int get_num_of_thread_working(threadpool *pool) { return pool->num_working; }

// 创建线程
int create_thread(threadpool *pool, thread **pthread, int id,
                  pthread_attr_t attr) {
  // 为 pthread 分配内存空间
  if ((*pthread = (thread *)calloc(1, sizeof(thread))) == 0) {
    perror("calloc");
    return -1;
  }
  // 设置这个 thread 的属性
  (*pthread)->pool = pool;
  (*pthread)->id = id;
  // 创建线程
  pthread_create(&(*pthread)->pthread, &attr, (void *)thread_do, *pthread);
  pthread_detach((*pthread)->pthread);
  return 0;
}

// 销毁线程池
void destroy_thread_pool(threadpool *pool) {
  // 线程池不存活，不再接受任务
  pool->is_alive = false;

  // 等待结束
  // wait_thread_pool(pool);

  // 销毁任务队列
  destroy_taskqueue(&pool->queue);

  // 销毁线程指针数组，并释放所有为线程池分配的内存
  // 销毁线程指针
  // 这里不用互斥锁读取 num_thread 是因为所有的子线程全部都退出了
  for (int i = 0; i < pool->num_threads; i++) {
    free(pool->threads[i]);
  }
  // 销毁线程数组
  free(pool->threads);

  // 销毁线程池本身
  free(pool);
}

// 线程运行的逻辑函数
void *thread_do(thread *pthread) {
  // 设置线程名字
  char thread_name[128] = {0};
  sprintf(thread_name, "thread_pool-%d", pthread->id);
  prctl(PR_SET_NAME, thread_name);
  printf("%s\n", thread_name);

  // 获得线程池
  threadpool *pool = pthread->pool;

  // 在线程池初始化时，用于已经创建线程的计数，执行 pool->num_threads++;
  pthread_mutex_lock(&pool->thread_count_lock);
  pool->num_threads++;
  pthread_mutex_unlock(&pool->thread_count_lock);

  // 线程一直循环往复运行，直到 pool->is_alive 变为 false
  while (pool->is_alive) {
    // 如果任务队列中还有任务，则继续运行，否则阻塞
    pthread_mutex_lock(&pool->queue.has_jobs->mutex);
    // 没任务的时候一直阻塞

    // 阻塞开始时间：
    struct timespec block_start_t;
    clock_gettime(CLOCK_REALTIME, &block_start_t);

    while (pool->queue.has_jobs->status == false) {
      // note: 这个 cond 要配合 pool->queue.has_jobs->status 的变化来使用
      // printf("%s: waiting...\n", thread_name);
      pthread_cond_wait(&pool->queue.has_jobs->cond,
                        &pool->queue.has_jobs->mutex);
      // printf("%s: received signal...\n", thread_name);
    }

    // 阻塞结束时间：
    struct timespec block_end_t;
    clock_gettime(CLOCK_REALTIME, &block_end_t);
    // 时间差
    const struct timespec block_diff = timer_diff(block_start_t, block_end_t);
    thread_block_time_add(block_diff);

    pthread_mutex_unlock(&pool->queue.has_jobs->mutex);

    // 线程开始活跃，计算活跃时间
    // 活跃开始时间：
    struct timespec active_start_t;
    clock_gettime(CLOCK_REALTIME, &active_start_t);

    // printf("%s: running...\n", thread_name);

    if (pool->is_alive == false) {
      break;
    }

    // 执行到此位置，表明线程在工作，需要对工作线程数量进行计数
    pthread_mutex_lock(&pool->thread_count_lock);
    // 修改之前，确定最大和最小线程工作数量
    pool->max_num_working = MAX(pool->max_num_working, pool->num_working);
    pool->min_num_working = MIN(pool->min_num_working, pool->num_working);
    // 然后再执行：
    pool->num_working++;
    pthread_mutex_unlock(&pool->thread_count_lock);

    // take_taskqueue 从任务队列头部提取任务，并在队列中删除此任务
    // printf("%s: take_taskqueue\n", thread_name);
    task *const current_task = take_taskqueue(&pool->queue);
    if (current_task == NULL) {
      // printf("%s: Found a null task!\n", thread_name);
      // 执行到此位置，表明线程已经将任务执行完成，需更改工作线程数量
      pthread_mutex_lock(&pool->thread_count_lock);
      // 修改之前，确定最大和最小线程工作数量
      pool->max_num_working = MAX(pool->max_num_working, pool->num_working);
      pool->min_num_working = MIN(pool->min_num_working, pool->num_working);
      pool->num_working--;
      // 此处还需注意，当工作线程数量为 0 ，表示任务全部完成，要让阻塞在
      // wait_thread_pool() 函数上的线程继续运行
      if (pool->num_working == 0) {
        pthread_cond_signal(&pool->threads_all_idle);
      }
      pthread_mutex_unlock(&pool->thread_count_lock);
      continue;
    }

    // 修改存在的任务数量
#ifdef USE_POOL_ALLOC
    if (sem_wait(global_task_count_semaphore) < 0) {
      perror("sem_wait error");
      exit(EXIT_FAILURE);
    }

    global_task_count--;

    if (sem_post(global_task_count_semaphore) < 0) {
      perror("sem_wait error");
      exit(EXIT_FAILURE);
    }
#endif

    // 从任务队列的队首提取任务并执行
    void *(*const function)(void *) = current_task->function;
    void *const arg = current_task->arg;
    if (function == NULL) {
      // 执行到此位置，表明线程已经将任务执行完成，需更改工作线程数量
      pthread_mutex_lock(&pool->thread_count_lock);
      // 修改之前，确定最大和最小线程工作数量
      pool->max_num_working = MAX(pool->max_num_working, pool->num_working);
      pool->min_num_working = MIN(pool->min_num_working, pool->num_working);
      pool->num_working--;
      // 此处还需注意，当工作线程数量为 0 ，表示任务全部完成，要让阻塞在
      // wait_thread_pool() 函数上的线程继续运行
      if (pool->num_working == 0) {
        pthread_cond_signal(&pool->threads_all_idle);
      }
      pthread_mutex_unlock(&pool->thread_count_lock);

      free(current_task);
      continue;
    }
    // printf("%s: function: %p, (arg: %p)\n", thread_name, function, arg);
    function(arg);
    // free task
    free(current_task);

    // 执行到此位置，表明线程已经将任务执行完成，需更改工作线程数量
    pthread_mutex_lock(&pool->thread_count_lock);
    // 修改之前，确定最大和最小线程工作数量
    pool->max_num_working = MAX(pool->max_num_working, pool->num_working);
    pool->min_num_working = MIN(pool->min_num_working, pool->num_working);
    pool->num_working--;
    // 此处还需注意，当工作线程数量为 0 ，表示任务全部完成，要让阻塞在
    // wait_thread_pool() 函数上的线程继续运行
    if (pool->num_working == 0) {
      pthread_cond_signal(&pool->threads_all_idle);
    }
    pthread_mutex_unlock(&pool->thread_count_lock);

    // 线程活跃结束
    // 活跃结束时间：
    struct timespec active_end_t;
    clock_gettime(CLOCK_REALTIME, &active_end_t);
    // 时间差
    const struct timespec active_diff =
        timer_diff(active_start_t, active_end_t);
    thread_active_time_add(active_diff);
  }
  // 运行到此位置表明，线程将要退出，需更改当前线程池中的线程数量
  pthread_mutex_lock(&pool->thread_count_lock);
  pool->num_threads--;
  pthread_mutex_unlock(&pool->thread_count_lock);

  pthread_exit(NULL);
}

// task queue operations
// 初始化任务队列
void init_taskqueue(taskqueue *queue) {
  // 将所有内存置 0
  memset(queue, 0, sizeof(*queue));

  // 初始化 mutex
  if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
    perror("pthread_mutex_init init failed");
    exit(EXIT_FAILURE);
  }

  // 分配 dummy head
  queue->front = (task *)calloc(1, sizeof(task));
  queue->rear = (task *)calloc(1, sizeof(task));

  queue->front->next = queue->rear;
  queue->rear->next = queue->front;

  // 分配 has_jobs
  // ** 初始化互斥锁之前，必须将其所在的内存清零 **
  queue->has_jobs = (staconv *)calloc(1, sizeof(staconv));
  // 初始化 mutex
  if (pthread_mutex_init(&queue->has_jobs->mutex, NULL) != 0) {
    perror("pthread_mutex_init init failed");
    exit(EXIT_FAILURE);
  }
  // 初始化用于阻塞和唤醒线程池中线程的 cond:
  // 1. 初始化条件变量属性
  pthread_condattr_t cattr;
  if (pthread_condattr_init(&cattr) != 0) {
    perror("pthread_condattr_init failed");
    exit(EXIT_FAILURE);
  }
  // 2. 初始化条件变量
  if (pthread_cond_init(&queue->has_jobs->cond, &cattr) != 0) {
    perror("pthread_cond_init failed");
    exit(EXIT_FAILURE);
  }

  // status 初始化为无任务
  queue->has_jobs->status = false;

  // length 初始化为 0
  queue->length = 0;
}

// 销毁任务队列
void destroy_taskqueue(taskqueue *queue) {
  // 删除 has_jobs
  // 1. 释放mutex和cond
  // if (pthread_mutex_destroy(&queue->has_jobs->mutex) != 0) {
  //   perror("pthread_mutex_destroy");
  //   exit(EXIT_FAILURE);
  // }
  // if (pthread_cond_destroy(&queue->has_jobs->cond) != 0) {
  //   perror("pthread_cond_destroy");
  //   exit(EXIT_FAILURE);
  // }
  // 2. 释放内存
  free(queue->has_jobs);

  // 删除 front 和 rear 的两个 dummyhead
  free(queue->front);
  free(queue->rear);

  // 释放 mutex
  if (pthread_mutex_destroy(&queue->mutex) != 0) {
    perror("pthread_mutex_destroy");
    exit(EXIT_FAILURE);
  }
}

// 将任务加入队列
void push_taskqueue(taskqueue *queue, task *curtask) {
  // 需要 lock 的参与
  pthread_mutex_lock(&queue->mutex);
  // 将参数的任务放入链表中的队列尾部
  // 将最后一个任务的 next 指向新任务
  queue->rear->next->next = curtask;
  // 将 rear 指向 新任务
  queue->rear->next = curtask;
  // 将新任务的 next 指向 rear
  curtask->next = queue->rear;

  // length++
  // printf("length (== %d)++ \n", queue->length);
  queue->length++;

  pthread_mutex_lock(&queue->has_jobs->mutex);
  // 同时，将 has_jobs 的 status 设置为 true，进行 signal 操作
  if (queue->has_jobs->status == false) {
    queue->has_jobs->status = true;
    pthread_cond_signal(&queue->has_jobs->cond);
  }
  pthread_mutex_unlock(&queue->has_jobs->mutex);

  // 释放 lock
  pthread_mutex_unlock(&queue->mutex);
}

// take_taskqueue 从任务队列头部提取任务，并在队列中删除此任务
task *take_taskqueue(taskqueue *queue) {
  // 需要 lock 的参与
  pthread_mutex_lock(&queue->mutex);
  // 可能会有因为调度问题产生的 queue length 为 0 的情况
  // 具体是因为：当前线程由于调度问题有一段时间没有获取队列头的任务
  // 在这期间，其他线程跳过了条件检查（state == true）
  // 从而“帮忙”当前线程完成了任务，而当前线程仍然试图获取队列头任务
  // 如果队列长度为 0，则必定报错
  // 所以，增加该条件以解决该问题
  if (queue->length == 0) {
    pthread_mutex_unlock(&queue->mutex);
    return NULL;
  }

  // 将参数的任务从链表中的队列头部取出
  // printf("queue task:%d\n", queue->length);
  task *const to_fetch_task = queue->front->next;
  // 将 dummyhead 的下一个设置为 to_fetch_task 的下一个
  if (queue->front == NULL) {
    // printf("dereference a nullptr queue->front\n");
    exit(EXIT_FAILURE);
  }
  if (to_fetch_task == NULL) {
    // printf("dereference a nullptr to_fetch_task\n");
    exit(EXIT_FAILURE);
  }
  queue->front->next = to_fetch_task->next;
  if (queue->length == 1) {
    queue->rear->next = queue->front;
  }
  // length--
  // printf("length (== %d)-- \n", queue->length);
  queue->length--;

  // 释放 lock
  pthread_mutex_unlock(&queue->mutex);

  // 由于length减少，如果 length 降为 0，那么 queue->has_jobs->status = false;
  // printf("pthread_mutex_lock(&queue->has_jobs->mutex)\n");
  pthread_mutex_lock(&queue->has_jobs->mutex);
  // printf("pthread_mutex_lock(&queue->has_jobs->mutex) ends.\n");
  if (queue->length == 0) {
    queue->has_jobs->status = false;
  }
  // printf("pthread_mutex_unlock(&queue->has_jobs->mutex)\n");
  pthread_mutex_unlock(&queue->has_jobs->mutex);
  // printf("pthread_mutex_unlock(&queue->has_jobs->mutex) ends.\n");

  // 返回 task
  return to_fetch_task;
}
