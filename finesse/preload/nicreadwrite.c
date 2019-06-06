/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#include "nicfs.h"

// This is under development, so there are stubs.
#pragma GCC diagnostic ignored "-Wunused-parameter"

/* 
 * REF: https://rafalcieslak.wordpress.com/2013/04/02/dynamic-linker-tricks-using-ld_preload-to-cheat-inject-features-and-investigate-programs/
 *      https://github.com/poliva/ldpreloadhook/blob/master/hook.c
 */

static int nic_read(int fd, void *buf, size_t count)
{
    typedef int (*orig_read_t)(int fd, void *buf, size_t count);
    static orig_read_t orig_read = NULL;
    
    if (NULL == orig_read) {
        orig_read = (orig_read_t) dlsym(RTLD_NEXT, "read");

        assert(NULL != orig_read);
        if (NULL == orig_read) {
            return EACCES;
        }
    }

    return orig_read(fd, buf, count);
}

static int nic_write(int fd, const void *buf, size_t count)
{
    typedef int (*orig_write_t)(int fd, const void *buf, size_t count);
    static orig_write_t orig_write = NULL;

    if (NULL == orig_write) {
        orig_write = (orig_write_t) dlsym(RTLD_NEXT, "write");

        assert(NULL != orig_write);
        if (NULL == orig_write) {
            return EACCES;
        }
    }

    return orig_write(fd, buf, count);
}

/* 
    NICFS_REQ_READ,
    NICFS_REQ_READAHEAD,
    NICFS_REQ_READDIR,
    NICFS_REQ_READLINK,
    NICFS_REQ_READLINKAT,
    NICFS_REQ_READV,

 */

ssize_t nicfs_read(int fd, void *buf, size_t count)
{
    /* TODO: satisfy this from SHM if possible */
    return nic_read(fd, buf, count);
}

#if 0
ssize_t readahead(int fd, off64_t offset, size_t count)
{
    assert(0);
    /* TODO: implement this */
    return ENOTIMPL;
}
#endif // 0

ssize_t nicfs_pread(int fd, void *buf, size_t count, off_t offset)
{
    assert(0);
    return -1;
}

ssize_t nicfs_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    assert(0);
    return -1;
}

ssize_t nicfs_readv(int fd, const struct iovec *iov, int iovcnt)
{
    assert(0);
    return -1;
}

ssize_t nicfs_preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    assert(0);
    return -1;
}

int nicfs_aio_read(struct aiocb *aiocbp)
{
    assert(0);
    return -1;
}

ssize_t nicfs_write(int fd, const void *buf, size_t count)
{
    /* TODO: satisfy this via SHM if possible */
    return nic_write(fd, buf, count);
}

ssize_t nicfs_writev(int fd, const struct iovec *iov, int iovcnt)
{
    assert(0);
    return -1;
}


ssize_t nicfs_pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    assert(0);
    return -1;
}

