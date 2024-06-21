// Server Code

// webserver.c

// The following main code from https://github.com/ankushagarwal/nweb*, but they
// are modified slightly

// to use POSIX features
#include <stdbool.h>
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/pthreadtypes.h>
#include <glib.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "include/business.h"
#include "include/cache.h"
#include "include/interrupt.h"
#include "include/logger.h"
#include "include/memory.h"
#include "include/threadpool.h"
#include "include/timer.h"
#include "include/types.h"

// extensions
struct file_extension extensions[] = {
    {"gif", "image/gif"}, {"jpg", "image/jpg"},   {"jpeg", "image/jpeg"},
    {"png", "image/png"}, {"webp", "image/webp"}, {"ico", "image/ico"},
    {"zip", "image/zip"}, {"gz", "image/gz"},     {"tar", "image/tar"},
    {"htm", "text/html"}, {"html", "text/html"},  {"css", "text/css"},
    {NULL, NULL},
};

// semaphore definitions
sem_t *logging_semaphore = NULL;
sem_t *output_sempaphore = NULL;
sem_t *thread_active_time_sempaphore = NULL;
sem_t *thread_block_time_sempaphore = NULL;

// 线程池全局指针，分别代表三种不同的业务
threadpool *read_message_pool = NULL;
threadpool *read_file_pool = NULL;
threadpool *send_message_pool = NULL;
threadpool *free_memory_pool = NULL;

// 缓存用 hash 表
GHashTable *cache_hash_table = NULL;
// 缓存用 hash 表的锁
pthread_mutex_t *cache_hash_table_mutex = NULL;
// 缓存hit/miss次数, protected by cache_hash_table_mutex
unsigned long cache_hit_times = 0;
unsigned long cache_miss_times = 0;

#ifdef USE_LRU
// LRU 算法用到的树
GTree *LRU_tree;
// LRU 算法所用的 tree 的互斥锁
pthread_mutex_t *LRU_tree_mutex = NULL;
// 初始化 LRU Tree
void LRU_tree_init(void);
#endif

#ifdef USE_LFU
// LFU 算法用到的树
GTree *LFU_tree;
// LFU 算法所用的 tree 的互斥锁
pthread_mutex_t *LFU_tree_mutex = NULL;
// 初始化 LFU Tree
void LFU_tree_init(void);
#endif

#ifdef USE_POOL_ALLOC
// 使用线程池的时候, 任务队列中的任务数量
size_t global_task_count = 0;
sem_t *global_task_count_semaphore = NULL;

// hash表, 存放每个分配了内存的指针信息
GHashTable *memory_hash_table = NULL;

// 动态数组, 存放 pool 的指针
GHashTable *pools = NULL;

// 保护的 mutex
pthread_mutex_t *memory_info_mutex = NULL;
#endif

// 解析命令参数
void argument_check(int argc, char const *argv[]);

// 捕捉 Ctrl+C 信号的 sigIntHandler
void sig_handler_init(void);

// 用来初始化信号量的函数
sem_t *semaphore_allocate_init(void);

// 初始化 hash table
void cache_hash_table_init(void);

int main(int argc, char const *argv[]) {
  // 解析命令参数
  argument_check(argc, argv);

  // 建立服务端侦听 socket
  long listenfd;
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    logger(ERROR, "system call", "socket", 0);
    perror("socket error");
    exit(EXIT_FAILURE);
  }

  const int enable = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) <
      0) {
    perror("setsocket error");
    exit(EXIT_FAILURE);
  }

  const long port = atoi(argv[1]);
  if (port < 0 || port > 60000) {
    logger(ERROR, "Invalid port number (try 1->60000)", argv[1], 0);
    exit(EXIT_FAILURE);
  }

  static struct sockaddr_in serv_addr; // static = initialised to zeros
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);

  if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    logger(ERROR, "system call", "bind", 0);
    perror("bind error");
    exit(EXIT_FAILURE);
  }

  if (listen(listenfd, LISTENQ) < 0) {
    logger(ERROR, "system call", "listen", 0);
    perror("listen error");
    exit(EXIT_FAILURE);
  }

  static struct sockaddr_in cli_addr; // static = initialised to zeros
  socklen_t length = sizeof(cli_addr);

  // 捕捉 Ctrl+C 信号的 sigIntHandler
  sig_handler_init();

  // 初始化信号量
  logging_semaphore = semaphore_allocate_init();
  output_sempaphore = semaphore_allocate_init();
  thread_active_time_sempaphore = semaphore_allocate_init();
  thread_block_time_sempaphore = semaphore_allocate_init();

#ifdef USE_POOL_ALLOC

  global_task_count_semaphore = semaphore_allocate_init();
  // 初始化 pool 所用 hash 表
  memory_hash_table =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

  // 初始化 array
  pools = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

  // 初始化 mutex
  memory_info_mutex = (pthread_mutex_t *)calloc(1, sizeof(pthread_mutex_t));
  if (pthread_mutex_init(memory_info_mutex, NULL) != 0) {
    perror("pthread_mutex_init failed");
    exit(EXIT_FAILURE);
  }

