// Server Code

// webserver.c

// The following main code from https://github.com/ankushagarwal/nweb*, but they
// are modified slightly

// to use POSIX features
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "include/interrupt.h"
#include "include/logger.h"
#include "include/timer.h"
#include "include/types.h"
#include "include/web.h"

// extensions
struct file_extension extensions[] = {
    {"gif", "image/gif"},  {"jpg", "image/jpg"}, {"jpeg", "image/jpeg"},
    {"png", "image/png"},  {"ico", "image/ico"}, {"zip", "image/zip"},
    {"gz", "image/gz"},    {"tar", "image/tar"}, {"htm", "text/html"},
    {"html", "text/html"}, {NULL, NULL},
};

// semaphore init
sem_t *logging_semaphore = NULL;
sem_t *timer_semaphore = NULL;

// global timer pointer
struct timespec *global_process_timer = NULL;
struct timespec *global_rsocket_timer = NULL;
struct timespec *global_wsocket_timer = NULL;
struct timespec *global_rfile_timer = NULL;
struct timespec *global_logger_timer = NULL;

// 子进程数初始化为 0
long process_count = 0;

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
  timer_semaphore = semaphore_allocate_init();

  // 全局计时器初始化
  global_process_timer = timer_init();
  global_rsocket_timer = timer_init();
  global_wsocket_timer = timer_init();
  global_rfile_timer = timer_init();
  global_logger_timer = timer_init();

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

  for (long hit = 1;; hit++) {
    // Await a connection on socket FD.
    long socketfd;
    if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) <
        0) {
      logger(ERROR, "system call", "accept", 0);
      perror("accept error");
      exit(EXIT_FAILURE);
    }

    // Fork一个子进程执行web响应操作
    const pid_t child_pid = fork();

    if (child_pid < 0) {
      // child_pid < 0, fork操作失败，退出
      perror("fork failed");
      exit(EXIT_FAILURE);
    }

    // 父进程 pid > 0
    if (child_pid > 0) {
      printf("PID %d: Sucessfully forked a child (PID = %d)\n", getpid(),
             child_pid);
      // 成功 fork，子进程的数量增加
      process_count++;
      // 关闭 socket
      close(socketfd);
      // 父进程继续接受请求
      continue;
    }

    // 子进程 child_pid == 0

    // 子进程计时开始
    struct timespec start_t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_t);

    // close listening socket
    close(listenfd);

    // 回应请求
    web(socketfd, hit); // never returns

    // 子进程计时器结束
    struct timespec end_t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_t);
    const struct timespec diff = timer_diff(start_t, end_t);

    // Wait for semaphore
    if (sem_wait(timer_semaphore) < 0) {
      perror("sem_wait error");
      exit(EXIT_FAILURE);
    }

    // ENTERING CRITICAL SECTION

    // 计时器加上 diff
    *global_process_timer = timer_add(*global_process_timer, diff);

    // CRITICAL SECTION ENDS

    // release semaphore
    if (sem_post(timer_semaphore) < 0) {
      perror("sem_post error");
      exit(EXIT_FAILURE);
    }

    // 退出
    printf("Child process (PID %d) exits successfully\n", getpid());
    exit(EXIT_SUCCESS);
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

void sig_handler_init(void) {
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = interrupt_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  // 开始捕捉信号
  sigaction(SIGINT, &sigIntHandler, NULL);
}

sem_t *semaphore_allocate_init(void) {
  // place semaphore in shared memory
  sem_t *semaphore = mmap(NULL, sizeof(*semaphore), PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (semaphore == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  // initialize semaphore
  if (sem_init(semaphore, 1, 1) < 0) {
    perror("sem_init");
    exit(EXIT_FAILURE);
  }

  // passing back
  return semaphore;
}

struct timespec *timer_init(void) {
  struct timespec *result =
      mmap(NULL, sizeof(*global_process_timer), PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  result->tv_sec = 0;
  result->tv_nsec = 0;

  return result;
}