/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif  // _GNU_SOURCE

#define _FCNTL_H 1

//#include <finesse.h>
//#include "preload.h"
//#include "libc-symbols.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int   finesse_open(const char *pathname, int flags, ...);
int   finesse_creat(const char *pathname, mode_t mode);
int   finesse_openat(int dirfd, const char *pathname, int flags, ...);
FILE *finesse_fopen(const char *pathname, const char *mode);
FILE *finesse_fdopen(int fd, const char *mode);
FILE *finesse_freopen(const char *pathname, const char *mode, FILE *stream);

int   open(const char *pathname, int flags, ...);
int   creat(const char *pathname, mode_t mode);
int   openat(int dirfd, const char *pathname, int flags, ...);
FILE *fopen(const char *pathname, const char *mode);
FILE *fdopen(int fd, const char *mode);
FILE *freopen(const char *pathname, const char *mode, FILE *stream);

int open(const char *pathname, int flags, ...)
{
    va_list args;
    mode_t  mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    // finesse_preload_init();

    return finesse_open(pathname, flags, mode);
}

int creat(const char *pathname, mode_t mode)
{
    return finesse_creat(pathname, mode);
}

int openat(int dirfd, const char *pathname, int flags, ...)
{
    va_list args;
    mode_t  mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    return finesse_openat(dirfd, pathname, flags, mode);
}

FILE *fopen(const char *pathname, const char *mode)
{
    return finesse_fopen(pathname, mode);
}

FILE *fdopen(int fd, const char *mode)
{
    return finesse_fdopen(fd, mode);
}

FILE *freopen(const char *pathname, const char *mode, FILE *stream)
{
    return finesse_freopen(pathname, mode, stream);
}
