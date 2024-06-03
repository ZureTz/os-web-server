#pragma once
#ifndef WEB_H
#define WEB_H

#include <stddef.h>
#include <sys/types.h>

void web(int fd, int hit);
ssize_t read_with_clocking(struct timespec *file_read_sum, int fd, void *buf,
                           size_t nbytes);

#endif