/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */


#include "api-internal.h"

static int fin_unlink_call(const char *unlinkfile_name);

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

    finesse_init();

    if (0 == finesse_check_prefix(pathname)) {
        // not of interest
        return fin_unlink(pathname);
    }

    status = fin_unlink_call(pathname);

    if (0 > status) {
        status = fin_unlink(pathname);
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



static int fin_unlink_call(const char *unlinkfile_name)
{
    int status;
    uuid_t null_uuid;
    fincomm_message message;

    memset(&null_uuid, 0, sizeof(uuid_t));

    status = FinesseSendUnlinkRequest(finesse_client_handle, &null_uuid, unlinkfile_name, &message);

    if (0 == status) {
        status = FinesseGetUnlinkResponse(finesse_client_handle, message);
        if (0 == status) {
            status = message->Result;
        }
        FinesseReleaseRequestBuffer(finesse_client_handle, message);
    }

    return status;
}
