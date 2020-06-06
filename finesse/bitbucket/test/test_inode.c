/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "test_bitbucket.h"
#include "bitbucket.h"
#include <stdlib.h>

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif //

static MunitResult
test_create_inode_table(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    void *Table = NULL;

    Table = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
    munit_assert(NULL != Table);

    BitbucketDestroyInodeTable(Table);

    return MUNIT_OK;
}

static void InodeInitialize(void *Object, size_t Length) 
{
    int status;
    static ino_t ino = 1024;

    bitbucket_inode_t *bbi = (bitbucket_inode_t *)Object;
    assert(Length >= sizeof(bitbucket_inode_t));
    CHECK_BITBUCKET_INODE_MAGIC(bbi);

    munit_assert(BITBUCKET_UNKNOWN_TYPE == bbi->InodeType);
    bbi->InodeType = BITBUCKET_FILE_TYPE;
    status = gettimeofday(&bbi->CreationTime, NULL);
    assert(0 == status);

    bbi->AccessTime = bbi->CreationTime;
    bbi->ModifiedTime = bbi->CreationTime;
    bbi->ChangeTime = bbi->CreationTime;
    bbi->Attributes.st_mode = S_IRWXU | S_IRWXG | S_IRWXO; // is this the right default?
    bbi->Attributes.st_gid = getgid();
    bbi->Attributes.st_uid = getuid();
    bbi->Attributes.st_size = 4096; // TODO: maybe do some sort of funky calculation here?
    bbi->Attributes.st_blksize = 4096; // TODO: again, does this matter?
    bbi->Attributes.st_blocks = bbi->Attributes.st_size / 512;
    bbi->Attributes.st_ino = ino++;
    bbi->Attributes.st_nlink = 0; // this should be bumped when this is added

}


static bitbucket_object_attributes_t TestInodeObjectAttributes = 
{
    .Magic = BITBUCKET_OBJECT_ATTRIBUTES_MAGIC,
    .Initialize = InodeInitialize,
    .Deallocate = NULL,
    .Lock = NULL,
    .Unlock = NULL,
};


static MunitResult
test_inode_table_usage(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    void *Table = NULL;
    const unsigned iterations = 4096;
    bitbucket_inode_t **inodes = NULL;
    uint64_t refcount = ~0;

    inodes = (bitbucket_inode_t **)malloc(sizeof(bitbucket_inode_t *) * iterations);
    munit_assert(NULL != inodes);

    Table = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
    munit_assert(NULL != Table);

    for (unsigned index = 0; index < iterations; index++) {
        bitbucket_inode_t *inode = NULL;
        inodes[index] = BitbucketCreateInode(Table, &TestInodeObjectAttributes, 0);
        CHECK_BITBUCKET_INODE_MAGIC(inodes[index]);
        refcount = BitbucketGetInodeReferenceCount(inodes[index]);
        munit_assert(1 == refcount); // the one I have

        BitbucketReferenceInode(inodes[index]);
        refcount = BitbucketGetInodeReferenceCount(inodes[index]);
        munit_assert(2 == refcount); // the two I have

        CHECK_BITBUCKET_INODE_MAGIC(inodes[index]);
        BitbucketDereferenceInode(inodes[index]);
        refcount = BitbucketGetInodeReferenceCount(inodes[index]);
        munit_assert(1 == refcount); // the ONE I have

        inode = BitbucketLookupInodeInTable(Table, inodes[index]->Attributes.st_ino);
        munit_assert(inodes[index] == inode);
        BitbucketDereferenceInode(inodes[index]);
        refcount = BitbucketGetInodeReferenceCount(inodes[index]);
        munit_assert(1 == refcount); // the ONE I have

    }

    for (unsigned index = 0; index < iterations; index++) {
        bitbucket_inode_t *inode = BitbucketLookupInodeInTable(Table, inodes[index]->Attributes.st_ino);
        assert(inode == inodes[index]);
        BitbucketDereferenceInode(inode);
        refcount = BitbucketGetInodeReferenceCount(inodes[index]);
        munit_assert(1 == refcount); // the ONE I have
    }

    for (unsigned index = 0; index < iterations; index++) {
        refcount = BitbucketGetInodeReferenceCount(inodes[index]);
        munit_assert(1 == refcount); // the ONE I have
        BitbucketDereferenceInode(inodes[index]);
        inodes[index] = NULL;
    }

    // This call only works if the table is empty
    BitbucketDestroyInodeTable(Table);

    // Check and see if we managed to clean everything up properly
    munit_assert(0 == BitbucketObjectCount());

    return MUNIT_OK;
}



static const MunitTest inode_tests[] = {
        TEST("/null", test_null, NULL),
        TEST("/create", test_create_inode_table, NULL),
        TEST("/usage", test_inode_table_usage, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite inode_suite = {
    .prefix = (char *)(uintptr_t)"/inode",
    .tests = (MunitTest *)(uintptr_t)inode_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};


