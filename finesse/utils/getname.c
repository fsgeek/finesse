/*
 * Copyright (c) 2020 Tony Mason
 * All rights reserved.
 */

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif  // _GNU_SOURCE

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <finesse-fuse.h>
#include <fuse_lowlevel.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "finesse-utils.h"

// utility function to get a path name for a file descriptor
// Note: this is going to be system dependent and can fail
// because the name has changed.
// boolean return: 0 means failed, anything else means success
int finesse_get_name_from_fd(int fd, char *buffer, size_t buffer_length)
{
    char   procbuf[PATH_MAX];
    size_t len;

    if (AT_FDCWD == fd) {
        // just get the current directory
        return getcwd(buffer, buffer_length) ? 1 : 0;
    }

    if (buffer_length < PATH_MAX) {
        // Let the caller fix this
        errno = EOVERFLOW;
        return -1;
    }

    snprintf(procbuf, sizeof(procbuf), "/proc/self/fd/%d", fd);

    len = readlink(procbuf, buffer, buffer_length);

    assert(len < buffer_length);  // overflow?

    return 1;
}
