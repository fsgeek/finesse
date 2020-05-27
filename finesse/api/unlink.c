/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */


#include "api-internal.h"

static int fin_unlink(const char *pathname)
{
    typedef int (*orig_unlink_t)(const char *pathname); 
    static orig_unlink_t orig_unlink = NULL;

    if (NULL == orig_unlink) {
        orig_unlink = (orig_unlink_t)dlsym(RTLD_NEXT, "unlink");

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
        orig_unlinkat = (orig_unlinkat_t) dlsym(RTLD_NEXT, "unlinkat");

        assert(NULL != orig_unlinkat);
        if (NULL == orig_unlinkat) {
            return EACCES;
        }
    }

    return orig_unlinkat(dirfd, pathname, flags);
}

int finesse_unlink(const char *pathname)
{
    int status;
    finesse_client_handle_t finesse_client_handle = NULL;
    uuid_t null_uuid;
    fincomm_message message = NULL;

    finesse_client_handle = finesse_check_prefix(pathname);

    if (NULL == finesse_client_handle) {
        // not of interest
        return fin_unlink(pathname);
    }

    memset(&null_uuid, 0, sizeof(uuid_t));

    status = FinesseSendUnlinkRequest(finesse_client_handle, &null_uuid, pathname, &message);

    if (0 == status) {
        status = FinesseGetUnlinkResponse(finesse_client_handle, message);
        if (0 == status) {
            status = message->Result;
        }
        FinesseFreeUnlinkResponse(finesse_client_handle, message);
    }

    return status;
}

int finesse_unlinkat(int dirfd, const char *pathname, int flags)
{
    int status;

    // TODO: implement the lookup here and really implement this
    status = fin_unlinkat(dirfd, pathname, flags);

    return status;
}
