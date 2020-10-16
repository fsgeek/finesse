/*
 * (C) Copyright 2018 Tony Mason
 * All Rights Reserved
 */

#include "api-internal.h"

/*
 * REF:
 * https://rafalcieslak.wordpress.com/2013/04/02/dynamic-linker-tricks-using-ld_preload-to-cheat-inject-features-and-investigate-programs/
 *      https://github.com/poliva/ldpreloadhook/blob/master/hook.c
 */

struct map_name_args {
    const char *mapfile_name;
    uuid_t *    uuid;
    int *       status;
};

static int fin_open(const char *pathname, int flags, ...)
{
    typedef int (*orig_open_t)(const char *pathname, int flags, ...);
    static orig_open_t orig_open = NULL;
    va_list            args;
    mode_t             mode;

    if (NULL == orig_open) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_open = (orig_open_t)dlsym(RTLD_NEXT, "open");
#pragma GCC diagnostic pop

        assert(NULL != orig_open);
        if (NULL == orig_open) {
            errno = EACCES;
            return -1;
        }
    }

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    return orig_open(pathname, flags, mode);
}

static int fin_openat(int dirfd, const char *pathname, int flags, ...)
{
    typedef int (*orig_openat_t)(int dirfd, const char *pathname, int flags, ...);
    static orig_openat_t orig_openat = NULL;
    va_list              args;
    mode_t               mode;

    if (NULL == orig_openat) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_openat = (orig_openat_t)dlsym(RTLD_NEXT, "openat");
#pragma GCC diagnostic pop

        assert(NULL != orig_openat);
        if (NULL == orig_openat) {
            errno = EACCES;
            return -1;
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_close = (orig_close_t)dlsym(RTLD_NEXT, "close");
#pragma GCC diagnostic pop

        assert(NULL != orig_close);
        if (NULL == orig_close) {
            errno = EACCES;
            return -1;
        }
    }

    /* TODO: add the remove/delete logic for this file descriptor to path mapping */

    return orig_close(fd);
}

int finesse_open(const char *pathname, int flags, ...)
{
    int                     fd;
    va_list                 args;
    mode_t                  mode;
    int                     status;
    uuid_t                  uuid;
    fincomm_message         message       = NULL;
    finesse_client_handle_t client_handle = NULL;
    finesse_file_state_t *  ffs           = NULL;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    //
    // Let's see if it makes sense for us to try opening this
    //
    client_handle = finesse_check_prefix(pathname);
    if (NULL == client_handle) {
        // not of interest
        return fin_open(pathname, flags, mode);
    }

    fprintf(stderr, "%s:%d open for %s\n", __FILE__, __LINE__, pathname);
    //
    // Ask the Finesse server
    //
    memset(&uuid, 0, sizeof(uuid));
    status = FinesseSendNameMapRequest(client_handle, &uuid, (char *)(uintptr_t)pathname, &message);

    if (0 != status) {
        // fallback
        fprintf(stderr, "%s:%d failed with result %d for file %s\n", __FILE__, __LINE__, status, pathname);
        return fin_open(pathname, flags, mode);
    }

    // Otherwise, let's do the open
    fd = fin_open(pathname, flags, mode);

    // Get the answer from the server
    status = FinesseGetNameMapResponse(client_handle, message, &uuid);
    assert(0 == status);
    assert(!uuid_is_null(uuid));
    FinesseFreeNameMapResponse(client_handle, message);

    if (0 > fd) {
        // both calls failed
        if (status != 0) {
            return fd;
        }
        // otherwise, the open failed but the remote succeeded
        status = FinesseSendNameMapReleaseRequest(client_handle, &uuid, &message);
        if (0 != status) {
            // note that this is uuid leak - maybe the server died?
            return fd;
        }
        FinesseFreeNameMapResponse(client_handle, message);
    }

    // the open succeeded
    if (0 != status) {
        // lookup failed
        fprintf(stderr, "%s:%d failed with result %d for file %s\n", __FILE__, __LINE__, status, pathname);
        return fd;
    }

    // open succeeded AND lookup succeeded - insert into the lookup table
    // Note that if this failed (file_state is null) we don't care - that
    // just turns this into a fallback case.
    ffs = finesse_create_file_state(fd, client_handle, &uuid, pathname);
    assert(NULL != ffs);  // if it failed, we'd need to release the name map

    return finesse_fd_to_nfd(fd);
}

