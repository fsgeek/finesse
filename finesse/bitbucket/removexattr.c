//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include <errno.h>
#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name);

void bitbucket_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
    struct timespec start, stop, elapsed;
    int             status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);
    status  = bitbucket_internal_removexattr(req, ino, name);
    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    BitbucketCountCall(BITBUCKET_CALL_REMOVEXATTR, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
    void *                userdata = fuse_req_userdata(req);
    bitbucket_userdata_t *BBud     = (bitbucket_userdata_t *)userdata;
    bitbucket_inode_t *   inode    = NULL;
    size_t                length;
    const void *          data;
    int                   status = EBADF;

    if (BBud->NoXattr) {
        fuse_reply_err(req, ENOSYS);
        return ENOSYS;
    }

    CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

    if (FUSE_ROOT_ID == ino) {
        inode = BBud->RootDirectory;
        BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
    }
    else {
        inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);
    }

    while (NULL != inode) {
        BitbucketLockInode(inode, 0);
        status = BitbucketLookupExtendedAttribute(inode, name, &length, &data);

        if (ENOENT == status) {
            status = ENODATA;
            break;
        }

        status = BitbucketRemoveExtendedAttribute(inode, name);
        break;
    }

    fuse_reply_err(req, status);

    if (NULL != inode) {
        BitbucketUnlockInode(inode);
        BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);
        inode = NULL;
    }

    return status;
}
