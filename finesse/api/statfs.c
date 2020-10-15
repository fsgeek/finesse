/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
 */

#include "api-internal.h"
#include "callstats.h"

static int fin_fstatvfs(int fd, struct statvfs *buf)
{
    typedef int (*orig_fstatfs_t)(int fd, struct statvfs *buf);
    static orig_fstatfs_t orig_fstatfs = NULL;

    if (NULL == orig_fstatfs) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_fstatfs = (orig_fstatfs_t)dlsym(RTLD_NEXT, "fstatvfs");
#pragma GCC diagnostic pop

        assert(NULL != orig_fstatfs);
        if (NULL == orig_fstatfs) {
            return EACCES;
        }
    }

    return orig_fstatfs(fd, buf);
}

int finesse_fstatvfs(int fd, struct statvfs *buf)
{
    int                   status;
    finesse_file_state_t *file_state = NULL;
    fincomm_message       message;

    file_state = finesse_lookup_file_state(finesse_nfd_to_fd(fd));

    if (NULL == file_state) {
        // this is a fallback case
        return fin_fstatvfs(fd, buf);
    }

    status = FinesseSendFstatfsRequest(file_state->client, &file_state->key, &message);
    assert(0 == status);
    status = FinesseGetFstatfsResponse(file_state->client, message, buf);
    assert(0 == status);
    FinesseFreeStatfsResponse(file_state->client, message);
    return 0;
}

static int fin_statvfs(const char *path, struct statvfs *buf)
{
    typedef int (*orig_statfs_t)(const char *path, struct statvfs *buf);
    static orig_statfs_t orig_statfs = NULL;

    if (NULL == orig_statfs) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_statfs = (orig_statfs_t)dlsym(RTLD_NEXT, "statfs");
#pragma GCC diagnostic pop

        assert(NULL != orig_statfs);
        if (NULL == orig_statfs) {
            return EACCES;
        }
    }

    return orig_statfs(path, buf);
}

int finesse_statvfs(const char *path, struct statvfs *buf)
{
    int                     status;
    fincomm_message         message;
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

static void map_statvfs_to_statfs(struct statfs *buf, const struct statvfs *vfsbuf)
{
    memset(buf, 0, sizeof(struct statvfs));

    buf->f_type   = 0;  // Note: we don't HAVE this information, so we lie
    buf->f_bsize  = vfsbuf->f_bsize;
    buf->f_blocks = vfsbuf->f_blocks;
    buf->f_bfree  = vfsbuf->f_bfree;
    buf->f_bavail = vfsbuf->f_bavail;
    buf->f_files  = vfsbuf->f_files;
    buf->f_ffree  = vfsbuf->f_ffree;
    memcpy(&buf->f_fsid, &vfsbuf->f_fsid, sizeof(buf->f_fsid));
    buf->f_namelen = vfsbuf->f_namemax;
    buf->f_frsize  = vfsbuf->f_frsize;
    buf->f_flags   = vfsbuf->f_flag;
}

int finesse_statfs(const char *path, struct statfs *buf)
{
    struct statvfs vfsbuf;
    int            result;

    memset(&vfsbuf, 0, sizeof(vfsbuf));
    result = finesse_statvfs(path, &vfsbuf);

    memset(buf, 0, sizeof(struct statfs));
    if (0 == result) {
        map_statvfs_to_statfs(buf, &vfsbuf);
    }

    return result;
}

int finesse_fstatfs(int fd, struct statfs *buf)
{
    struct statvfs vfsbuf;
    int            result;

    memset(&vfsbuf, 0, sizeof(vfsbuf));
    result = finesse_fstatvfs(fd, &vfsbuf);

    memset(buf, 0, sizeof(struct statfs));
    if (0 == result) {
        map_statvfs_to_statfs(buf, &vfsbuf);
    }

    return result;
}
