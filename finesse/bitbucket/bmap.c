//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_bmap(fuse_req_t req, fuse_ino_t ino, size_t blocksize, uint64_t idx);

void bitbucket_bmap(fuse_req_t req, fuse_ino_t ino, size_t blocksize, uint64_t idx)
{
    struct timespec start, stop, elapsed;
    int             status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);
    status  = bitbucket_internal_bmap(req, ino, blocksize, idx);
    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    BitbucketCountCall(BITBUCKET_CALL_BMAP, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_bmap(fuse_req_t req, fuse_ino_t ino, size_t blocksize, uint64_t idx)
{
    (void)req;
    (void)ino;
    (void)blocksize;
    (void)idx;

    assert(0);  // Not implemented
}