#endif

  // 初始化3个线程池
  read_message_pool = init_thread_pool(NUM_THREADS);
  read_file_pool = init_thread_pool(NUM_THREADS);
  send_message_pool = init_thread_pool(NUM_THREADS);
  free_memory_pool = init_thread_pool(NUM_THREADS);

  // 初始化 cache_hash_table 及其互斥锁
  cache_hash_table_init();

  // 初始化 LRU/LFU Tree 及其互斥锁
#ifdef USE_LRU
  LRU_tree_init();
#endif

#ifdef USE_LFU
  LFU_tree_init();
#endif

  // 创建一个 monitor 线程来监控性能
  pthread_t monitor_thread;
  pthread_create(&monitor_thread, NULL, (void *)monitor, NULL);
  pthread_detach(monitor_thread);

  printf("%s\n", "Server running...waiting for connections.");

  for (long hit = 1;; hit++) {
    // Await a connection on socket FD.
    long socketfd;
    if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) <
        0) {
      logger(ERROR, "system call", "accept", 0);
      perror("accept error");
      exit(EXIT_FAILURE);
    }

    // 成功，放入线程池中
    // 需要给 thread runner 传递的参数
    struct read_message_args *const next_args =
        (struct read_message_args *)malloc(sizeof(*next_args));
    next_args->socketfd = socketfd;
    next_args->hit = hit;

    // 创建 task
    task *const new_task = (task *)malloc(sizeof(task));
    new_task->next = NULL;
    new_task->function = (void *)read_message;
    new_task->arg = next_args;

    // 将 task 放入 read_message 线程池中
    add_task_to_thread_pool(read_message_pool, new_task);
  }
}

void argument_check(int argc, char const *argv[]) {
  if (argc < 3 || argc > 3 || !strcmp(argv[1], "-?")) {
    printf(
        "hint: nweb Port-Number Top-Directory\t\tversion %d\n\n"
        "\tnweb is a small and very safe mini web server\n"
        "\tnweb only servers out file/web pages with extensions named below\n"
        "\t and only from the named directory or its sub-directories.\n"
        "\tThere is no fancy features = safe and secure.\n\n"
        "\tExample:webserver 8181 /home/nwebdir &\n\n"
        "\tOnly Supports:",
        VERSION);

    for (long i = 0; extensions[i].ext != 0; i++) {
      printf(" %s", extensions[i].ext);
    }

    printf(
        "\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
        "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
        "\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n");
    exit(EXIT_SUCCESS);
  }

  if (!strncmp(argv[2], "/", 2) || !strncmp(argv[2], "/etc", 5) ||
      !strncmp(argv[2], "/bin", 5) || !strncmp(argv[2], "/lib", 5) ||
      !strncmp(argv[2], "/tmp", 5) || !strncmp(argv[2], "/usr", 5) ||
      !strncmp(argv[2], "/dev", 5) || !strncmp(argv[2], "/sbin", 6)) {
    printf("ERROR: Bad top directory %s, see nweb -?\n", argv[2]);
    exit(EXIT_FAILURE);
  }

  if (chdir(argv[2]) == -1) {
    printf("ERROR: Can't Change to directory %s\n", argv[2]);
    exit(EXIT_FAILURE);
  }
}

sem_t *semaphore_allocate_init(void) {
  // place semaphore in shared memory
  sem_t *semaphore = (sem_t *)calloc(1, sizeof(*semaphore));

  // initialize semaphore
  if (sem_init(semaphore, 0, 1) < 0) {
    perror("sem_init failed");
    exit(EXIT_FAILURE);
  }

  // passing back
  return semaphore;
}

// 初始化 hash table
void cache_hash_table_init(void) {
  // 初始化缓存所用 hash 表
  cache_hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
                                           //  cached_file_handle_free);
                                           cached_file_handle_free);
  // 初始化 hash 表的锁
  cache_hash_table_mutex =
      (pthread_mutex_t *)calloc(1, sizeof(pthread_mutex_t));
  if (pthread_mutex_init(cache_hash_table_mutex, NULL) != 0) {
    perror("pthread_mutex_init failed");
    exit(EXIT_FAILURE);
  }
}

#ifdef USE_LRU
// 初始化 LRU Tree
void LRU_tree_init(void) {
  LRU_tree =
      g_tree_new_full(LRU_tree_node_cmp, NULL, LRU_tree_node_destroy, NULL);
  // 初始化LRU 算法所用的 tree 的互斥锁
  LRU_tree_mutex = (pthread_mutex_t *)calloc(1, sizeof(pthread_mutex_t));
  if (pthread_mutex_init(LRU_tree_mutex, NULL) != 0) {
    perror("pthread_mutex_init failed");
    exit(EXIT_FAILURE);
  }
}
#endif

#ifdef USE_LFU
// 初始化 LRU Tree
void LFU_tree_init(void) {
  LFU_tree =
      g_tree_new_full(LFU_tree_node_cmp, NULL, LFU_tree_node_destroy, NULL);
  // 初始化LRU 算法所用的 tree 的互斥锁
  LFU_tree_mutex = (pthread_mutex_t *)calloc(1, sizeof(pthread_mutex_t));
  if (pthread_mutex_init(LFU_tree_mutex, NULL) != 0) {
    perror("pthread_mutex_init failed");
    exit(EXIT_FAILURE);
  }
}
#endif