//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#define _GNU_SOURCE

#include <errno.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_ioctl(fuse_req_t req, fuse_ino_t ino, unsigned int cmd, void *arg, struct fuse_file_info *fi,
                                    unsigned flags, const void *in_buf, size_t in_bufsz, size_t out_bufsz);

void bitbucket_ioctl(fuse_req_t req, fuse_ino_t ino, unsigned int cmd, void *arg, struct fuse_file_info *fi, unsigned flags,
                     const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
    struct timespec start, stop, elapsed;
    int             status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);
    status  = bitbucket_internal_ioctl(req, ino, cmd, arg, fi, flags, in_buf, in_bufsz, out_bufsz);
    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    BitbucketCountCall(BITBUCKET_CALL_IOCTL, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_ioctl(fuse_req_t req, fuse_ino_t ino, unsigned int cmd, void *arg, struct fuse_file_info *fi,
                                    unsigned flags, const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
    (void)req;
    (void)ino;
    (void)cmd;
    (void)flags;
    (void)in_buf;
    (void)in_bufsz;
    (void)out_bufsz;
    (void)arg;
    (void)fi;

    // At least for now, we don't support any IOCTLs
    fuse_reply_err(req, EINVAL);

    return 0;
}
