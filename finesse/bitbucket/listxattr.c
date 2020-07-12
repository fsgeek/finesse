//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size);

void bitbucket_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
    struct timespec start, stop, elapsed;
    int             status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);
    status  = bitbucket_internal_listxattr(req, ino, size);
    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    BitbucketCountCall(BITBUCKET_CALL_LISTXATTR, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
    (void)req;
    (void)ino;
    (void)size;

    assert(0);  // Not implemented
}
