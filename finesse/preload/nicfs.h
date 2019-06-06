/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <aio.h>

extern void (*nicfs_init)(void);
extern void (*nicfs_shutdown)(void);

extern int nicfs_check_prefix(const char *name);


/*
 * The nicfs_XXX routines are signature matched versions of the XXX routines.  This permits invoking
 * them directly from an application (and test code) or patching them via LD_PRELOAD with a simple
 * wrapper.
 */
extern int nicfs_open(const char *pathname, int flags, ...);
extern int nicfs_creat(const char *pathname, mode_t mode);
extern int nicfs_openat(int dirfd, const char *pathname, int flags, ...);
extern int nicfs_close(int fd);
extern int nicfs_unlink(const char *pathname);
extern int nic_unlinkat(int dirfd, const char *pathname, int flags);

extern ssize_t nicfs_read(int fd, void *buf, size_t count);
extern ssize_t readahead(int fd, off64_t offset, size_t count);
extern ssize_t nicfs_pread(int fd, void *buf, size_t count, off_t offset);
extern ssize_t nicfs_pwrite(int fd, const void *buf, size_t count, off_t offset);
extern ssize_t nicfs_readv(int fd, const struct iovec *iov, int iovcnt);
extern ssize_t nicfs_preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset);
extern int nicfs_aio_read(struct aiocb *aiocbp);
extern ssize_t nicfs_write(int fd, const void *buf, size_t count);
extern ssize_t nicfs_writev(int fd, const struct iovec *iov, int iovcnt);
extern ssize_t nicfs_pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset);
