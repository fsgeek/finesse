/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#include "api-internal.h"
#include "callstats.h"

static int fin_unlink(const char *pathname)
{
    typedef int (*orig_unlink_t)(const char *pathname);
    static orig_unlink_t orig_unlink = NULL;

    if (NULL == orig_unlink) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_unlink = (orig_unlink_t)dlsym(RTLD_NEXT, "unlink");
#pragma GCC diagnostic pop

        assert(NULL != orig_unlink);
        if (NULL == orig_unlink) {
            return EACCES;
        }
    }

    return orig_unlink(pathname);
}

static int fin_unlinkat(int dirfd, const char *pathname, int flags)
{
    typedef int (*orig_unlinkat_t)(int dirfd, const char *pathname, int flags);
    static orig_unlinkat_t orig_unlinkat = NULL;

    if (NULL == orig_unlinkat) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_unlinkat = (orig_unlinkat_t)dlsym(RTLD_NEXT, "unlinkat");
#pragma GCC diagnostic pop

        assert(NULL != orig_unlinkat);
        if (NULL == orig_unlinkat) {
            return EACCES;
        }
    }

    return orig_unlinkat(dirfd, pathname, flags);
}

static int internal_unlink(const char *file_name)
{
    fincomm_message         message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int                     result;
    struct timespec         start, stop, elapsed;
    int                     status, tstatus;
    uuid_t                  null_uuid;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    finesse_client_handle = finesse_check_prefix(file_name);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_UNLINK, &elapsed);

    if (NULL == finesse_client_handle) {
        // not of interest - fallback
        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        assert(0 == tstatus);

        status = fin_unlink(file_name);

        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        assert(0 == tstatus);
        timespec_diff(&start, &stop, &elapsed);
        FinesseApiRecordNative(FINESSE_API_CALL_STAT, &elapsed);

        return status;
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    memset(&null_uuid, 0, sizeof(uuid_t));

    status = FinesseSendUnlinkRequest(finesse_client_handle, &null_uuid, file_name, &message);
    assert(0 == status);
    status = FinesseGetUnlinkResponse(finesse_client_handle, message);
    assert(0 == status);
    result = message->Result;
    FinesseFreeUnlinkResponse(finesse_client_handle, message);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_UNLINK, &elapsed);

    return result;
}

int finesse_unlink(const char *pathname)
{
    int result = internal_unlink(pathname);

    FinesseApiCountCall(FINESSE_API_CALL_UNLINK, 0 == result);

    return result;
}

static int internal_unlinkat(int dirfd, const char *pathname, int flags)
{
    struct timespec         start, stop, elapsed;
    int                     status, tstatus;
    fincomm_message         message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int                     result;
    finesse_file_state_t *  ffs = NULL;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    // Let's see if we know about this file descriptor
    ffs = finesse_lookup_file_state(dirfd);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_UNLINKAT, &elapsed);

    if (NULL == ffs) {
        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        assert(0 == tstatus);

        // We aren't tracking this, so we do pass-through
        status = fin_unlinkat(dirfd, pathname, flags);

        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        assert(0 == tstatus);
        timespec_diff(&start, &stop, &elapsed);
        FinesseApiRecordNative(FINESSE_API_CALL_FSTAT, &elapsed);

        return status;
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    // We ARE tracking the file, so we can use the key to query.
    assert(0 == flags);  // we currently don't support flags here

    status = FinesseSendUnlinkRequest(finesse_client_handle, &ffs->key, pathname, &message);
    assert(0 == status);
    status = FinesseGetUnlinkResponse(finesse_client_handle, message);
    assert(0 == status);
    result = message->Result;
    FinesseFreeUnlinkResponse(finesse_client_handle, message);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_UNLINKAT, &elapsed);

    return result;
}

int finesse_unlinkat(int dirfd, const char *pathname, int flags)
{
    int status = internal_unlinkat(dirfd, pathname, flags);

    FinesseApiCountCall(FINESSE_API_CALL_UNLINKAT, 0 == status);

    return status;
}
