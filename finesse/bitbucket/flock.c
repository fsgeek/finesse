//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include <errno.h>
#include <malloc.h>
#include <sys/file.h>
#include "bitbucket.h"
#include "bitbucketcalls.h"

typedef struct _bitbucket_flock_entry {
    uint64_t     Magic;
    fuse_req_t   Req;
    pid_t        OwnerPid;
    uint8_t      Exclusive;  // 0 = read, 1 = write
    list_entry_t ListEntry;
} bitbucket_flock_entry_t;

#define BITBUCKET_FLOCK_ENTRY_MAGIC (0xdf5bcef89bea083b)
#define CHECK_BITBUCKET_FLOCK_ENTRY_MAGIC(bbfe) \
    verify_magic("bitbucket_flock_entry_t", __FILE__, __func__, __LINE__, BITBUCKET_FLOCK_ENTRY_MAGIC, (bbfe)->Magic)

static int bitbucket_internal_flock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, int op);

void bitbucket_flock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, int op)
{
    struct timespec start, stop, elapsed;
    int             status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);
    status  = bitbucket_internal_flock(req, ino, fi, op);
    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    BitbucketCountCall(BITBUCKET_CALL_FLOCK, status ? 0 : 1, &elapsed);
}

//
// This routine is used to clean up any outstanding flock structures
//
void bitbucket_cleanup_flock(bitbucket_inode_t *Inode);

void bitbucket_cleanup_flock(bitbucket_inode_t *Inode)
{
    list_entry_t *           le         = NULL;
    bitbucket_flock_entry_t *flockEntry = NULL;

    assert(NULL != Inode);
    CHECK_BITBUCKET_INODE_MAGIC(Inode);

    while (BITBUCKET_FILE_TYPE == Inode->InodeType) {
        BitbucketLockInode(Inode, 1);
        while (!empty_list(&Inode->Instance.File.LockWaitersList)) {
            le = remove_list_head(&Inode->Instance.File.LockWaitersList);
            assert(NULL != le);
            flockEntry = container_of(le, bitbucket_flock_entry_t, ListEntry);
            CHECK_BITBUCKET_FLOCK_ENTRY_MAGIC(flockEntry);

            fuse_reply_err(flockEntry->Req, EBADF);  // not an open FD now...
            flockEntry->Req = NULL;
            if (flockEntry->Exclusive) {
                assert(Inode->Instance.File.WaitingWriters > 0);
                Inode->Instance.File.WaitingWriters--;
            }
            else {
                assert(Inode->Instance.File.WaitingReaders > 0);
                Inode->Instance.File.WaitingReaders--;
            }
            free(flockEntry);
            flockEntry = NULL;
        }
        assert(empty_list(&Inode->Instance.File.LockOwnersList));  // deal with it if it happens
        BitbucketUnlockInode(Inode);
        break;
    }
}

//
// Local helper routine for unlocking the lock(s) owned by the given process
//
static int bitbucket_flock_unlock(bitbucket_inode_t *Inode, pid_t ProcessId)
{
    list_entry_t *           le         = NULL;
    bitbucket_flock_entry_t *fe         = NULL;
    unsigned                 count      = 0;
    unsigned                 exclusives = 0;
    int                      status     = 0;

    while (1) {
        // Do this manually because we're changing the list as we permute it.
        // Move's all flocks for the give PID to a separate list
        for (le = list_head(&Inode->Instance.File.LockOwnersList); le != &Inode->Instance.File.LockOwnersList;) {
            list_entry_t *nle = le->next;

            fe = container_of(le, bitbucket_flock_entry_t, ListEntry);
            CHECK_BITBUCKET_FLOCK_ENTRY_MAGIC(fe);

            if (fe->OwnerPid == ProcessId) {
                assert(exclusives < 2);  // otherwise it's not very exclusive
                remove_list_entry(&fe->ListEntry);
                count++;

                if (fe->Exclusive) {
                    exclusives++;
                    assert(1 == Inode->Instance.File.Writers);  // can't have more than 1
                    Inode->Instance.File.Writers--;
                }
                else {
                    assert(Inode->Instance.File.Readers >= count);
                    Inode->Instance.File.Readers--;
                }

                free(fe);
                fe = NULL;
            }
            // Now advance to the next entry
            le = nle;
        }

        assert(count > 0);  // why would we not be getting a valid unlock request?
        assert((0 == exclusives) || ((0 == Inode->Instance.File.Readers) && (0 == Inode->Instance.File.Writers)));

        if ((0 == Inode->Instance.File.WaitingReaders) && (0 == Inode->Instance.File.WaitingWriters)) {
            // Nothing more to do
            status = 0;
            break;
        }

        // Look at the next entry
        le = list_head(&Inode->Instance.File.LockWaitersList);
        fe = container_of(le, bitbucket_flock_entry_t, ListEntry);

        CHECK_BITBUCKET_FLOCK_ENTRY_MAGIC(fe);

        count = Inode->Instance.File.Readers + Inode->Instance.File.Writers;
        if (fe->Exclusive) {
            if (count > 0) {
                // can't do anything further with this one
                status = 0;
                break;
            }

            // We can grant this lock; it becomes the exclusive owner
            Inode->Instance.File.Writers++;
            Inode->Instance.File.WaitingWriters--;
            remove_list_entry(&fe->ListEntry);
            fuse_reply_err(fe->Req, 0);  // unblock and grant
            status = 0;
            break;
        }

        // This is a shared lock - there shouldn't be any issue
        // granting it (e.g., the lock can't be owned exclusive).
        assert(0 == Inode->Instance.File.Writers);

        while (NULL != fe) {
            if (fe->Exclusive) {
                // We stop here
                fe     = NULL;
                status = 0;
                break;
            }

            Inode->Instance.File.Readers++;
            Inode->Instance.File.WaitingReaders--;
            remove_list_entry(&fe->ListEntry);
            fuse_reply_err(fe->Req, 0);  // unblock and grant
            free(fe);

            le = list_head(&Inode->Instance.File.LockWaitersList);
            fe = container_of(le, bitbucket_flock_entry_t, ListEntry);
        }

        // At this point we've allowed as many lock requests go as possible.
        // Let's do a quick sanity check: if we don't have any current owners
        // we can't have any waiters.
        count = Inode->Instance.File.Readers + Inode->Instance.File.Writers;
        assert((0 != count) || ((0 == Inode->Instance.File.WaitingReaders) && (0 == Inode->Instance.File.WaitingWriters)));

        // done
        break;
    }

    return status;
}

