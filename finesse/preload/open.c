/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <finesse.h>
#include "preload.h"

int open(const char *pathname, int flags, ...)
{
    va_list args;
    mode_t mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    finesse_preload_init();

    return finesse_open(pathname, flags, mode);
}

int creat(const char *pathname, mode_t mode) 
{
    finesse_preload_init();

    return finesse_creat(pathname, mode);
}

int openat(int dirfd, const char *pathname, int flags, ...)
{
    va_list args;
    mode_t mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    finesse_preload_init();

    return finesse_openat(dirfd, pathname, flags, mode);
}


#if 0
int finesse_open(const char *pathname, int flags, ...);
int finesse_creat(const char *pathname, mode_t mode);
int finesse_openat(int dirfd, const char *pathname, int flags, ...);
int finesse_close(int fd);
int finesse_unlink(const char *pathname);
int finesse_unlinkat(int dirfd, const char *pathname, int flags);
int finesse_statvfs(const char *path, struct statvfs *buf);
int finesse_fstatvfs(int fd, struct statvfs *buf);
int finesse_fstatfs(int fd, struct statfs *buf);
int finesse_statfs(const char *path, struct statfs *buf);
#endif // 0
