/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
 */


#include "api-internal.h"

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


int finesse_access(const char *pathname, int mode)
{
    int status;
    fincomm_message message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int result;

    finesse_client_handle = finesse_check_prefix(pathname);

    if (NULL == finesse_client_handle) {
        // not of interest - fallback
        return fin_access(pathname, mode);
    }

    status = FinesseSendAccessRequest(finesse_client_handle, NULL, pathname, mode, &message);
    assert(0 == status);
    status = FinesseGetAccessResponse(finesse_client_handle, message, &result);    
    assert(0 == status);
    FinesseFreeAccessResponse(finesse_client_handle, message);

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

int finesse_faccessat(int dirfd, const char *pathname, int mode, int flags)
{
    int status;
    finesse_file_state_t *file_state = NULL;
    fincomm_message message;
    finesse_client_handle_t finesse_client_handle = NULL;
    int result;

    finesse_client_handle = finesse_check_prefix(pathname);

    if (NULL == finesse_client_handle) {
        // not of interest - fallback
        if (-1 == dirfd) {
            return fin_access(pathname, mode);
        }
        else {
            return fin_faccessat(dirfd, pathname, mode, flags);
        }
    }

    file_state = finesse_lookup_file_state(finesse_nfd_to_fd(dirfd));

    if (NULL == file_state) {
        // don't know the parent? Not interested...
        return fin_faccessat(dirfd, pathname, mode, flags);
    }

    status = FinesseSendAccessRequest(finesse_client_handle, &file_state->key, pathname, mode, &message);
    assert(0 == status);
    status = FinesseGetAccessResponse(finesse_client_handle, message, &result);    
    assert(0 == status);
    FinesseFreeAccessResponse(finesse_client_handle, message);

    return result;
}
