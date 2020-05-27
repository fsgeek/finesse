/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
 */


#include "api-internal.h"

static int fin_fstatvfs(int fd, struct statvfs *buf)
{
    typedef int (*orig_fstatfs_t)(int fd, struct statvfs *buf);
    static orig_fstatfs_t orig_fstatfs = NULL;

    if (NULL == orig_fstatfs) {
        orig_fstatfs = (orig_fstatfs_t)dlsym(RTLD_NEXT, "fstatfs");

        assert(NULL != orig_fstatfs);
        if (NULL == orig_fstatfs) {
            return EACCES;
        }
    }

    return orig_fstatfs(fd, buf);
}

int finesse_fstatvfs(int fd, struct statvfs *buf)
{
    int status;
    finesse_file_state_t *file_state = NULL;
    fincomm_message message;

    file_state = finesse_lookup_file_state(finesse_nfd_to_fd(fd));

    if (NULL == file_state) {
        // this is a fallback case
        return fin_fstatvfs(fd, buf);
    }

    status = FinesseSendFstatfsRequest(file_state->client, &file_state->key, &message);
    assert(0 == status);
    status  = FinesseGetFstatfsResponse(file_state->client, message, buf);
    assert(0 == status);
    FinesseFreeStatfsResponse(file_state->client, message);
    return 0;

}

static int fin_statvfs(const char *path, struct statvfs *buf)
{
    typedef int (*orig_statfs_t)(const char *path, struct statvfs *buf);
    static orig_statfs_t orig_statfs = NULL;

    if (NULL == orig_statfs) {
        orig_statfs = (orig_statfs_t)dlsym(RTLD_NEXT, "statfs");

        assert(NULL != orig_statfs);
        if (NULL == orig_statfs) {
            return EACCES;
        }
    }

    return orig_statfs(path, buf);
}

int finesse_statvfs(const char *path, struct statvfs *buf)
{
    int status;
    fincomm_message message;
    finesse_client_handle_t finesse_client_handle = NULL;

    finesse_client_handle = finesse_check_prefix(path);
    
    if (NULL == finesse_client_handle) {
        // not of interest - fallback path
        return fin_statvfs(path, buf);
    }

    status = FinesseSendStatfsRequest(finesse_client_handle, path, &message);
    assert(0 == status);
    status = FinesseGetStatfsResponse(finesse_client_handle, message, buf);    
    assert(0 == status);
    FinesseFreeStatfsResponse(finesse_client_handle, message);

    return status;
}


// There are statfs fstatfs defined but they're giving me fits at the moment,
// and are deprecated.  So commenting them out for now.

static int fin_statfs(const char *path, struct statfs *buf)
{
    typedef int (*orig_statfs_t)(const char *path, struct statfs *buf);
    static orig_statfs_t orig_statfs = NULL;

    if (NULL == orig_statfs) {
        orig_statfs = (orig_statfs_t)dlsym(RTLD_NEXT, "statfs");

        assert(NULL != orig_statfs);
        if (NULL == orig_statfs) {
            return EACCES;
        }
    }

    return orig_statfs(path, buf);
}

int finesse_statfs(const char *path, struct statfs *buf)
{
    return fin_statfs(path, buf);
}


static int fin_fstatfs(int fd, struct statfs *buf)
{
    typedef int (*orig_fstatfs_t)(int fd, struct statfs *buf);
    static orig_fstatfs_t orig_fstatfs = NULL;

    if (NULL == orig_fstatfs) {
        orig_fstatfs = (orig_fstatfs_t)dlsym(RTLD_NEXT, "fstatfs");

        assert(NULL != orig_fstatfs);
        if (NULL == orig_fstatfs) {
            return EACCES;
        }
    }

    return orig_fstatfs(fd, buf);

}

int finesse_fstatfs(int fd, struct statfs *buf)
{
    // TODO: implement this?
    return fin_fstatfs(fd, buf);
}

