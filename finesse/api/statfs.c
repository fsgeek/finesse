/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
 */


#include "api-internal.h"

static int fin_fstatfs(fuse_ino_t nodeid, struct statvfs *buf)
{
    typedef int (*orig_fstatfs_t)(fuse_ino_t nodeid, struct statvfs *buf);
    static orig_fstatfs_t orig_fstatfs = NULL;

    if (NULL == orig_fstatfs) {
        orig_fstatfs = (orig_fstatfs_t)dlsym(RTLD_NEXT, "fstatfs");

        assert(NULL != orig_fstatfs);
        if (NULL == orig_fstatfs) {
            return EACCES;
        }
    }

    return orig_fstatfs(nodeid, buf);
}

int finesse_fstatfs(fuse_ino_t nodeid, struct statvfs *buf)
{
    int status;

    finesse_init();

    //status = fin_fstatfs_call(nodeid, buf);
    status = fin_fstatfs(nodeid, buf);

    if (0 > status) {
        status = fin_fstatfs(nodeid, buf);
    }

    return status;
}

//static int fin_fstatfs_call(fuse_ino_t nodeid, struct statvfs *buf)
//{
//    int status;
//    uint64_t req_id;
//
//    status = FinesseSendFstatfsRequest(finesse_client_handle, nodeid, &req_id);
//    while (0 == status) {
//        status = FinesseGetFstatfsResponse(finesse_client_handle, req_id, buf);
//        break;
//    }
//
//    return status;
//}

static int fin_statfs_call(const char *path, struct statvfs *buf);

static int fin_statfs(const char *path, struct statvfs *buf)
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

int finesse_statfs(const char *path, struct statvfs *buf)
{
    int status;

    finesse_init();

    status = fin_statfs_call(path, buf);

    if (0 > status) {
        status = fin_statfs(path, buf);
    }

    return status;
}

static int fin_statfs_call(const char *path, struct statvfs *buf)
{
    int status;
    fincomm_message message;

    status = FinesseSendStatfsRequest(finesse_client_handle, path, &message);
    while (0 == status) {
        status = FinesseGetStatfsResponse(finesse_client_handle, message, buf);
        break;
    }

    return status;
}
