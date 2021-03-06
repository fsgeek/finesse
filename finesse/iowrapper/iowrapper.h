/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

/*
 * This is a simple wrapper for converting I/O operations into copy operations from
 * a memory mapped file.
 *
 */

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

ssize_t write(int fd, const void *buf, size_t count);
ssize_t iowrap_read(int fd, void *buf, size_t count);
int     iowrap_open(const char *pathname, int flags, ...);
int     iowrap_close(int fd);
