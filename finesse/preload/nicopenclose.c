/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */


#include "nicfsinternal.h"
#include <uuid/uuid.h>

// forward references
// static int map_name(mqd_t mq_req, mqd_t mq_rsp, uuid_t clientUuid, const char *mapfile_name, uuid_t *uuid);

/* 
 * REF: https://rafalcieslak.wordpress.com/2013/04/02/dynamic-linker-tricks-using-ld_preload-to-cheat-inject-features-and-investigate-programs/
 *      https://github.com/poliva/ldpreloadhook/blob/master/hook.c
 */

static int nic_open(const char *pathname, int flags, ...)
{
    typedef int (*orig_open_t)(const char *pathname, int flags, ...); 
    static orig_open_t orig_open = NULL;
    va_list args;
    mode_t mode;

    if (NULL == orig_open) {
        orig_open = (orig_open_t)dlsym(RTLD_NEXT, "open");

        assert(NULL != orig_open);
        if (NULL == orig_open) {
            return EACCES;
        }
    }

	va_start (args, flags);
	mode = va_arg (args, int);
	va_end (args);

    return orig_open(pathname,flags, mode);
}

static int nic_openat(int dirfd, const char *pathname, int flags, ...)
{
    typedef int (*orig_openat_t)(int dirfd, const char *pathname, int flags, ...);
    static orig_openat_t orig_openat = NULL;
    va_list args;
    mode_t mode;

    if (NULL == orig_openat) {
        orig_openat = (orig_openat_t) dlsym(RTLD_NEXT, "openat");

        assert(NULL != orig_openat);
        if (NULL == orig_openat) {
            return EACCES;
        }
    }

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    return orig_openat(dirfd, pathname, flags, mode);
}

static int nic_close(int fd)
{
    typedef int (*orig_close_t)(int fd);
    static orig_close_t orig_close = NULL;

    if (NULL == orig_close) {
        orig_close = (orig_close_t) dlsym(RTLD_NEXT, "close");

        assert(NULL != orig_close);
        if (NULL == orig_close) {
            return EACCES;
        }
    }

    /* TODO: add the remove/delete logic for this file descriptor to path mapping */

    return orig_close(fd);
}

int nicfs_open(const char *pathname, int flags, ...)
{
    int fd;
    va_list args;
    mode_t mode;
    int status;
    uuid_t uuid;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    nicfs_init();

    //
    // Let's see if it makes sense for us to try opening this
    //
    if (0 == nicfs_check_prefix(pathname)) {
        // not of interest
        return nic_open(pathname, flags, mode);
    }

    //
    // let's see if the other side knows about this file already
    //
    status = nicfs_map_name(pathname, &uuid);

    if (0 == status) {
        // if this is a NOENT and O_CREAT is not specified, we know this open will fail
        if ((ENOENT == status) && (O_CREAT == (flags & O_CREAT))) {
            errno = ENOENT;
            return -1;
        }
    }

    fd = nic_open(pathname, flags, mode);
 
    while (fd >= 0) {
        nicfs_key_t key;
        nicfs_file_state_t *file_state;

        if (0 != status) {
            // retry the lookup here
            status = nicfs_map_name(pathname, &uuid);

            if (0 != status) {
                // map still failed
                break;
            }
        }

        file_state = nicfs_create_file_state(fd, &key, pathname);

        if (NULL == file_state) {
            fprintf(stderr, "%d @ %s (%s) - NULL from nicfs_create_file_state\n", __LINE__, __FILE__, __FUNCTION__);

            //
            // If anything goes wrong, we just do nothing because the fallback is
            // to go the "old" way.
            //
        }
        break;

    }

    return nicfs_fd_to_nfd(fd);
}


int nicfs_creat(const char *pathname, mode_t mode) 
{
    int fd = nicfs_open(pathname, O_CREAT|O_WRONLY|O_TRUNC, mode);

    return nicfs_fd_to_nfd(fd);
}

int nicfs_openat(int dirfd, const char *pathname, int flags, ...)
{
    int fd;
    va_list args;
    mode_t mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    fd = nic_openat(dirfd, pathname, flags, mode);

    if (fd >= 0) {
        /* TODO: add the lookup/insert logic for this pair of fds */
    }

    return nicfs_fd_to_nfd(fd);
}

int nicfs_close(int fd)
{
    nicfs_file_state_t *file_state = NULL;
    int status = 0;

    nicfs_init();

    status = nic_close(nicfs_nfd_to_fd(fd));

    if (0 != status) {
        // call failed
        return status;
    }

    file_state = nicfs_lookup_file_state(nicfs_nfd_to_fd(fd));

    if (NULL != file_state) {
        nicfs_delete_file_state(file_state);
    }

    return 0;
}

