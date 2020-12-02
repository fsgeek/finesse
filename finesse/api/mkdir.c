/*
 * (C) Copyright 2018-2020 Tony Mason
 * All Rights Reserved
 */

#include "api-internal.h"
#include "callstats.h"

static int fin_mkdir(const char *path, mode_t mode)
{
    typedef int (*orig_mkdir_t)(const char *path, mode_t mode);
    static orig_mkdir_t orig_mkdir = NULL;

    if (NULL == orig_mkdir) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_mkdir = (orig_mkdir_t)dlsym(RTLD_NEXT, "mkdir");
#pragma GCC diagnostic pop

        assert(NULL != orig_mkdir);
        if (NULL == orig_mkdir) {
            return EACCES;
        }
    }

    return orig_mkdir(path, mode);
}

static int fin_mkdirat(int fd, const char *path, mode_t mode)
{
    typedef int (*orig_mkdirat_t)(int fd, const char *path, mode_t mode);
    static orig_mkdirat_t orig_mkdirat = NULL;

    if (NULL == orig_mkdirat) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_mkdirat = (orig_mkdirat_t)dlsym(RTLD_NEXT, "mkdirat");
#pragma GCC diagnostic pop

        assert(NULL != orig_mkdirat);
        if (NULL == orig_mkdirat) {
            return EACCES;
        }
    }

    return orig_mkdirat(fd, path, mode);
}

static int internal_mkdir(const char *path, mode_t mode)
{
    // TODO: implement this as a Finesse call
    return fin_mkdir(path, mode);
}

int finesse_mkdir(const char *path, mode_t mode)
{
    int status = -1;

    status = internal_mkdir(path, mode);
    FinesseApiCountCall(FINESSE_API_CALL_MKDIR, (status >= 0));

    return status;
}

static int internal_mkdirat(int fd, const char *path, mode_t mode)
{
    // TODO: implement this as a Finesse call
    return fin_mkdirat(fd, path, mode);
}

int finesse_mkdirat(int fd, const char *path, mode_t mode)
{
    int status = -1;

    status = internal_mkdirat(fd, path, mode);
    FinesseApiCountCall(FINESSE_API_CALL_MKDIRAT, (status >= 0));

    return status;
}
