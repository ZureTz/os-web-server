# 实验2

## 题目1

> 题目 1：使用fork函数，设计并实现WebServer以支持多进程并发处理众多客户端的请求。

由于之前的单进程服务器在遇到并发的请求的时候会出现响应慢的问题，因此改用多进程的服务器。如果使用多进程，就能同时处理多个请求，提高响应的速度。

对于WebServer，主要是使用fork函数在父进程中接受连接请求，在子进程中处理请求并响应。每当有一个客户端连接到服务器时，就会fork一个子进程来处理该请求，这样可以支持多个客户端并发请求处理。在子进程中处理完请求后，会计算处理时间，并更新全局计时器。

```c
int main(int argc, char const *argv[]) {
  // 前面的代码保持不变
  // ...
  for (long hit = 1;; hit++) {
    static struct sockaddr_in cli_addr; // static = initialised to zeros
    socklen_t length = sizeof(cli_addr);
    // Await a connection on socket FD.
    long socketfd;
    if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) <
        0) {
      logger(ERROR, "system call", "accept", 0);
    }

    // Fork一个子进程执行web响应操作
    pid_t child_pid = fork();

    if (child_pid < 0) {
      // child_pid < 0, fork操作失败，退出
      perror("fork failed");
      exit(EXIT_FAILURE);
    }

    if (child_pid > 0) {
      // child_pid > 0, 父进程继续接受请求
      printf("PID %d: Sucessfully forked a child(PID = %d)\n", getpid(),
             child_pid);
      continue;
    }

    // 子进程 child_pid == 0

    // close listening socket
    close(listenfd);

    // 回应请求
    web(socketfd, hit); // never read_returns

    // 子进程退出
    printf("Child process (PID %d) exits successfully\n", getpid());
    exit(EXIT_SUCCESS);
  }
}

```

关键操作包括：

- 接受客户端连接请求：使用accept函数接受客户端连接请求，得到socket描述符。
- Fork子进程：使用fork函数创建子进程，父进程继续等待新的连接请求，子进程处理当前请求。

运行结果如下图所示：

![web1](web1.png)

![step1](1.png)

- 实验运行结果显示，每当有一个客户端连接到服务器，就会创建一个子进程来处理请求，并且在命令行中打印出子进程的PID。子进程能够独立处理请求，并在处理完成后退出。在子进程退出后，会显示该PID已经成功退出。

- 通过子进程处理请求，即使不断刷新网页，也不会出现如之前单进程的web服务器一样卡顿的现象，可以同时处理多个请求，提高了服务器的并发性能。

## 题目2

> 题目 2：使用信号量、共享内存等系统接口函数，来统计每个子进程的消耗时间以及所有子进程消耗时间之和。

1. 定义两个信号量，一个用作logger记录，一个用作timer计时

```c
// in types.h
// semaphores
extern sem_t *logging_semaphore;
extern sem_t *timer_semaphore;
```

2. 在 `main` 中初始化这两个信号量

```c
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

// semaphore definition
sem_t *logging_semaphore = NULL;
sem_t *timer_semaphore = NULL;

int main(int argc, char const *argv[]) {
  // ...
    
  // 初始化信号量
  logging_semaphore = semaphore_allocate_init();
  timer_semaphore = semaphore_allocate_init();
    
  // ...
    
}
```

3. 在fork的子进程中增加计时的代码

```c
int main(int argc, char const *argv[]) {
  
   // ...
    
  for (long hit = 1;; hit++) {
    // Fork一个子进程执行web响应操作
    const pid_t child_pid = fork();

    // ... 父进程执行代码
	// ...
    // ...
      
    // 子进程 child_pid == 0

    // 子进程计时开始
    struct timespec start_t;
    clock_gettime(CLOCK_REALTIME, &start_t);

    // close listening socket
    close(listenfd);

    // 回应请求
    web(socketfd, hit); // never returns

    // 子进程计时器结束
    struct timespec end_t;
    clock_gettime(CLOCK_REALTIME, &end_t);
    struct timespec diff = timer_diff(start_t, end_t);

    // Wait for semaphore
    if (sem_wait(timer_semaphore) < 0) {
      perror("sem_wait error");
      exit(EXIT_FAILURE);
    }

    // ENTERING CRITICAL SECTION

    // 计时器加上 diff
    *global_timer = timer_add(*global_timer, diff);

    // CRITICAL SECTION ENDS

    // release semaphore
    if (sem_post(timer_semaphore) < 0) {
      perror("sem_post error");
      exit(EXIT_FAILURE);
    }

    // 输出子进程的时间
    printf("The cost of PID %d is: %lds, %ldns\n", getpid(), diff.tv_sec,
           diff.tv_nsec);
    // 退出
    printf("Child process (PID %d) exits successfully\n", getpid());
    exit(EXIT_SUCCESS);
  }
```

这样，就用信号量完整实现了计时的功能。