static int bitbucket_internal_flock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, int op)
{
    int                      status      = EBADF;
    void *                   userdata    = fuse_req_userdata(req);
    bitbucket_userdata_t *   BBud        = (bitbucket_userdata_t *)userdata;
    bitbucket_inode_t *      inode       = NULL;
    bitbucket_flock_entry_t *flockEntry  = NULL;
    const struct fuse_ctx *  fuseContext = fuse_req_ctx(req);

    (void)fi;

    CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

    if (FUSE_ROOT_ID == ino) {
        inode = BBud->RootDirectory;
        BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
    }
    else {
        inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);
    }

    while (NULL != inode) {
        if (BITBUCKET_FILE_TYPE != inode->InodeType) {
            status = EINVAL;
            break;
        }

        if (LOCK_UN != op) {  // not an unlock request --> lock request
            flockEntry = malloc(sizeof(bitbucket_flock_entry_t));
            if (NULL == flockEntry) {
                status = ENOMEM;
                break;
            }
            flockEntry->Magic     = BITBUCKET_FLOCK_ENTRY_MAGIC;
            flockEntry->Req       = req;
            flockEntry->OwnerPid  = fuseContext->pid;
            flockEntry->Exclusive = 0;
        }

        BitbucketLockInode(inode, 1);
        switch (op) {
            default: {
                status = EINVAL;
                break;
            }
            case LOCK_SH: {
                if ((inode->Instance.File.Writers > 0) || (inode->Instance.File.WaitingWriters > 0)) {
                    inode->Instance.File.WaitingReaders++;
                    // Insert at tail, since we will scan from head
                    insert_list_tail(&inode->Instance.File.LockWaitersList, &flockEntry->ListEntry);
                    flockEntry = NULL;
                }
                else {
                    inode->Instance.File.Readers++;
                    flockEntry->Req = NULL;
                    insert_list_tail(&inode->Instance.File.LockOwnersList, &flockEntry->ListEntry);
                    flockEntry = NULL;
                }
                status = 0;
                break;
            }
            case LOCK_NB | LOCK_SH: {
                if ((inode->Instance.File.Writers > 0) || (inode->Instance.File.WaitingWriters > 0)) {
                    status = EWOULDBLOCK;
                }
                else {
                    inode->Instance.File.Readers++;
                    flockEntry->Req = NULL;
                    insert_list_tail(&inode->Instance.File.LockOwnersList, &flockEntry->ListEntry);
                    flockEntry = NULL;
                }
                status = 0;
                break;
            }
            case LOCK_EX: {
                flockEntry->Exclusive = 1;
                if ((inode->Instance.File.Writers > 0) || (inode->Instance.File.WaitingWriters > 0) ||
                    (inode->Instance.File.WaitingReaders > 0)) {
                    insert_list_tail(&inode->Instance.File.LockWaitersList, &flockEntry->ListEntry);
                    flockEntry = NULL;
                }
                else {
                    inode->Instance.File.Writers++;
                    flockEntry->Req = NULL;
                    insert_list_tail(&inode->Instance.File.LockOwnersList, &flockEntry->ListEntry);
                    flockEntry = NULL;
                }
                status = 0;
                break;
            }
            case LOCK_NB | LOCK_EX: {
                flockEntry->Exclusive = 1;
                if ((inode->Instance.File.Writers > 0) || (inode->Instance.File.Readers > 0)) {
                    insert_list_tail(&inode->Instance.File.LockWaitersList, &flockEntry->ListEntry);
                    flockEntry = NULL;
                }
                else {
                    assert(empty_list(&inode->Instance.File.LockWaitersList));  // can't be waiters: logic error
                    inode->Instance.File.Writers++;
                    flockEntry->Req = NULL;
                    insert_list_tail(&inode->Instance.File.LockOwnersList, &flockEntry->ListEntry);
                    flockEntry = NULL;
                }
                status = 0;
                break;
            }
            case LOCK_UN: {
                status = bitbucket_flock_unlock(inode, fuseContext->pid);
                break;
            }
        }
        BitbucketUnlockInode(inode);
        break;
    }

    if (NULL != inode) {
        BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);
        inode = NULL;
    }

    fuse_reply_err(req, status);

    if (NULL != flockEntry) {
        free(flockEntry);
        flockEntry = NULL;
    }

    return status;
}
