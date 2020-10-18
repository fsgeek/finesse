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
    int                     status;
    uuid_t                  null_uuid;
    DECLARE_TIME(FINESSE_API_CALL_UNLINK)

    START_TIME

    finesse_client_handle = finesse_check_prefix(file_name);

    STOP_FINESSE_TIME

    if (NULL == finesse_client_handle) {
        // not of interest - fallback

        START_TIME

        status = fin_unlink(file_name);

        STOP_NATIVE_TIME

        return status;
    }

    START_TIME

    memset(&null_uuid, 0, sizeof(uuid_t));

    status = FinesseSendUnlinkRequest(finesse_client_handle, &null_uuid, file_name, &message);
    assert(0 == status);
    status = FinesseGetUnlinkResponse(finesse_client_handle, message);
    assert(0 == status);
    result = message->Result;
    FinesseFreeUnlinkResponse(finesse_client_handle, message);

    STOP_NATIVE_TIME

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
    int                   status;
    fincomm_message       message;
    finesse_file_state_t *ffs = NULL;
    DECLARE_TIME(FINESSE_API_CALL_UNLINKAT)

    while (1) {
        if (0 != flags) {
            START_TIME

            // We don't handle any flags at present
            status = fin_unlinkat(dirfd, pathname, flags);

            STOP_NATIVE_TIME
            break;
        }

        if (AT_FDCWD == dirfd) {
            if ('/' == *pathname) {
                // Absolute path name!
                status = internal_unlink(pathname);
            }
            else {
                START_TIME

                status = fin_unlinkat(dirfd, pathname, flags);

                STOP_NATIVE_TIME
            }
            break;
        }

        START_TIME

        // look up the file descriptor to see if we know about it already
        ffs = finesse_lookup_file_state(finesse_nfd_to_fd(dirfd));

        STOP_FINESSE_TIME

        if (NULL == ffs) {
            START_TIME

            // Don't care about this one
            status = fin_unlinkat(dirfd, pathname, flags);

            STOP_NATIVE_TIME

            break;
        }

        START_TIME
        // At this point we do know about the directory, so we can construct the unlink request
        status = FinesseSendUnlinkRequest(ffs->client, &ffs->key, pathname, &message);
        assert(0 == status);
        status = FinesseGetUnlinkResponse(ffs->client, message);
        assert(0 == status);
        status = message->Result;
        FinesseFreeUnlinkResponse(ffs->client, message);

        STOP_NATIVE_TIME

        // Done!
        break;
    }

    return status;
}

int finesse_unlinkat(int dirfd, const char *pathname, int flags)
{
    int status = internal_unlinkat(dirfd, pathname, flags);

    FinesseApiCountCall(FINESSE_API_CALL_UNLINKAT, 0 == status);

    return status;
}
