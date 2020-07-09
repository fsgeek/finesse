//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include <errno.h>
#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);

void bitbucket_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    struct timespec start, stop, elapsed;
    int             status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);
    status  = bitbucket_internal_release(req, ino, fi);
    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    BitbucketCountCall(BITBUCKET_CALL_RELEASE, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    void *                userdata = fuse_req_userdata(req);
    bitbucket_userdata_t *BBud     = (bitbucket_userdata_t *)userdata;
    bitbucket_inode_t *   inode    = NULL;
    int                   status   = 0;

    (void)fi;

    // TODO: should we be doing anything with the flags in fi->flags?

    CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

    if (FUSE_ROOT_ID == ino) {
        inode = BBud->RootDirectory;
        BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);  // must have a lookup ref.
    }
    else {
        inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);  // returns a lookup ref.
    }

    //
    // For now there's not much to do since we don't maintain any specific open instance state
    // We might want to do that (e.g., we could use the file handle in the fi structure to point
    // to some useful state if we wanted).
    //
    status = EBADF;

    if (NULL != inode) {
        status = 0;                                                      // success
        BitbucketDereferenceInode(inode, INODE_FUSE_OPEN_REFERENCE, 1);  // explicit open, implicit open for create
        BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);
        inode = NULL;
    }

    fuse_reply_err(req, status);

    return status;
}
