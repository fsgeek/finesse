//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include <errno.h>
#include <unistd.h>

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include "string.h"

static const char *BitbucketMagicNames[] = {
    "Bitbucket", "Size",    "Name",    "Creation", "Uuid",    "Unused10", "Unused9", "Unused8",
    "Unused7",   "Unused6", "Unused5", "Unused4",  "Unused3", "Unused2",  "Unused1", "Unused0",
};

static int bitbucket_internal_init(void *userdata, struct fuse_conn_info *conn);
static int bitbucket_internal_destroy(void *userdata);

void bitbucket_init(void *userdata, struct fuse_conn_info *conn)
{
    struct timespec start, stop, elapsed;
    int             status, tstatus;
    const char *    calldata_string = NULL;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);
    status  = bitbucket_internal_init(userdata, conn);
    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    BitbucketCountCall(BITBUCKET_CALL_INIT, status ? 0 : 1, &elapsed);

    calldata_string = BitbucketFormatCallData(NULL, 0);

    if (NULL != calldata_string) {
        printf("Bitbucket Final Call Data:\n%s\n", calldata_string);
        BitbucketFreeFormattedCallData(calldata_string);
        calldata_string = NULL;
    }
}

static int bitbucket_internal_init(void *userdata, struct fuse_conn_info *conn)
{
    bitbucket_userdata_t *BBud  = (bitbucket_userdata_t *)userdata;
    unsigned              index = 0;

    assert(NULL != conn);

    // Options we could set:
    // FUSE_CAP_EXPORT_SUPPORT - allow lookup of . and ..
    // FUSE_CAP_SPLICE_MOVE - allows grabbing pages
    // FUSE_CAP_AUTO_INVAL_DATA - this is something we should probably set in
    // Finesse mode FUSE_CAP_WRITEBACK_CACHE - enable writeback data caching
    // FUSE_CAP_POSIX_ACL - this would entail adding support for POSIX ACLs
    // unsigned time_gran; -- we internally have nanosecond granularity (default)
    // but we aren't populating the structures with it currently (should fix)
    //
    conn->want |= FUSE_CAP_WRITEBACK_CACHE;
    conn->want |= FUSE_CAP_EXPORT_SUPPORT;

    BBud->Magic = BITBUCKET_USER_DATA_MAGIC;
    CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);
    BBud->InodeTable = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
    assert(NULL != BBud->InodeTable);
    BBud->RootDirectory = BitbucketCreateRootDirectory(BBud->InodeTable);
    assert(NULL != BBud->RootDirectory);

    BBud->BitbucketMagicDirectories[0].Name  = BitbucketMagicNames[0];
    BBud->BitbucketMagicDirectories[0].Inode = BitbucketCreateDirectory(BBud->RootDirectory, BitbucketMagicNames[0]);
    assert(NULL != BBud->BitbucketMagicDirectories[0].Inode);
    BBud->BitbucketMagicDirectories[0].Inode->Attributes.st_mode &= ~0222;  // strip off write bits

    for (index++; index < sizeof(BitbucketMagicNames) / sizeof(const char *); index++) {
        if (0 == strncmp("Unused", BitbucketMagicNames[index], 6)) {
            continue;  // skip these for now.
        }
        BBud->BitbucketMagicDirectories[index].Name = BitbucketMagicNames[index];
        BBud->BitbucketMagicDirectories[index].Inode =
            BitbucketCreateDirectory(BBud->BitbucketMagicDirectories[0].Inode, BBud->BitbucketMagicDirectories[index].Name);
        assert(NULL != BBud->BitbucketMagicDirectories[index].Inode);
        BBud->BitbucketMagicDirectories[index].Inode->Attributes.st_mode &= ~0222;  // strip off write bits
    }

    BitbucketInitializeCallStatistics();

    // All of these inodes have a lookup reference on them.
    return 0;
}

void bitbucket_destroy(void *userdata)
{
    struct timespec start, stop, elapsed;
    int             status, tstatus;

    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    assert(0 == tstatus);
    status  = bitbucket_internal_destroy(userdata);
    tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    assert(0 == tstatus);
    timespec_diff(&start, &stop, &elapsed);
    BitbucketCountCall(BITBUCKET_CALL_INIT, status ? 0 : 1, &elapsed);
}

int bitbucket_internal_destroy(void *userdata)
{
    bitbucket_userdata_t *BBud            = (bitbucket_userdata_t *)userdata;
    unsigned              index           = 0;
    const char *          calldata_string = BitbucketFormatCallData(NULL, 0);  //
    int                   fd              = -1;
    ssize_t               written         = 0;

    if (NULL != calldata_string) {
        if (NULL != BBud->CallStatFile) {
            fd = open(BBud->CallStatFile, O_WRONLY | O_APPEND | O_CREAT, 0664);

            if (fd > 0) {
                written = write(fd, calldata_string, strlen(calldata_string));
                if (written > 0) {
                    written += write(fd, "\n", 1);
                }
                close(fd);
                fd = -1;
            }
            else {
                fprintf(stderr, "Unable to open %s, errno = %d (%s)", BBud->CallStatFile, errno, strerror(errno));
            }
        }

        if (0 >= written) {
            printf("Bitbucket Final Call Data:\n%s\n", calldata_string);
        }

        BitbucketFreeFormattedCallData(calldata_string);
        calldata_string = NULL;
    }

    // Let's undo the work that we did in init.
    // Note: this is going to crash if the volume isn't cleanly torn down, so
    // that's probabl not viable long term.  Good for testing, though.

    for (index = sizeof(BitbucketMagicNames) / sizeof(const char *); index > 0;) {
        index--;

        if (0 == strncmp("Unused", BitbucketMagicNames[index], 6)) {
            continue;  // skip these for now.
        }

        BitbucketDeleteDirectory(BBud->BitbucketMagicDirectories[index].Inode);
        BitbucketDereferenceInode(BBud->BitbucketMagicDirectories[index].Inode, INODE_LOOKUP_REFERENCE,
                                  1);  // undo original create ref
        BBud->BitbucketMagicDirectories[index].Inode = NULL;
    }

#if 0
	// This is where we have to "come clean".
	BitbucketDestroyInodeTable(BBud->InodeTable);
	BBud->InodeTable = NULL;
#endif  // 0

    return 0;
}
