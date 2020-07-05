//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include <errno.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags);

void bitbucket_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags)
{
    struct timespec start, stop, elapsed;
    int             status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);
    status  = bitbucket_internal_setxattr(req, ino, name, value, size, flags);
    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    BitbucketCountCall(BITBUCKET_CALL_SETXATTR, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags)
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

    // Three options here:
    // (1) flags = 0 implies we're setting the value
    // (2) flags = XATTR_CREATE implies we fail if it already exists
    // (3) flags = XATTR_REPLACE implies we fail if it doesn't exist
    //
    // So for case (1) I'll delete an entry that already exists and proceed
    // For case (2) I'll _fail_ if an entry already exists and proceed
    // For case (3) I'll _fail_ if an entry doesn't already exist proceed
    while (NULL != inode) {
        BitbucketLockInode(inode, 0);
        status = BitbucketLookupExtendedAttribute(inode, name, &length, &data);

        if (0 == status) {
            if (0 != (flags & XATTR_CREATE)) {
                // case (2)
                status = EEXIST;
                break;
            }
            // Otherwise, let's remove this entry and proceed (all cases)
            status = BitbucketRemoveExtendedAttribute(inode, name);
            assert(0 == status);  // we don't have any graceful recovery here.
        }
        else if (ENOENT == status) {
            if (0 != (flags & XATTR_REPLACE)) {
                // case (3)
                status = ENODATA;
                break;
            }
        }
        else {
            // unexpected error state
            break;
        }

        // Create the entry
        status = BitbucketInsertExtendedAttribute(inode, name, size, value);
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
