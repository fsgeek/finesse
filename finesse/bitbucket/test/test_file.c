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

static
MunitResult
test_create(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    bitbucket_inode_t *rootdir = NULL;
    const unsigned file_count = 4096;
    struct _file_data {
        bitbucket_inode_t *inode;
        uuid_t             Uuid;
        char               UuidString[40];
    } *file_data;
    int status = 0;
    uint64_t refcount;
    uint64_t predictedRefCount = 0;
    bitbucket_inode_table_t *Table = NULL;

    Table = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
    munit_assert(NULL != Table);

    rootdir = BitbucketCreateRootDirectory(Table);
    munit_assert(NULL != rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(4 == refcount); // lookup + 2 dir entries + parent ref
    predictedRefCount = refcount;

    file_data = (struct _file_data *)malloc(sizeof(struct _file_data) * file_count);
    munit_assert(NULL != file_data);

    for (unsigned index = 0; index < file_count; index++) {
        uuid_generate_random(file_data[index].Uuid);
        uuid_unparse(file_data[index].Uuid, file_data[index].UuidString);
        file_data[index].inode = BitbucketCreateFile(rootdir, file_data[index].UuidString, NULL);
        munit_assert(NULL != file_data[index].inode);
        refcount = BitbucketGetInodeReferenceCount(rootdir);
        munit_assert(predictedRefCount == refcount); // shouldn't change
    }

    for (unsigned index = 0; index < file_count; index++) {
        refcount = BitbucketGetInodeReferenceCount(file_data[index].inode);
        munit_assert(2 == refcount); // lookup + 1 dir

        status = BitbucketRemoveFileFromDirectory(rootdir, file_data[index].UuidString);
        munit_assert(0 == status);

        refcount = BitbucketGetInodeReferenceCount(file_data[index].inode);
        munit_assert(1 == refcount); // lookup

        BitbucketDereferenceInode(file_data[index].inode, INODE_LOOKUP_REFERENCE);
        file_data[index].inode = NULL;

        // Now let's make sure we don't find the entry
        status = BitbucketRemoveFileFromDirectory(rootdir, file_data[index].UuidString);
        munit_assert(ENOENT == status);
    }

    BitbucketDeleteRootDirectory(rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(1 == refcount);
    BitbucketDereferenceInode(rootdir, INODE_LOOKUP_REFERENCE);
    rootdir = NULL;
    
    BitbucketDestroyInodeTable(Table);

    if (NULL != file_data) {
        free(file_data);
    }

    return MUNIT_OK;
}

static
MunitResult
test_forget(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    bitbucket_inode_t *rootdir = NULL;
    const unsigned file_count = 4096;
    struct _file_data {
        bitbucket_inode_t *inode;
        uuid_t             Uuid;
        char               UuidString[40];
    } *file_data;
    int status = 0;
    uint64_t refcount;
    uint64_t predictedRefCount = 0;
    bitbucket_inode_table_t *Table = NULL;

    Table = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
    munit_assert(NULL != Table);

    rootdir = BitbucketCreateRootDirectory(Table);
    munit_assert(NULL != rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    predictedRefCount = refcount;

    file_data = (struct _file_data *)malloc(sizeof(struct _file_data) * file_count);
    munit_assert(NULL != file_data);

    for (unsigned index = 0; index < file_count; index++) {
        uuid_generate_random(file_data[index].Uuid);
        uuid_unparse(file_data[index].Uuid, file_data[index].UuidString);
        file_data[index].inode = BitbucketCreateFile(rootdir, file_data[index].UuidString, NULL);
        munit_assert(NULL != file_data[index].inode);
        refcount = BitbucketGetInodeReferenceCount(rootdir);
        munit_assert(predictedRefCount == refcount); // shouldn't change
    }

    for (unsigned index = 0; index < file_count; index++) {
        ino_t ino;

        refcount = BitbucketGetInodeReferenceCount(file_data[index].inode);
        munit_assert(2 == refcount); // lookup + 1 dir

        status = BitbucketRemoveFileFromDirectory(rootdir, file_data[index].UuidString);
        munit_assert(0 == status);

        // Now let's make sure we don't find the entry in the directory
        status = BitbucketRemoveFileFromDirectory(rootdir, file_data[index].UuidString);
        munit_assert(ENOENT == status);

        refcount = BitbucketGetInodeReferenceCount(file_data[index].inode);
        munit_assert(1 == refcount); // lookup

        // Let's create a FUSE style "lookup" reference
        BitbucketReferenceInode(file_data[index].inode, INODE_FUSE_LOOKUP_REFERENCE);
        ino = file_data[index].inode->Attributes.st_ino;

        BitbucketDereferenceInode(file_data[index].inode, INODE_LOOKUP_REFERENCE);
        file_data[index].inode = NULL;

        // Now let's use the inode number to find it
        file_data[index].inode = BitbucketLookupInodeInTable(Table, ino);
        assert(NULL != file_data[index].inode);

        // Now let's release the lookup reference
        BitbucketDereferenceInode(file_data[index].inode, INODE_LOOKUP_REFERENCE);
        BitbucketDereferenceInode(file_data[index].inode, INODE_FUSE_LOOKUP_REFERENCE);

        // Make sure we can't find it now...
        file_data[index].inode = BitbucketLookupInodeInTable(Table, ino);
        assert(NULL == file_data[index].inode);
    }

    BitbucketDeleteRootDirectory(rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(1 == refcount);
    BitbucketDereferenceInode(rootdir, INODE_LOOKUP_REFERENCE);
    rootdir = NULL;
    
    BitbucketDestroyInodeTable(Table);

    if (NULL != file_data) {
        free(file_data);
    }

    return MUNIT_OK;
}

static const MunitTest file_tests[] = {
        TEST("/null", test_null, NULL),
        TEST("/create", test_create, NULL),
        TEST("/forget", test_forget, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite file_suite = {
    .prefix = (char *)(uintptr_t)"/file",
    .tests = (MunitTest *)(uintptr_t)file_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};