int finesse_creat(const char *pathname, mode_t mode)
{
    int fd = finesse_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);

    return finesse_fd_to_nfd(fd);
}

int finesse_openat(int dirfd, const char *pathname, int flags, ...)
{
    int                   fd;
    va_list               args;
    mode_t                mode;
    int                   status;
    uuid_t                uuid;
    fincomm_message       message = NULL;
    finesse_file_state_t *ffs     = NULL;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    //
    // first, let's lookup this file descriptor and see if we already know about it
    //
    if (AT_FDCWD == dirfd) {
        // special case
        // assert(0); // TODO
        return fin_openat(dirfd, pathname, flags, mode);
    }

    // Let's see if we know about this file descriptor
    ffs = finesse_lookup_file_state(dirfd);
    if (NULL == ffs) {
        // We aren't tracking this, so we do pass-through
        return fin_openat(dirfd, pathname, flags, mode);
    }

    // We ARE tracking the directory, so we need to do a name map operation.
    status = FinesseSendNameMapRequest(ffs->client, &ffs->key, pathname, &message);
    assert(0 == status);

    // Now call the underlying implementation
    fd = fin_openat(dirfd, pathname, flags, mode);

    // Get the answer from the server
    status = FinesseGetNameMapResponse(ffs->client, message, &uuid);
    FinesseFreeNameMapResponse(ffs->client, message);

    if (0 > fd) {
        if (0 != status) {
            // both calls failed
            return fd;
        }

        // otherwise, the open failed but the remote succeeded
        status = FinesseSendNameMapReleaseRequest(ffs->client, &uuid, &message);
        if (0 != status) {
            // maybe the server died?
            return fd;
        }
        FinesseFreeNameMapResponse(ffs->client, message);
    }

    // the open succeeded
    if (0 != status) {
        // name map failed
        return fd;
    }

    // open succeeded AND lookup succeeded - insert into the lookup table
    // Note that if this failed (file_state is null) we don't care - that
    // just turns this into a fallback case.
    ffs = finesse_create_file_state(fd, ffs->client, &uuid, pathname);
    assert(NULL != ffs);  // if it failed, we'd need to release the name map

    return finesse_fd_to_nfd(fd);
}

int finesse_close(int fd)
{
    finesse_file_state_t *file_state = NULL;
    int                   status     = 0;

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

static FILE *fin_fopen(const char *pathname, const char *mode)
{
    typedef FILE *(*orig_fopen_t)(const char *pathname, const char *mode);
    static orig_fopen_t orig_fopen = NULL;

    if (NULL == orig_fopen) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_fopen = (orig_fopen_t)dlsym(RTLD_NEXT, "fopen");
#pragma GCC diagnostic pop

        assert(NULL != orig_fopen);
        if (NULL == orig_fopen) {
            errno = EACCES;
            return NULL;
        }
    }

    return orig_fopen(pathname, mode);
}

static FILE *fin_fdopen(int fd, const char *mode)
{
    typedef FILE *(*orig_fdopen_t)(int fd, const char *mode);
    static orig_fdopen_t orig_fdopen = NULL;

    if (NULL == orig_fdopen) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_fdopen = (orig_fdopen_t)dlsym(RTLD_NEXT, "fdopen");
#pragma GCC diagnostic pop

        assert(NULL != orig_fdopen);
        if (NULL == orig_fdopen) {
            errno = EACCES;
            return NULL;
        }
    }

    return orig_fdopen(fd, mode);
}

static FILE *fin_freopen(const char *pathname, const char *mode, FILE *stream)
{
    typedef FILE *(*orig_freopen_t)(const char *pathname, const char *mode, FILE *stream);
    static orig_freopen_t orig_freopen = NULL;

    if (NULL == orig_freopen) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_freopen = (orig_freopen_t)dlsym(RTLD_NEXT, "freopen");
#pragma GCC diagnostic pop

        assert(NULL != orig_freopen);
        if (NULL == orig_freopen) {
            errno = EACCES;
            return NULL;
        }
    }

    return orig_freopen(pathname, mode, stream);
}