运行程序并刷新网页，可以得到：

![3](3.png)

## 题目3

> 题目 3：使用 http_load 来测试当前设计的多进程 WebServer 服务性能， 根据测试结果来分析其比单进程Web服务性能提高的原因。同时结合题目2，来分析当前多进程 WebServer 的性能瓶颈在何处？是否还能够继续提高此WebServer服务的性能？

如图所示，使用 `http_load` 进行测试，用最高10次并发请求 `webserver` 5秒。

![image-20240528223911975](2.png)

运行结果如下图所示，发现在请求比较多的时候，连接被终止了。

![image-20240528224133520](4.png)

查看 `webserver` 的输出，发现：程序在请求过多的时候退出，原因是 fork 失败了。

![image-20240528224415026](5.png)

这在代码中对应在 `webserver` 接受了请求并 fork 自己的那一段：

```c
    // Fork一个子进程执行web响应操作
    const pid_t child_pid = fork();

    if (child_pid < 0) {
      // child_pid < 0, fork操作失败，退出
      perror("fork failed");
      exit(EXIT_FAILURE);
    }

    if (child_pid > 0) {
      // child_pid > 0, 父进程继续接受请求
      printf("PID %d: Sucessfully forked a child (PID = %d)\n", getpid(),
             child_pid);
      continue;
    }
   
    // 子进程 child_pid == 0
	// ...

```

可以看出来，多进程 `webserver` 的性能瓶颈就在当请求过多过密集的时候，由于过多过频繁的 fork，系统于是拒绝分配更多的资源。由于 fork 的数量没有限制， 多进程 `webserver` 倾向于无限制地请求系统资源。当系统不再为多进程 `webserver` 分配资源的时候，服务器便不能正常工作，这不仅是服务器的性能瓶颈，而且是一个潜在的问题。

### 与之前测试结果的对比

之前的测试结果为：

```bash
20 fetches, 5 max parallel, 6200 bytes, in 20.0041 seconds
310 mean bytes/connection
0.999797 fetches/sec, 309.937 bytes/sec
msecs/connect: 0.5565 mean, 0.867 max, 0.217 min
msecs/first-response: 4262.02 mean, 5019.46 max, 4.481 min
HTTP response codes:
  code 200 -- 20
```

而对比之前的测试结果：

![image-20240528224133520](4.png)

可以发现相应的次数提升了不少，在请求刚开始并发的时候能够妥善处理。

但是经过一段时间后，服务器完全拒绝了连接，这是由于服务器fork进程失败而退出了。

所以，需要继续提高性能，让服务器不再受到系统资源的影响。考虑通过线程解决系统因为 fork 太频繁而不分配资源的问题。

## 附录: `webserver.c` 的完整代码

```c
// Server Code

// webserver.c

// The following main code from https://github.com/ankushagarwal/nweb*, but they
// are modified slightly

// to use POSIX features
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <arpa/inet.h>
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

#include "interrupt.h"
#include "logger.h"
#include "timer.h"
#include "types.h"
#include "web.h"

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
struct timespec *global_timer = NULL;

// 解析命令参数
void argument_check(int argc, char const *argv[]);

// 捕捉 Ctrl+C 信号的 sigIntHandler
void sig_handler_init(void);

// 用来初始化信号量的函数
sem_t *semaphore_allocate_init(void);

int main(int argc, char const *argv[]) {
  // 解析命令参数
  argument_check(argc, argv);

  // 捕捉 Ctrl+C 信号的 sigIntHandler
  sig_handler_init();

  // 初始化信号量
  logging_semaphore = semaphore_allocate_init();
  timer_semaphore = semaphore_allocate_init();

  // 全局计时器初始化
  global_timer = mmap(NULL, sizeof(*global_timer), PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  global_timer->tv_sec = 0;
  global_timer->tv_nsec = 0;

  // 建立服务端侦听 socket
  long listenfd;

  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    logger(ERROR, "system call", "socket", 0);
    perror("socket error");
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

    if (child_pid > 0) {
      // child_pid > 0, 父进程继续接受请求
      printf("PID %d: Sucessfully forked a child (PID = %d)\n", getpid(),
             child_pid);
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
    struct timespec diff = timer_diff(start_t, end_t);

    // Wait for semaphore
    if (sem_wait(timer_semaphore) < 0) {
      perror("sem_wait error");
      exit(EXIT_FAILURE);
    }

    // ENTERING CRITICAL SECTION

    // 计时器加上 diff
    *global_timer = timer_add(*global_timer, diff);

    // CRITICAL SECTION ENDS

    // release semaphore
    if (sem_post(timer_semaphore) < 0) {
      perror("sem_post error");
      exit(EXIT_FAILURE);
    }

    // 输出子进程的时间
    printf("The cost of PID %d is: %lds, %ldns\n", getpid(), diff.tv_sec,
           diff.tv_nsec);
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
```

