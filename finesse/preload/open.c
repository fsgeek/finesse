/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <finesse.h>
#include "preload.h"


#if 0
int open(const char *pathname, int flags, ...)
{
    va_list args;
    mode_t mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    finesse_preload_init();

    fprintf(stderr, "finesse preload called for open\n");

    return finesse_open(pathname, flags, mode);
}
#endif // 

int creat(const char *pathname, mode_t mode) 
{
    finesse_preload_init();

    return finesse_creat(pathname, mode);
}

#if 0
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
#endif // 0

#if 0
FILE *fopen(const char *pathname, const char *mode)
{
    finesse_preload_init();

    return finesse_fopen(pathname, mode);
}
#endif // 0

FILE *fopen64(const char *pathname, const char *mode)
{
    finesse_preload_init();

    return finesse_fopen64(pathname, mode);
}

int open64(const char *pathname, int flags, ...)
{
    va_list args;
    mode_t mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    finesse_preload_init();

    fprintf(stderr, "finesse preload called for open\n");

    return finesse_open64(pathname, flags, mode);
}

int openat64(int dirfd, const char *pathname, int flags, ...)
{
    va_list args;
    mode_t mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    finesse_preload_init();

    return finesse_openat64(dirfd, pathname, flags, mode);
}
