/*
 * (C) Copyright 2018 Tony Mason
 * All Rights Reserved
 */

#include "finesse-internal.h"
#include <stdarg.h>
#include <uuid/uuid.h>
#include <pthread.h>
/* 
 * REF: https://rafalcieslak.wordpress.com/2013/04/02/dynamic-linker-tricks-using-ld_preload-to-cheat-inject-features-and-investigate-programs/
 *      https://github.com/poliva/ldpreloadhook/blob/master/hook.c
 */

struct map_name_args {
    const char *mapfile_name;
    uuid_t *uuid;
    int *status;  
};

static int fin_open(const char *pathname, int flags, ...)
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

static int fin_openat(int dirfd, const char *pathname, int flags, ...)
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

static int fin_close(int fd)
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

int finesse_open(const char *pathname, int flags, ...)
{
    int fd;
    va_list args;
    mode_t mode;
    int status;
    uuid_t uuid;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    finesse_init();

    //
    // Let's see if it makes sense for us to try opening this
    //
    if (0 == finesse_check_prefix(pathname)) {
        // not of interest
        return fin_open(pathname, flags, mode);
    }

    //
    // let's see if the other side knows about this file already
    // invoke finesse_map_name asynchronously
    //
    pthread_t tid;
    struct map_name_args *mn_args = (struct map_name_args *) malloc(sizeof(struct map_name_args));
    mn_args->mapfile_name = pathname;
    mn_args->uuid = &uuid;
    mn_args->status = &status;
    pthread_create(&tid, NULL, finesse_map_name_async, (void *) mn_args);

    if (0 == status) {
        // if this is a NOENT and O_CREAT is not specified, we know this open will fail
        if ((ENOENT == status) && (O_CREAT == (flags & O_CREAT))) {
            errno = ENOENT;
            return -1;
        }
    }

    fd = fin_open(pathname, flags, mode);
    pthread_join(tid, NULL);
    free(mn_args); 

    while (fd >= 0) {
        finesse_key_t key;
        finesse_file_state_t *file_state;

        if (0 != status) {
            // retry the lookup here
            status = finesse_map_name(pathname, &uuid);

            if (0 != status) {
                // map still failed
                break;
            }
        }

        file_state = finesse_create_file_state(fd, &key, pathname);

        if (NULL == file_state) {
            fprintf(stderr, "%d @ %s (%s) - NULL from finesse_create_file_state\n", __LINE__, __FILE__, __FUNCTION__);

            //
            // If anything goes wrong, we just do nothing because the fallback is
            // to go the "old" way.
            //
        }
        break;

    }

    return finesse_fd_to_nfd(fd);
}


int finesse_creat(const char *pathname, mode_t mode) 
{
    int fd = finesse_open(pathname, O_CREAT|O_WRONLY|O_TRUNC, mode);

    return finesse_fd_to_nfd(fd);
}

int finesse_openat(int dirfd, const char *pathname, int flags, ...)
{
    int fd;
    va_list args;
    mode_t mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    fd = fin_openat(dirfd, pathname, flags, mode);

    if (fd >= 0) {
        /* TODO: add the lookup/insert logic for this pair of fds */
    }

    return finesse_fd_to_nfd(fd);
}

int finesse_close(int fd)
{
    finesse_file_state_t *file_state = NULL;
    int status = 0;

    finesse_init();

    status = fin_close(finesse_nfd_to_fd(fd));

    if (0 != status) {
        // call failed
        return status;
    }

    file_state = finesse_lookup_file_state(finesse_nfd_to_fd(fd));

    if (NULL != file_state) {
        finesse_delete_file_state(file_state);
    }

    return 0;
}

int finesse_map_name(const char *mapfile_name, uuid_t *uuid)
{
    int status;
    fincomm_message message;

    status = FinesseSendNameMapRequest(finesse_client_handle, (char *)(uintptr_t)mapfile_name, &message);
    while (0 == status) {
        status = FinesseGetNameMapResponse(finesse_client_handle, message, uuid);
        break;
    }

    return status;
}


void *finesse_map_name_async(void *args)
{
    struct map_name_args *parsed_args = (struct map_name_args *) args;  
    int status;
    fincomm_message message;

    status = FinesseSendNameMapRequest(finesse_client_handle, (char *)(uintptr_t)parsed_args->mapfile_name, &message);
    while (0 == status) {
        status = FinesseGetNameMapResponse(finesse_client_handle, message, parsed_args->uuid);
        break;
    }
    *(parsed_args->status) = status;
     
    return NULL;
}
