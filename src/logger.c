#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "include/logger.h"
#include "include/types.h"

// 日志函数，将运行过程中的提示信息记录到 webserver.log 文件中

void logger(const int type, const char *s1, const char *s2,
            const int socket_fd) {

  char timebuffer[BUFSIZE * 2];
  char logbuffer[BUFSIZE * 2];

  // 在 LOG 的每一行都增加日期和时间信息
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  sprintf(timebuffer, "[%d-%02d-%02d %02d:%02d:%02d] ", tm.tm_year + 1900,
          tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

  // 根据消息类型，将消息放入 logbuffer 缓存，或直接将消息通过 socket
  // 通道返回给客户端
  switch (type) {
  case ERROR:
    sprintf(logbuffer, "ERROR: %s:%s Errno=%d exiting pid=%d", s1, s2, errno,
            getpid());
    break;

  case FORBIDDEN:
    write(socket_fd,
          "HTTP/1.1 403 Forbidden\n"
          "Content-Length: 193\n"
          "Connection:close\n"
          "Content-Type: text/html\n\n"
          "<html>"
          "<head>\n"
          "<title>403 Forbidden</title>\n"
          "</head>"
          "<body>\n"
          "<h1>Forbidden</h1>\n"
          "<p>The requested URL, file "
          "type or operation is not allowed on this simple static file "
          "webserver.</p>\n"
          "</body>"
          "</html>\n",
          278);
    sprintf(logbuffer, "FORBIDDEN: %s:%s", s1, s2);
    break;

  case NOTFOUND:
    write(socket_fd,
          " HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: "
          "close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not "
          "Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL "
          "was not found on this server.\n</body></html>\n",
          224);
    sprintf(logbuffer, "NOT FOUND: %s:%s", s1, s2);

    break;

  case LOG:
    sprintf(logbuffer, "INFO: %s:%s:%d", s1, s2, socket_fd);
    break;
  }

  // Wait for semaphore
  if (sem_wait(logging_semaphore) < 0) {
    perror("sem_wait error");
    exit(EXIT_FAILURE);
  }

  // ENTERING CRITICAL SECTION

  // 将 logbuffer 缓存中的消息存入 webserver.log 文件
  int fd = -1;
  if ((fd = open("log/webserver.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >=
      0) {
    write(fd, timebuffer, strlen(timebuffer));
    write(fd, logbuffer, strlen(logbuffer));
    write(fd, "\n", 1);
    close(fd);
  }

  // CRITICAL SECTION ENDS

  // release semaphore
  if (sem_post(logging_semaphore) < 0) {
    perror("sem_post error");
    exit(EXIT_FAILURE);
  }
}
