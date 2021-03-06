/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include "iowrapper.h"

int open(const char *pathname, int flags, ...)
{
    va_list args;
    mode_t  mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    // finesse_preload_init();

    return iowrap_open(pathname, flags, mode);
}

ssize_t read(int fd, void *buf, size_t count)
{
    return iowrap_read(fd, buf, count);
}

#if 0
ssize_t write(int fd, const void *buf, size_t count)
{
    return iowrap_write(fd, buf, count);
}
#endif  // 0

int close(int fd)
{
    return iowrap_close(fd);
}
