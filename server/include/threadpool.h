#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <bits/pthreadtypes.h>
#include <stdbool.h>

/* Queue status and conditional variable*/
typedef struct staconv {
  pthread_mutex_t mutex;
  pthread_cond_t cond; // 用于阻塞和唤醒线程池中线程

  // 表示任务队列状态：false 为无任务；true 为有任务
  bool status;
} staconv;

/* Task */
typedef struct task {
  struct task *next;            // 指向下一任务的指针
  void *(*function)(void *arg); // 函数指针
  void *arg;                    // 函数参数指针
} task;

/* Task Queue */
typedef struct taskqueue {
  pthread_mutex_t mutex; // 用于互斥读/写任务队列
  task *front;           //指向队首
  task *rear;            //指向队尾

  staconv *has_jobs; //根据状态，阻塞线程
  int length;        //队列中任务个数
} taskqueue;

/* Thread Pool */
typedef struct threadpool {
  // 线程指针数组
  // an array of pointer to a struct thread
  struct thread **threads;

  volatile int num_threads;          // 线程池中线程数量 (静态)
  volatile int num_working;          // 目前正在工作的线程个数
  volatile int max_num_working;      // 最大工作线程数量
  volatile int min_num_working;      // 最小工作线程数量
  pthread_mutex_t thread_count_lock; // 线程池锁, 用于修改上面两个变量

  pthread_cond_t threads_all_idle; // 用于销毁线程的条件变量

  taskqueue queue; // 等待队列

  volatile bool is_alive; // 表示线程池是否还存活

} threadpool;

/* Thread */
typedef struct thread {
  int id;            // identifiers
  pthread_t pthread; // 封装的 POSIX 线程
  threadpool *pool;  // 与线程池绑定
} thread;

// constructor and destructor of the thread pool
// 线程池初始化函数
threadpool *init_thread_pool(int num_threads);
// 向线程池中添加任务
void add_task_to_thread_pool(struct threadpool *pool, struct task *curtask);
// 等待当前任务全部运行完
void wait_thread_pool(struct threadpool *pool);
// 获取线程池中线程数量
int get_num_of_thread_working(struct threadpool *pool);
// 创建线程
int create_thread(struct threadpool *pool, struct thread **pthread, int id,
                  pthread_attr_t attr);
// 销毁线程池
void destroy_thread_pool(struct threadpool *pool);
// 线程运行的逻辑函数
void *thread_do(struct thread *pthread);

// task queue operations
// 初始化任务队列
void init_taskqueue(struct taskqueue *queue);
// 销毁任务队列
void destroy_taskqueue(struct taskqueue *queue);
// 将任务加入队列
void push_taskqueue(struct taskqueue *queue, struct task *curtask);
// take_taskqueue 从任务队列头部提取任务，并在队列中删除此任务
task *take_taskqueue(struct taskqueue *queue);

// 创建的 thread pool 指针
extern threadpool *read_message_pool;
extern threadpool *read_file_pool;
extern threadpool *send_message_pool;
extern threadpool *free_memory_pool;

#endif