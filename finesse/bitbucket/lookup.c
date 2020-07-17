//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include <errno.h>
#include <string.h>
#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);

void bitbucket_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    struct timespec start, stop, elapsed;
    int             status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);
    status  = bitbucket_internal_lookup(req, parent, name);
    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    BitbucketCountCall(BITBUCKET_CALL_LOOKUP, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    void *                  userdata    = fuse_req_userdata(req);
    bitbucket_userdata_t *  BBud        = (bitbucket_userdata_t *)userdata;
    bitbucket_inode_t *     parentInode = NULL;
    bitbucket_inode_t *     inode       = NULL;
    struct fuse_entry_param fep;
    int                     status = 0;

    CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

    if (FUSE_ROOT_ID == parent) {
        parentInode = BBud->RootDirectory;
        BitbucketReferenceInode(parentInode, INODE_LOOKUP_REFERENCE);
    }
    else {
        parentInode = BitbucketLookupInodeInTable(BBud->InodeTable, parent);
    }

    if (NULL == parentInode) {
        fuse_reply_err(req, EBADF);
        return EBADF;
    }

    BitbucketLookupObjectInDirectory(parentInode, name, &inode);

    if (NULL == inode) {
        fuse_reply_err(req, ENOENT);
        status = ENOENT;
    }
    else {
        memset(&fep, 0, sizeof(fep));
        fep.ino           = inode->Attributes.st_ino;
        fep.generation    = inode->Epoch;
        fep.attr          = inode->Attributes;
        fep.attr_timeout  = BBud->AttrTimeout;
        fep.entry_timeout = BBud->AttrTimeout;

        BitbucketReferenceInode(inode, INODE_FUSE_LOOKUP_REFERENCE);  // this matches the fuse_reply_entry
        fuse_reply_entry(req, &fep);

        if (BBud->Debug) {
            fuse_log(FUSE_LOG_DEBUG, "  %lli/%s -> %lli\n", (unsigned long long)parent, name,
                     (unsigned long long)inode->Attributes.st_ino);
        }
    }

    if (NULL != parentInode) {
        BitbucketDereferenceInode(parentInode, INODE_LOOKUP_REFERENCE, 1);
        parentInode = NULL;
    }

    if (NULL != inode) {
        BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);
        inode = NULL;
    }

    return status;
}
