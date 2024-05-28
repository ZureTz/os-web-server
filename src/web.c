#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "logger.h"
#include "types.h"
#include "web.h"

// 此函数完成了 WebServer
// 主要功能，它首先解析客户端发送的消息，然后从中获取客户端请求的文
// 件名，然后根据文件名从本地将此文件读入缓存，并生成相应的 HTTP
// 响应消息；最后通过服务器与客户端的 socket 通道向客户端返回 HTTP 响应消息

void web(int fd, int hit) {
  // 计时器起点
  // struct timespec start_t;
  // clock_gettime(CLOCK_REALTIME, &start_t);

  static char buffer[BUFSIZE + 1]; // 设置静态缓冲区

  const int socket_read_ret =
      read(fd, buffer, BUFSIZE); // 从连接通道中读取客户端的请求消息
  if (socket_read_ret == 0 || socket_read_ret == -1) {
    // 如果读取客户端消息失败，则向客户端发送 HTTP 失败响应信息
    logger(FORBIDDEN, "failed to read browser request", "", fd);
    // sleep(1); // sleep 的作用是防止消息未发出，已经将此 socket 通道关闭
    close(fd);
    return;
  }

  if (socket_read_ret > 0 && socket_read_ret < BUFSIZE) {
    // 设置有效字符串，即将字符串尾部表示为 0
    buffer[socket_read_ret] = 0;
  } else {
    buffer[0] = 0;
  }

  for (long i = 0; i < socket_read_ret; i++) {
    // 移除消息字符串中的“CF”和“LF”字符
    if (buffer[i] == '\r' || buffer[i] == '\n') {
      buffer[i] = '*';
    }
  }

  logger(LOG, "request", buffer, hit);

  // 判断客户端 HTTP 请求消息是否为 GET 类型，如果不是则给出相应的响应消息

  if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
    logger(FORBIDDEN, "Only simple GET operation supported", buffer, fd);
    // sleep(1); // sleep 的作用是防止消息未发出，已经将此 socket 通道关闭
    close(fd);
    return;
  }

  int buflen = 0;
  for (long i = 4; i < BUFSIZE; i++) {
    // null terminate after the second space to ignore extra stuff
    if (buffer[i] == ' ') { // string is "GET URL " + lots of other stuff
      buffer[i] = 0;
      // set length of the buffer to i
      buflen = i;
      break;
    }
  }

  for (long j = 0; j < buflen - 1; j++) {
    // 在消息中检测路径，不允许路径中出现 '.'
    if (buffer[j] == '.' && buffer[j + 1] == '.') {
      logger(FORBIDDEN, "Parent directory (..) path names not supported",
             buffer, fd);
      // sleep(1); // sleep 的作用是防止消息未发出，已经将此 socket 通道关闭
      close(fd);
      return;
    }
  }
  if (!strncmp(&buffer[0], "GET /\0", 6) ||
      !strncmp(&buffer[0], "get /\0", 6)) {
    // 如果请求消息中没有包含有效的文件名，则使用默认的文件名 index.html
    strcpy(buffer, "GET /index.html");
  }

  // 根据预定义在 extensions 中的文件类型，检查请求的文件类型是否本服务器支持

  buflen = strlen(buffer);
  const char *fstr = NULL;

  for (long i = 0; extensions[i].ext != 0; i++) {
    long len = strlen(extensions[i].ext);
    if (!strncmp(&buffer[buflen - len], extensions[i].ext, len)) {
      fstr = extensions[i].filetype;
      break;
    }
  }

  if (fstr == NULL) {
    logger(FORBIDDEN, "file extension type not supported", buffer, fd);
    // sleep(1); // sleep 的作用是防止消息未发出，已经将此 socket 通道关闭
    close(fd);
    return;
  }

  int file_fd = -1;
  if ((file_fd = open(&buffer[5], O_RDONLY)) == -1) { // 打开指定的文件名
    logger(NOTFOUND, "failed to open file", &buffer[5], fd);
    // sleep(1); // sleep 的作用是防止消息未发出，已经将此 socket 通道关闭
    close(fd);
    return;
  }

  logger(LOG, "SEND", &buffer[5], hit);

  off_t len = lseek(file_fd, (off_t)0, SEEK_END); // 通过 lseek 获取文件长度
  lseek(file_fd, (off_t)0, SEEK_SET); // 将文件指针移到文件首位置

  sprintf(buffer,
          "HTTP/1.1 200 OK\n"
          "Server: nweb/%d.0\n"
          "Content-Length: %ld\n"
          "Connection: close\n"
          "Content-Type: %s",
          VERSION, len, fstr); // Header without a blank line

  logger(LOG, "Header", buffer, hit);

  sprintf(buffer,
          "HTTP/1.1 200 OK\n"
          "Server: nweb/%d.0\n"
          "Content-Length: %ld\n"
          "Connection: close\n"
          "Content-Type: %s\n\n",
          VERSION, len, fstr); // Header + a blank line
  write(fd, buffer, strlen(buffer));

  // 不停地从文件里读取文件内容，并通过 socket 通道向客户端返回文件内容
  int file_read_ret;
  while ((file_read_ret = read(file_fd, buffer, BUFSIZE)) > 0) {
    write(fd, buffer, file_read_ret);
  }

  if (file_read_ret < 0) {
    printf("Read Error\n");
    // sleep(1); // sleep 的作用是防止消息未发出，已经将此 socket 通道关闭
    close(file_fd);
    close(fd);
    return;
  }

  // 
  close(file_fd);
  close(fd);

  // 计时器终点
  // struct timespec end_t;
  // clock_gettime(CLOCK_REALTIME, &end_t);
  // struct timespec diff = timer_diff(start_t, end_t);
  // printf("From PID %d, the cost of the web() is: %lds, %ldns\n", getpid(),
  //        diff.tv_sec, diff.tv_nsec);
}