FILE *finesse_fopen(const char *pathname, const char *mode)
{
    // TODO: add the finesse integration here
    // (1) send the request (just like open)
    // (2) if the open succeeds to the library, match it up with
    //     the response from FUSE
    // (3) Update the mapping table, if necessary - keep using the file descriptor (its inside FILE *)
    //
    // Something to consider: glibc supports options for these files to be _memory mapped_
    // Not sure if we need to handle that differently, or not
    //
    FILE *                  file;
    int                     status;
    uuid_t                  uuid;
    fincomm_message         message               = NULL;
    finesse_client_handle_t finesse_client_handle = NULL;
    finesse_file_state_t *  ffs                   = NULL;

    finesse_client_handle = finesse_check_prefix(pathname);

    if (NULL == finesse_client_handle) {
        // not of interest
        return fin_fopen(pathname, mode);
    }

    //
    // Ask the other side
    //
    memset(&uuid, 0, sizeof(uuid));
    status = FinesseSendNameMapRequest(finesse_client_handle, &uuid, (char *)(uintptr_t)pathname, &message);

    if (0 != status) {
        // fallback
        return fin_fopen(pathname, mode);
    }

    // Let's do the open
    file = fin_fopen(pathname, mode);

    // Now get the answer from the server
    status = FinesseGetNameMapResponse(finesse_client_handle, message, &uuid);
    FinesseFreeNameMapResponse(finesse_client_handle, message);

    if (NULL == file) {
        if (0 != status) {
            // both calls failed
            return file;
        }

        // otherwise, the fopen failed, but the remote map succeeded
        // release the name map
        status = FinesseSendNameMapReleaseRequest(finesse_client_handle, &uuid, &message);
        (void)FinesseFreeNameMapResponse(finesse_client_handle, message);

        if (0 != status) {
            // Note that something went wrong with the server, maybe it
            // died?
            fprintf(stderr, "%s:%d failed with result %d for file %s\n", __FILE__, __LINE__, status, pathname);
            return file;
        }
    }

    if (0 != status) {
        // open succeeded but name map failed.
        return file;
    }

    // Open + lookup both worked, so we need to track the file descriptor
    // to uuid mapping.
    ffs = finesse_create_file_state(fileno(file), finesse_client_handle, &uuid, pathname);
    assert(NULL != ffs);  // if it failed, we'd need to release the name map

    return fin_fopen(pathname, mode);
}

FILE *finesse_fdopen(int fd, const char *mode)
{
    // TODO: do we need to trap this call at all?  The file descriptor already exists.
    return fin_fdopen(fd, mode);
}

FILE *finesse_freopen(const char *pathname, const char *mode, FILE *stream)
{
    // TODO: this is going to change the file descriptor; I'm not sure we are going to see the
    // close call here, or if it will bypass us.  So, that needs to be determined.
    // (1) keep track of the existing fd;
    // (2) send the new name request (if appropriate)
    // (3) Match up the results with the tracking table.
    //
    // Keep in mind that we have four cases here:
    //   old finesse name  -> new finesse name
    //   old finesse name  -> new non-finesse name
    //   old non-finesse name -> new finesse name
    //   old non-finesse name -> new non-finesse name
    //
    // We care about all but the last of those cases
    FILE *                  file   = NULL;
    int                     status = 0;
    uuid_t                  uuid;
    fincomm_message         message       = NULL;
    finesse_file_state_t *  ffs           = NULL;
    finesse_client_handle_t client_handle = NULL;
    int                     fd            = -1;

    // Need the fd, since this is an implicit close of the underlying file.
    fd = fileno(stream);

    // Get the existing state
    ffs = finesse_lookup_file_state(finesse_nfd_to_fd(fd));

    if (NULL == ffs) {
        // pass-through
        return fin_freopen(pathname, mode, stream);
    }

    // we have to tear this down
    finesse_delete_file_state(ffs);
    ffs = NULL;

    // Let's get the client handle for the new name
    client_handle = finesse_check_prefix(pathname);

    if (NULL != client_handle) {
        // Now start the name map
        memset(&uuid, 0, sizeof(uuid));
        status = FinesseSendNameMapRequest(client_handle, &uuid, pathname, &message);
    }

    // invoke the underlying library implementation
    file = fin_freopen(pathname, mode, stream);

    if ((NULL == client_handle) || (0 != status)) {
        return file;  // not of interest to us
    }

    // Name map request returned, so get the response.
    status = FinesseGetNameMapResponse(client_handle, message, &uuid);
    (void)FinesseFreeNameMapResponse(client_handle, message);

    if (0 != status) {
        // Name map failed
        return file;
    }

    assert(NULL != file);  // in this case we need to do the name map release

    // create state for this file
    ffs = finesse_create_file_state(fileno(file), client_handle, &uuid, pathname);
    assert(NULL != ffs);  // if it failed, we'd need to release the name map

    return fin_freopen(pathname, mode, stream);
}
