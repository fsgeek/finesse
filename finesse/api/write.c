/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
 */

#include "api-internal.h"
#include "callstats.h"

static int fin_write(int fd, void *buffer, size_t length)
{
    typedef int (*orig_write_t)(int fd, void *buffer, size_t length);
    static orig_write_t orig_write = NULL;

    if (NULL == orig_write) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_write = (orig_write_t)dlsym(RTLD_NEXT, "write");
#pragma GCC diagnostic pop
    }

    assert(NULL != orig_write);
    if (NULL == orig_write) {
        errno = ENOSYS;
        return -1;
    }

    return orig_write(fd, buffer, length);
}

static int internal_write(int fd, void *buffer, size_t length)
{
    int status;

    DECLARE_TIME(FINESSE_API_CALL_WRITE)

    START_TIME
    status = fin_write(fd, buffer, length);
    STOP_NATIVE_TIME;

    return status;
}

int finesse_write(int fd, void *buffer, size_t length);

int finesse_write(int fd, void *buffer, size_t length)
{
    int status = internal_write(fd, buffer, length);

    FinesseApiCountCall(FINESSE_API_CALL_WRITE, !(status < 0));  // number of bytes, -1 on error
    return status;
}
