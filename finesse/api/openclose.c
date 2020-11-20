/*
 * (C) Copyright 2018 Tony Mason
 * All Rights Reserved
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "api-internal.h"
#include "callstats.h"

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

static int internal_open(const char *pathname, int flags, mode_t mode)
{
    int                     fd;
    int                     status;
    uuid_t                  uuid;
    fincomm_message         message       = NULL;
    finesse_client_handle_t client_handle = NULL;
    finesse_file_state_t *  ffs           = NULL;
    DECLARE_TIME(FINESSE_API_CALL_OPEN)

    // Note: for now, we shunt create operations (though we should deal with them when we can)
    if (O_CREAT & flags) {
        START_TIME

        status = fin_open(pathname, flags, mode);

        STOP_NATIVE_TIME

        // TODO: if the create is successful, we should name map it here and add it to our tracking
        // database.
        fprintf(stderr, "%s: open(%s) with O_CREAT (skipped)\n", __func__, pathname);

        return status;
    }

    //
    // Let's see if it makes sense for us to try opening this
    //
    START_TIME

    client_handle = finesse_check_prefix(pathname);

    STOP_FINESSE_TIME

    if (NULL == client_handle) {
        // not of interest
        START_TIME

        status = fin_open(pathname, flags, mode);

        STOP_NATIVE_TIME

        return status;
    }

    //
    // Ask the Finesse server
    //
    memset(&uuid, 0, sizeof(uuid));
    status = FinesseSendNameMapRequest(client_handle, &uuid, (char *)(uintptr_t)pathname, flags, &message);

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
    ffs = finesse_create_file_state(fd, client_handle, &uuid, pathname, flags);
    assert(NULL != ffs);  // if it failed, we'd need to release the name map

    return finesse_fd_to_nfd(fd);
}

int finesse_open(const char *pathname, int flags, ...)
{
    mode_t  mode   = 0;
    int     result = -1;
    va_list args;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    // During initialization, we have to pass this through - we cannot
    // handle it.
    if (finesse_api_init_in_progress) {
        return fin_open(pathname, flags, mode);
    }

    result = internal_open(pathname, flags, mode);

    FinesseApiCountCall(FINESSE_API_CALL_OPEN, (result >= 0));

    return result;
}

int finesse_creat(const char *pathname, mode_t mode)
{
    int fd = finesse_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);

    FinesseApiCountCall(FINESSE_API_CALL_CREAT, (fd >= 0));

    return finesse_fd_to_nfd(fd);
}

static int internal_openat(int dirfd, const char *pathname, int flags, mode_t mode)
{
    int                   fd = -1;
    int                   status;
    uuid_t                uuid;
    fincomm_message       message    = NULL;
    finesse_file_state_t *ffs        = NULL;
    size_t                cwd_length = PATH_MAX;
    char *                cwdbuf     = NULL;
    DECLARE_TIME(FINESSE_API_CALL_OPENAT);
    static int handled_flags =
        O_RDONLY | O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_NOCTTY | O_NONBLOCK | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC;

    while (1) {
        // flags = 2604400
        //      0400 = O_NOCTTY     - do not make this the controlling TTY
        //     04000 = O_NONBLOCK   - non-blocking open (and fd)
        //   0200000 = O_DIRECTORY  - must be a directory (should be used by opendir)
        //   0400000 = O_NOFOLLOW   - do not follow symbolic links
        //  02000000 = O_CLOEXEC    - close this handle on an exec (not inherited)
        //
        // List of flags:
        //        01 - O_WRONLY
        //        02 - O_RDWR  (oddly 00 is O_RDONLY)
        //      0100 - O_CREAT
        //      0200 - O_EXCL
        //      0400 = O_NOCTTY     - do not make this the controlling TTY
        //     01000 - O_TRUNC
        //     02000 - O_APPEND
        //     04000 = O_NONBLOCK   - non-blocking open (and fd)
        //    010000 - O_DSYNC
        //    020000 - O_ASYNC
        //    040000 - O_DIRECT
        //   0100000 - O_LARGEFILE
        //   0200000 = O_DIRECTORY  - must be a directory (should be used by opendir)
        //   0400000 = O_NOFOLLOW   - do not follow symbolic links
        //  01000000 - O_NOATIME
        //  02000000 = O_CLOEXEC    - close this handle on an exec (not inherited)
        //  04010000 - O_SYNC
        // 010000000 - O_PATH
        // 020200000 - O_TMPFILE
        //

        // Flags we don't handle at this point can just be shunted to the
        // fallback path.
        if (0 != (flags & ~handled_flags)) {
            START_TIME

            fd = fin_openat(dirfd, pathname, flags, mode);

            STOP_NATIVE_TIME;

            fprintf(stderr, "%s: openat(%s) skipped due to flags %d\n", __func__, pathname, flags);

            break;
        }

        if (AT_FDCWD == dirfd) {
            if ('/' == *pathname) {
                fd = internal_open(pathname, flags, mode);
                break;
            }

            // This is relative to the CWD value, so let's grab that
            cwdbuf = malloc(cwd_length);

            while ((NULL != cwdbuf) && (NULL == getcwd(cwdbuf, cwd_length))) {
                free(cwdbuf);
                if (ERANGE == errno) {
                    cwd_length *= 2;
                    cwdbuf = malloc(cwd_length);
                }
            }

            if (NULL == cwdbuf) {  // allocation failed?
                // fall back
                START_TIME

                fd = fin_openat(dirfd, pathname, flags, mode);

                STOP_NATIVE_TIME;

                break;
            }

            // At this point I have the cwd: I'm going to have to concatentate them.
            size_t cwdl = strlen(cwdbuf);
            size_t pnl  = strlen(pathname);
            assert((cwdl + pnl + 1) < cwd_length);  // otherwise, add the code to make it big enough.  Bleh...
            if ((cwdbuf[cwdl - 1] != '/') && (pathname[0] != '/')) {
                strcat(cwdbuf, "/");
            }

            strcat(cwdbuf, pathname);

            fd = internal_open(cwdbuf, flags, mode);

            break;
        }

        START_TIME
        // otherwise, this is a file descriptor, so let's go see if it is one we are tracking.
        ffs = finesse_lookup_file_state(finesse_nfd_to_fd(dirfd));

        fprintf(stderr, "%s: openat(%s in %d) lookup 0x%p\n", __func__, pathname, dirfd, (void *)ffs);

        STOP_FINESSE_TIME

        if (NULL == ffs) {
            START_TIME
            // Don't care about this one
            fd = fin_openat(dirfd, pathname, flags, mode);

            STOP_NATIVE_TIME

            break;
        }

        START_TIME

        // We DO care about this one
        status = FinesseSendNameMapRequest(ffs->client, &ffs->key, pathname, flags, &message);
        assert(0 == status);

        STOP_FINESSE_TIME

        // XXX: it is possible these are going to race, so the interleaving won't work
        START_TIME
        fd = fin_openat(dirfd, pathname, flags, mode);
        STOP_NATIVE_TIME

        START_TIME
        status = FinesseGetNameMapResponse(ffs->client, message, &uuid);
        assert(0 == status);
        status = message->Result;
        FinesseFreeNameMapResponse(ffs->client, message);

        STOP_FINESSE_TIME

        if (0 != status) {
            // just use the native fd
            break;
        }

        assert(fd >= 0);  // otherwise, the Finesse call WORKED and the kernel call failed - TODO

        // TODO: track full path name, not just the relative one
        finesse_file_state_t *ffs2 = finesse_create_file_state(fd, ffs->client, &uuid, pathname, flags);
        assert(NULL != ffs2);

        // Done
        break;
    }

    if (NULL != cwdbuf) {
        free(cwdbuf);
        cwdbuf = NULL;
    }

    return fd;
}

int finesse_openat(int dirfd, const char *pathname, int flags, ...)
{
    int     status = -1;
    va_list args;
    mode_t  mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    status = internal_openat(dirfd, pathname, flags, mode);

    FinesseApiCountCall(FINESSE_API_CALL_OPENAT, (status >= 0));

    return status;
}

int finesse_close(int fd)
{
    finesse_file_state_t *file_state = NULL;
    int                   status     = 0;

    // During initialization, we cannot handle this call,
    // we have to pass it through.
    if (finesse_api_init_in_progress) {
        return fin_close(fd);
    }

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

static int fopen_mode_to_flags(const char *mode)
{
    int flags = -1;

    // first character is the primary mode
    switch (*mode) {
        case 'r':
            flags = O_RDONLY;
            break;
        case 'w':
            flags = O_WRONLY;
            break;
        case 'a':
            flags = O_WRONLY | O_CREAT | O_APPEND;
            break;
        default:
            errno = EINVAL;
            return flags;
    }

    for (unsigned index = 0; index < 7; index++) {  // 7 is a magic value from glibc
        if ('\0' == *(mode + index)) {
            break;  // NULL terminated
        }
        switch (*(mode + index)) {
            default:  // ignore
                continue;
                break;
            case '+':
                flags |= O_RDWR;
                break;
            case 'x':
                flags |= O_EXCL;
                break;
            case 'b':
            case 't':
                // no-op on UNIX/Linux
                break;
            case 'm':
                // memory mapped - doesn't matter
                break;
            case 'c':
                // internal to libc
                break;
            case 'e':
                flags |= O_CLOEXEC;
                break;
        }
    }

    return flags;
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
    int                     flags;

    finesse_client_handle = finesse_check_prefix(pathname);

    if (NULL == finesse_client_handle) {
        // not of interest
        return fin_fopen(pathname, mode);
    }

    flags = fopen_mode_to_flags(mode);
    assert(-1 != flags);  // otherwise, it's an error

    //
    // Ask the other side
    //
    memset(&uuid, 0, sizeof(uuid));
    status = FinesseSendNameMapRequest(finesse_client_handle, &uuid, (char *)(uintptr_t)pathname, flags, &message);

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
    ffs = finesse_create_file_state(fileno(file), finesse_client_handle, &uuid, pathname, flags);
    assert(NULL != ffs);  // if it failed, we'd need to release the name map

    return fin_fopen(pathname, mode);
}

FILE *finesse_fdopen(int fd, const char *mode)
{
    // TODO: do we need to trap this call at all?  The file descriptor already exists.
    return fin_fdopen(fd, mode);
}

static FILE *internal_freopen(const char *pathname, const char *mode, FILE *stream)
{
    struct timespec         start, stop, elapsed;
    int                     status, tstatus;
    fincomm_message         message;
    finesse_file_state_t *  ffs  = NULL;
    FILE *                  file = NULL;
    uuid_t                  uuid;
    finesse_client_handle_t client_handle = NULL;
    int                     fd            = -1;
    int                     flags         = 0;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

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
    // Need the fd, since this is an implicit close of the underlying file.
    fd = fileno(stream);

    // Get the existing state
    ffs = finesse_lookup_file_state(finesse_nfd_to_fd(fd));

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_FREOPEN, &elapsed);

    if (NULL == ffs) {
        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        assert(0 == tstatus);

        // pass-through
        file = fin_freopen(pathname, mode, stream);

        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        assert(0 == tstatus);
        timespec_diff(&start, &stop, &elapsed);
        FinesseApiRecordNative(FINESSE_API_CALL_FREOPEN, &elapsed);

        return file;
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    // save the original flags
    flags = ffs->flags;

    // we have to tear this down
    finesse_delete_file_state(ffs);
    ffs = NULL;

    // Let's get the client handle for the new name
    client_handle = finesse_check_prefix(pathname);
    status        = -1;

    if (NULL != client_handle) {
        // Now start the name map
        memset(&uuid, 0, sizeof(uuid));
        status = FinesseSendNameMapRequest(client_handle, &uuid, pathname, flags, &message);
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_FREOPEN, &elapsed);

    // While the name map request is being handled, let's run the native API

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_FSTATAT, &elapsed);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    // invoke the underlying library implementation
    file = fin_freopen(pathname, mode, stream);

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordNative(FINESSE_API_CALL_FREOPEN, &elapsed);

    if ((NULL != client_handle) && (0 == status)) {
        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        assert(0 == tstatus);

        // Name map request returned, so get the response.
        status = FinesseGetNameMapResponse(client_handle, message, &uuid);
        FinesseFreeNameMapResponse(client_handle, message);

        tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        assert(0 == tstatus);
        timespec_diff(&start, &stop, &elapsed);
        FinesseApiRecordOverhead(FINESSE_API_CALL_FSTATAT, &elapsed);
    }

    if ((NULL == client_handle) || (0 != status)) {
        return file;  // not of interest to us
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);

    if (0 == status) {
        // create state for this file
        ffs = finesse_create_file_state(fileno(file), client_handle, &uuid, pathname, flags);
        assert(NULL != ffs);  // if it failed, we'd need to release the name map
    }

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    FinesseApiRecordOverhead(FINESSE_API_CALL_FSTATAT, &elapsed);

    return file;
}

FILE *finesse_freopen(const char *pathname, const char *mode, FILE *stream)
{
    FILE *file = internal_freopen(pathname, mode, stream);

    FinesseApiCountCall(FINESSE_API_CALL_FREOPEN, (NULL != file));

    return file;
}
