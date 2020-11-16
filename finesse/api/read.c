/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
 */

#include "api-internal.h"
#include "callstats.h"

static int fin_read(int fd, void *buffer, size_t length)
{
    typedef int (*orig_read_t)(int fd, void *buffer, size_t length);
    static orig_read_t orig_read = NULL;

    if (NULL == orig_read) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_read = (orig_read_t)dlsym(RTLD_NEXT, "read");
#pragma GCC diagnostic pop
    }

    assert(NULL != orig_read);
    if (NULL == orig_read) {
        errno = ENOSYS;
        return -1;
    }

    return orig_read(fd, buffer, length);
}

static int internal_read(int fd, void *buffer, size_t length)
{
    int status;

    DECLARE_TIME(FINESSE_API_CALL_READ)

    START_TIME
    status = fin_read(fd, buffer, length);
    STOP_NATIVE_TIME;

    return status;
}

int finesse_read(int fd, void *buffer, size_t length);

int finesse_read(int fd, void *buffer, size_t length)
{
    int status = internal_read(fd, buffer, length);

    FinesseApiCountCall(FINESSE_API_CALL_READ, !(status < 0));  // number of bytes, -1 on error
    return status;
}
