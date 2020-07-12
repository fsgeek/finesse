//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include <errno.h>
#include <string.h>
#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_write_buf(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *bufv, off_t off,
                                        struct fuse_file_info *fi);

void bitbucket_write_buf(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *bufv, off_t off, struct fuse_file_info *fi)
{
    struct timespec start, stop, elapsed;
    int             status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);
    status  = bitbucket_internal_write_buf(req, ino, bufv, off, fi);
    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    BitbucketCountCall(BITBUCKET_CALL_WRITE_BUF, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_write_buf(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *bufv, off_t off,
                                        struct fuse_file_info *fi)
{
    bitbucket_userdata_t *BBud;
    bitbucket_inode_t *   inode     = NULL;
    int                   status    = 0;
    size_t                size      = 0;
    size_t                offset    = 0;
    int                   extending = 0;

    (void)fi;  // could probably just use fi here...

    BBud = (bitbucket_userdata_t *)fuse_req_userdata(req);
    assert(NULL != BBud);
    CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

    // Compute the size
    if (NULL != bufv) {
        for (unsigned index = 0; index < bufv->count; index++) {
            if (bufv->idx >= index) {
                size += bufv->buf[index].size;
            }
        }
    }

    if (0 == size) {
        // zero byte writes always succeed
        fuse_reply_write(req, 0);
        return size;
    }

    // TODO: should we be doing anything with the flags in fi->flags?

    if (FUSE_ROOT_ID == ino) {
        inode = BBud->RootDirectory;
        BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);  // must have a lookup ref.
    }
    else {
        inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);  // returns a lookup ref.
    }

    assert(NULL != inode);
    CHECK_BITBUCKET_INODE_MAGIC(inode);

    //
    // This is what makes the file system a bit bucket: writes are discarded (with success, of course)
    //
    status = EBADF;

    while (NULL != inode) {
        if (BITBUCKET_FILE_TYPE != inode->InodeType) {
            status = EINVAL;
            break;
        }

        status = 0;  // success

        BitbucketLockInode(inode, 0);
        if (off + size > inode->Attributes.st_size) {
            extending = 1;
            BitbucketUnlockInode(inode);
            BitbucketLockInode(inode, 1);
        }

        if (off + size > inode->Attributes.st_size) {
            assert(extending);  // otherwise this is broken!
            status = BitbucketAdjustFileStorage(inode, off + size);
            assert(0 == status);  // probably a programming bug
            // move the EOF pointer out.
            inode->Attributes.st_size   = off + size;
            inode->Attributes.st_blocks = inode->Attributes.st_size / inode->Attributes.st_blksize;
        }

        // At this point it is safe for us to copy data
        assert(NULL != bufv);
        offset = off;
        for (unsigned index = 0; index < bufv->count; index++) {
            if (bufv->idx >= index) {
                char *ptr = (char *)((uintptr_t)inode->Instance.File.Map);
                assert(0 == bufv->buf[index].flags);  // we aren't using any flags...
                assert((uintptr_t)(offset + bufv->buf[index].size) <= (uintptr_t)inode->Attributes.st_size);
                memcpy(ptr + offset, bufv->buf[index].mem, bufv->buf[index].size);
                offset += bufv->buf[index].size;
            }
        }
        BitbucketUnlockInode(inode);

        BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);
        inode = NULL;
    }

    if (0 == status) {
        fuse_reply_write(req, size);
    }
    else {
        fuse_reply_err(req, status);
    }

    return status;
}
