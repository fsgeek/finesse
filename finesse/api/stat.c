/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
 */


#include "api-internal.h"

// int stat(const char *file_name, struct stat *buf);

static int fin_stat(const char *file_name, struct stat *buf)
{
    typedef int (*orig_stat_t)(const char *file_name, struct stat *buf);
    static orig_stat_t orig_stat = NULL;

    if (NULL == orig_stat) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_stat = (orig_stat_t)dlsym(RTLD_NEXT, "stat");
#pragma GCC diagnostic pop

        assert(NULL != orig_stat);
        if (NULL == orig_stat) {
            return ENOSYS;
        }
    }

    return orig_stat(file_name, buf);
}

int finesse_stat(const char *file_name, struct stat *buf)
{
    int status;
    fincomm_message message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int result;
    double timeout = 0;

    finesse_client_handle = finesse_check_prefix(file_name);

    if (NULL == finesse_client_handle) {
        // not of interest - fallback
        return fin_stat(file_name, buf);
    }

    status = FinesseSendStatRequest(finesse_client_handle, file_name, &message);
    assert(0 == status);
    status = FinesseGetStatResponse(finesse_client_handle, message, buf, &timeout, &result);
    assert(0 == status);
    FinesseFreeStatResponse(finesse_client_handle, message);

    return result;
}

static int fin_fstat(int filedes, struct stat *buf)
{
    typedef int (*orig_fstat_t)(int filedes, struct stat *buf);
    static orig_fstat_t orig_fstat = NULL;

    if (NULL == orig_fstat) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_fstat = (orig_fstat_t)dlsym(RTLD_NEXT, "fstat");
#pragma GCC diagnostic pop

        assert(NULL != orig_fstat);
        if (NULL == orig_fstat) {
            return ENOSYS;
        }
    }

    return orig_fstat(filedes, buf);

}

int finesse_fstat(int filedes, struct stat *buf)
{
    int status;
    fincomm_message message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int result;
    double timeout = 0;
    finesse_file_state_t *ffs = NULL;

        // Let's see if we know about this file descriptor
    ffs = finesse_lookup_file_state(filedes);
    if (NULL == ffs) {
        // We aren't tracking this, so we do pass-through
        return fin_fstat(filedes, buf);
    }

    // We ARE tracking the file, so we can use the key to query.
    status = FinesseSendFstatRequest(finesse_client_handle, &ffs->key, &message);
    assert(0 == status);
    status = FinesseGetStatResponse(finesse_client_handle, message, buf, &timeout, &result);
    assert(0 == status);
    FinesseFreeStatResponse(finesse_client_handle, message);

    return result;

}

static int fin_lstat(const char *file_name, struct stat *buf)
{
    typedef int (*orig_lstat_t)(const char *file_name, struct stat *buf);
    static orig_lstat_t orig_lstat = NULL;

    if (NULL == orig_lstat) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_lstat = (orig_lstat_t)dlsym(RTLD_NEXT, "lstat");
#pragma GCC diagnostic pop

        assert(NULL != orig_lstat);
        if (NULL == orig_lstat) {
            return ENOSYS;
        }
    }

    return orig_lstat(file_name, buf);
}


int finesse_lstat(const char *pathname, struct stat *statbuf)
{
    int status;
    fincomm_message message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int result;
    double timeout = 0;

    finesse_client_handle = finesse_check_prefix(pathname);

    if (NULL == finesse_client_handle) {
        // not of interest - fallback
        return fin_lstat(pathname, statbuf);
    }

    status = FinesseSendLstatRequest(finesse_client_handle, pathname, &message);
    assert(0 == status);
    status = FinesseGetStatResponse(finesse_client_handle, message, statbuf, &timeout, &result);
    assert(0 == status);
    FinesseFreeStatResponse(finesse_client_handle, message);

    return result;
}

static int fin_fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags)
{
    typedef int (*orig_fstatat_t)(int dirfd, const char *pathname, struct stat *statbuf, int flags);
    static orig_fstatat_t orig_fstatat = NULL;

    if (NULL == orig_fstatat) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_fstatat = (orig_fstatat_t)dlsym(RTLD_NEXT, "fstatat");
#pragma GCC diagnostic pop

        assert(NULL != orig_fstatat);
        if (NULL == orig_fstatat) {
            return ENOSYS;
        }
    }

    return orig_fstatat(dirfd, pathname, statbuf, flags);
}


int finesse_fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags)
{
    int status;
    fincomm_message message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int result;
    double timeout = 0;
    finesse_file_state_t *ffs = NULL;

        // Let's see if we know about this file descriptor
    ffs = finesse_lookup_file_state(dirfd);
    if (NULL == ffs) {
        // We aren't tracking this, so we do pass-through
        return fin_fstatat(dirfd, pathname, statbuf, flags);
    }

    // We ARE tracking the file, so we can use the key to query.
    status = FinesseSendFstatAtRquest(finesse_client_handle, &ffs->key, pathname, flags, &message);
    assert(0 == status);
    status = FinesseGetStatResponse(finesse_client_handle, message, statbuf, &timeout, &result);
    assert(0 == status);
    FinesseFreeStatResponse(finesse_client_handle, message);

    return result;

}
