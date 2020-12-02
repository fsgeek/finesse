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

// utility function to get a path name for a file descriptor
// Note: this is going to be system dependent and can fail
// because the name has changed.
int finesse_get_name_from_fd(int fd, char *buffer, size_t buffer_length)
{
    char procbuf[PATH_MAX];

    if (AT_FDCWD == fd) {
        // just get the current directory
        return getcwd(buffer, buffer_length) ? NULL : -1 : 0;
    }

    size_t len;

    snprintf(procbuf, sizeof(procbuf), "/proc/self/fd/%d", fd);

    len = readlink(procbuf, buffer, buffer_length);

    if (buffer_length < PATH_MAX) {
        // Let the caller fix this
        errno = EINVAL;
        return -1;
    }

    return fcntl(fd, F_GETPATH, buffer);
}
