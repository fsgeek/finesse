/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
 */

#include "api-internal.h"
#include "callstats.h"

// int stat(const char *file_name, struct stat *buf);

static int fin_stat(const char *file_name, struct stat *buf)
{
    typedef int (*orig_stat_t)(const char *file_name, struct stat *buf);
    static orig_stat_t orig_stat = NULL;
    typedef int (*orig_xstat_t)(int ver, const char *file_name, struct stat *buf);
    static orig_xstat_t orig_xstat = NULL;

    if (NULL == orig_stat) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_stat = (orig_stat_t)dlsym(RTLD_NEXT, "stat");
        if (NULL == orig_stat) {
            orig_xstat = (orig_xstat_t)dlsym(RTLD_NEXT, "__xstat");  // ubuntu 20.04 uses glibc 2.31, which uses statx
        }
#pragma GCC diagnostic pop
    }

    if (orig_xstat) {
        return orig_xstat(_STAT_VER, file_name, buf);
    }

    if (orig_stat) {
        return orig_stat(file_name, buf);
    }

    errno = ENOSYS;
    return -1;
}

static int internal_stat(const char *file_name, struct stat *buf)
{
    fincomm_message         message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int                     result;
    double                  timeout = 0;
    struct timespec         start, stop, elapsed;
    int                     status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    finesse_client_handle = finesse_check_prefix(file_name);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_STAT, &elapsed);

    if (NULL == finesse_client_handle) {
        // not of interest - fallback
        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        assert(0 == tstatus);

        status = fin_stat(file_name, buf);

        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        assert(0 == tstatus);
        timespec_diff(&start, &stop, &elapsed);
        FinesseApiRecordNative(FINESSE_API_CALL_STAT, &elapsed);

        return status;
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    status = FinesseSendStatRequest(finesse_client_handle, file_name, &message);
    assert(0 == status);
    status = FinesseGetStatResponse(finesse_client_handle, message, buf, &timeout, &result);
    assert(0 == status);
    FinesseFreeStatResponse(finesse_client_handle, message);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_STAT, &elapsed);

    if (result != 0) {
        fprintf(stderr, "%s:%d failed with result %d for file %s\n", __func__, __LINE__, result, file_name);
    }

    return result;
}

int finesse_stat(const char *file_name, struct stat *buf)
{
    int result = internal_stat(file_name, buf);

    FinesseApiCountCall(FINESSE_API_CALL_STAT, 0 == result);
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

static int internal_fstat(int filedes, struct stat *buf)
{
    struct timespec         start, stop, elapsed;
    int                     status, tstatus;
    fincomm_message         message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int                     result;
    double                  timeout = 0;
    finesse_file_state_t *  ffs     = NULL;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    // Let's see if we know about this file descriptor
    ffs = finesse_lookup_file_state(filedes);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_FSTAT, &elapsed);

    if (NULL == ffs) {
        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        assert(0 == tstatus);

        // We aren't tracking this, so we do pass-through
        status = fin_fstat(filedes, buf);

        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        assert(0 == tstatus);
        timespec_diff(&start, &stop, &elapsed);
        FinesseApiRecordNative(FINESSE_API_CALL_FSTAT, &elapsed);
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    // We ARE tracking the file, so we can use the key to query.
    status = FinesseSendFstatRequest(finesse_client_handle, &ffs->key, &message);
    assert(0 == status);
    status = FinesseGetStatResponse(finesse_client_handle, message, buf, &timeout, &result);
    assert(0 == status);
    FinesseFreeStatResponse(finesse_client_handle, message);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_FSTAT, &elapsed);

    return result;
}

int finesse_fstat(int filedes, struct stat *buf)
{
    int result = internal_fstat(filedes, buf);

    FinesseApiCountCall(FINESSE_API_CALL_FSTAT, 0 == result);

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

static int internal_lstat(const char *pathname, struct stat *statbuf)
{
    struct timespec         start, stop, elapsed;
    int                     status, tstatus;
    fincomm_message         message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int                     result;
    double                  timeout = 0;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    finesse_client_handle = finesse_check_prefix(pathname);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_LSTAT, &elapsed);

    if (NULL == finesse_client_handle) {
        // not of interest - fallback
        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        assert(0 == tstatus);

        status = fin_lstat(pathname, statbuf);

        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        assert(0 == tstatus);
        timespec_diff(&start, &stop, &elapsed);
        FinesseApiRecordNative(FINESSE_API_CALL_LSTAT, &elapsed);
        return status;
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    status = FinesseSendLstatRequest(finesse_client_handle, pathname, &message);
    assert(0 == status);
    status = FinesseGetStatResponse(finesse_client_handle, message, statbuf, &timeout, &result);
    assert(0 == status);
    FinesseFreeStatResponse(finesse_client_handle, message);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_LSTAT, &elapsed);

    return result;
}

int finesse_lstat(const char *pathname, struct stat *statbuf)
{
    int result = internal_lstat(pathname, statbuf);

    FinesseApiCountCall(FINESSE_API_CALL_LSTAT, 0 == result);

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

static int internal_fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags)
{
    struct timespec         start, stop, elapsed;
    int                     status, tstatus;
    fincomm_message         message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int                     result;
    double                  timeout = 0;
    finesse_file_state_t *  ffs     = NULL;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    // Let's see if we know about this file descriptor
    ffs = finesse_lookup_file_state(dirfd);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_FSTATAT, &elapsed);

    if (NULL == ffs) {
        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        assert(0 == tstatus);

        // We aren't tracking this, so we do pass-through
        status = fin_fstatat(dirfd, pathname, statbuf, flags);

        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        assert(0 == tstatus);
        timespec_diff(&start, &stop, &elapsed);
        FinesseApiRecordNative(FINESSE_API_CALL_FSTAT, &elapsed);

        return status;
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    // We ARE tracking the file, so we can use the key to query.
    status = FinesseSendFstatAtRquest(finesse_client_handle, &ffs->key, pathname, flags, &message);
    assert(0 == status);
    status = FinesseGetStatResponse(finesse_client_handle, message, statbuf, &timeout, &result);
    assert(0 == status);
    FinesseFreeStatResponse(finesse_client_handle, message);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_FSTATAT, &elapsed);

    return result;
}

int finesse_fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags)
{
    int result = internal_fstatat(dirfd, pathname, statbuf, flags);

    FinesseApiCountCall(FINESSE_API_CALL_FSTATAT, 0 == result);

    return result;
}

static int fin_statx(int dfd, const char *filename, unsigned atflag, unsigned mask, struct statx *buffer)
{
    typedef int (*orig_statx_t)(int dfd, const char *file_name, unsigned atflag, unsigned mask, struct statx *buf);
    static orig_statx_t orig_statx = NULL;

    if (NULL == orig_statx) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_statx = (orig_statx_t)dlsym(RTLD_NEXT, "statx");
#pragma GCC diagnostic pop

        assert(NULL != orig_statx);
        if (NULL == orig_statx) {
            return ENOSYS;
        }
    }

    return orig_statx(dfd, filename, atflag, mask, buffer);
}

static int internal_statx(int dfd, const char *filename, unsigned atflag, unsigned mask, struct statx *buffer)
{
    // TODO: implement this.

    return fin_statx(dfd, filename, atflag, mask, buffer);
}

int finesse_statx(int dfd, const char *filename, unsigned atflag, unsigned mask, struct statx *buffer)
{
    int result = internal_statx(dfd, filename, atflag, mask, buffer);

    FinesseApiCountCall(FINESSE_API_CALL_STATX, 0 == result);

    return result;
}
