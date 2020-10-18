/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
 */

#include "api-internal.h"
#include "callstats.h"

static int fin_chdir(const char *pathname)
{
    typedef int (*orig_chdir_t)(const char *pathname);
    static orig_chdir_t orig_chdir = NULL;

    if (NULL == orig_chdir) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_chdir = (orig_chdir_t)dlsym(RTLD_NEXT, "chdir");
#pragma GCC diagnostic pop

        assert(NULL != orig_chdir);
        if (NULL == orig_chdir) {
            return ENOSYS;
        }
    }

    return orig_chdir(pathname);
}

static int internal_chdir(const char *pathname)
{
    int status;
    DECLARE_TIME(FINESSE_API_CALL_CHDIR)

    START_TIME

    status = fin_chdir(pathname);

    STOP_NATIVE_TIME

    return status;
}

int finesse_chdir(const char *pathname)
{
    int status = internal_chdir(pathname);

    FinesseApiCountCall(FINESSE_API_CALL_CHDIR, 0 == status);

    return status;
}

// TODO: fchdir
