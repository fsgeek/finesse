//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include <errno.h>
#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_unlink(fuse_req_t req, fuse_ino_t parent, const char *name);

void bitbucket_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    struct timespec start, stop, elapsed;
    int             status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);
    status  = bitbucket_internal_unlink(req, parent, name);
    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    BitbucketCountCall(BITBUCKET_CALL_UNLINK, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    void *                userdata = fuse_req_userdata(req);
    bitbucket_userdata_t *BBud     = (bitbucket_userdata_t *)userdata;
    bitbucket_inode_t *   inode    = NULL;
    int                   status   = EBADF;

    CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

    if (FUSE_ROOT_ID == parent) {
        inode = BBud->RootDirectory;
        BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
    }
    else {
        inode = BitbucketLookupInodeInTable(BBud->InodeTable, parent);
    }

    while (NULL != inode) {
        if (BITBUCKET_DIR_TYPE != inode->InodeType) {
            status = ENOTDIR;
            break;
        }

        status = BitbucketDeleteDirectoryEntry(inode, name);
        break;
    }

    fuse_reply_err(req, status);

    if (NULL != inode) {
        BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);
    }

    return status;
}
