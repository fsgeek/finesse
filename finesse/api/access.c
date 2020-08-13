/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
 */

#include "api-internal.h"
#include "callstats.h"

static int fin_access(const char *pathname, int mode)
{
    typedef int (*orig_access_t)(const char *path, int mode);
    static orig_access_t orig_access = NULL;

    if (NULL == orig_access) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_access = (orig_access_t)dlsym(RTLD_NEXT, "access");
#pragma GCC diagnostic pop
        assert(NULL != orig_access);
        if (NULL == orig_access) {
            return EACCES;
        }
    }

    return orig_access(pathname, mode);
}

static int internal_access(const char *pathname, int mode)
{
    fincomm_message         message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int                     result;
    struct timespec         start, stop, elapsed;
    int                     status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    finesse_client_handle = finesse_check_prefix(pathname);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_ACCESS, &elapsed);

    if (NULL == finesse_client_handle) {
        // not of interest - fallback
        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        assert(0 == tstatus);

        status = fin_access(pathname, mode);

        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        assert(0 == tstatus);
        timespec_diff(&start, &stop, &elapsed);
        FinesseApiRecordNative(FINESSE_API_CALL_ACCESS, &elapsed);

        return status;
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    status = FinesseSendAccessRequest(finesse_client_handle, NULL, pathname, mode, &message);
    assert(0 == status);
    status = FinesseGetAccessResponse(finesse_client_handle, message, &result);
    assert(0 == status);
    FinesseFreeAccessResponse(finesse_client_handle, message);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_ACCESS, &elapsed);

    return result;
}

int finesse_access(const char *pathname, int mode)
{
    int result = internal_access(pathname, mode);

    FinesseApiCountCall(FINESSE_API_CALL_ACCESS, 0 == result);

    return result;
}

static int fin_faccessat(int dirfd, const char *pathname, int mode, int flags)
{
    typedef int (*orig_faccessat_t)(int dirfd, const char *pathname, int mode, int flags);
    static orig_faccessat_t orig_faccessat = NULL;

    if (NULL == orig_faccessat) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_faccessat = (orig_faccessat_t)dlsym(RTLD_NEXT, "faccessat");
#pragma GCC diagnostic pop

        assert(NULL != orig_faccessat);
        if (NULL == orig_faccessat) {
            return EACCES;
        }
    }

    return orig_faccessat(dirfd, pathname, mode, flags);
}

static int internal_faccessat(int dirfd, const char *pathname, int mode, int flags)
{
    finesse_file_state_t *  file_state = NULL;
    fincomm_message         message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int                     result;
    struct timespec         start, stop, elapsed;
    int                     status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    finesse_client_handle = finesse_check_prefix(pathname);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_ACCESS, &elapsed);

    if (NULL == finesse_client_handle) {
        // not of interest - fallback
        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        assert(0 == tstatus);
        if (-1 == dirfd) {
            status = fin_access(pathname, mode);
        }
        else {
            status = fin_faccessat(dirfd, pathname, mode, flags);
        }
        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        assert(0 == tstatus);
        timespec_diff(&start, &stop, &elapsed);
        FinesseApiRecordNative(FINESSE_API_CALL_ACCESS, &elapsed);
        return status;
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    file_state = finesse_lookup_file_state(finesse_nfd_to_fd(dirfd));

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_ACCESS, &elapsed);

    if (NULL == file_state) {
        // don't know the parent? Not interested...
        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        assert(0 == tstatus);

        status = fin_faccessat(dirfd, pathname, mode, flags);

        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        assert(0 == tstatus);
        timespec_diff(&start, &stop, &elapsed);
        FinesseApiRecordNative(FINESSE_API_CALL_ACCESS, &elapsed);
        return status;
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    status = FinesseSendAccessRequest(finesse_client_handle, &file_state->key, pathname, mode, &message);
    assert(0 == status);
    status = FinesseGetAccessResponse(finesse_client_handle, message, &result);
    assert(0 == status);
    FinesseFreeAccessResponse(finesse_client_handle, message);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_ACCESS, &elapsed);

    return result;
}

int finesse_faccessat(int dirfd, const char *pathname, int mode, int flags)
{
    int result = internal_faccessat(dirfd, pathname, mode, flags);

    FinesseApiCountCall(FINESSE_API_CALL_FACCESSAT, 0 == result);

    return result;
}
