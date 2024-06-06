// Server Code

// webserver.c

// The following main code from https://github.com/ankushagarwal/nweb*, but they
// are modified slightly

// to use POSIX features
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
#include "include/interrupt.h"
#include "include/logger.h"
#include "include/threadpool.h"
#include "include/types.h"

// extensions
struct file_extension extensions[] = {
    {"gif", "image/gif"},  {"jpg", "image/jpg"}, {"jpeg", "image/jpeg"},
    {"png", "image/png"},  {"ico", "image/ico"}, {"zip", "image/zip"},
    {"gz", "image/gz"},    {"tar", "image/tar"}, {"htm", "text/html"},
    {"html", "text/html"}, {NULL, NULL},
};

// semaphore init
sem_t *logging_semaphore = NULL;
sem_t *output_sempaphore = NULL;

// 线程池全局指针，分别代表三种不同的业务
threadpool *read_message_pool = NULL;
threadpool *read_file_pool = NULL;
threadpool *send_message_pool = NULL;

// 解析命令参数
void argument_check(int argc, char const *argv[]);

// 捕捉 Ctrl+C 信号的 sigIntHandler
void sig_handler_init(void);

// 用来初始化信号量的函数
sem_t *semaphore_allocate_init(void);

// 全局计时器初始化
struct timespec *timer_init(void);

int main(int argc, char const *argv[]) {
  // 解析命令参数
  argument_check(argc, argv);

  // 捕捉 Ctrl+C 信号的 sigIntHandler
  sig_handler_init();

  // 初始化信号量
  logging_semaphore = semaphore_allocate_init();
  output_sempaphore = semaphore_allocate_init();

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

  printf("%s\n", "Server running...waiting for connections.");

  static struct sockaddr_in cli_addr; // static = initialised to zeros
  socklen_t length = sizeof(cli_addr);

  // 初始化3个线程池
  read_message_pool = init_thread_pool(NUM_THREADS);
  read_file_pool = init_thread_pool(NUM_THREADS);
  send_message_pool = init_thread_pool(NUM_THREADS);

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