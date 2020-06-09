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
test_create_dir(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    bitbucket_inode_t *rootdir = NULL;
    bitbucket_inode_table_t *Table = NULL;
    uint32_t refcount = 0;

    Table = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
    munit_assert(NULL != Table);

    rootdir = BitbucketCreateRootDirectory(Table); 

    munit_assert(NULL != rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(5 == refcount); // table + lookup + 2 dir entries + parent ref

    // TODO: this is a strange case, since the root directory has multiple references to itself:
    //  (1) for the table reference on the inode
    //  (2) for the directory entries ("." and "..")
    //  (1) for the parent reference (on itself).
    //  (1) for the pointer returned from CreateRootDirectory (a "lookup" reference)
    //
    // Thus, tearing down a root directory requires more work than tearing down a regular
    // directory, due to the self-referential nature of the root directory.
    BitbucketDeleteRootDirectory(rootdir);
    BitbucketDereferenceInode(rootdir, INODE_LOOKUP_REFERENCE); // from the original creation

    rootdir = NULL;

    BitbucketDestroyInodeTable(Table);
    Table = NULL;

    return MUNIT_OK;
}

static MunitResult
test_create_subdir(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    bitbucket_inode_t *rootdir = NULL;
    const unsigned subdir_count = 4096;
    struct _subdir_data {
        bitbucket_inode_t *inode;
        uuid_t             Uuid;
        char               UuidString[40];
    } *subdir_data;
    int status = 0;
    uint64_t refcount;
    uint64_t predictedRefCount = 0;
    bitbucket_inode_table_t *Table = NULL;

    Table = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
    munit_assert(NULL != Table);

    rootdir = BitbucketCreateRootDirectory(Table);
    munit_assert(NULL != rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(5 == refcount); // table + lookup + 2 dir entries + parent ref
    predictedRefCount = refcount;

    subdir_data = (struct _subdir_data *)malloc(sizeof(struct _subdir_data) * subdir_count);
    munit_assert(NULL != subdir_data);

    for (unsigned index = 0; index < subdir_count; index++) {
        uuid_generate_random(subdir_data[index].Uuid);
        uuid_unparse(subdir_data[index].Uuid, subdir_data[index].UuidString);
        subdir_data[index].inode = BitbucketCreateDirectory(rootdir, subdir_data[index].UuidString);
        munit_assert(NULL != subdir_data[index].inode);
        predictedRefCount += 2; // + 1 for parent, +1 for ".."
        refcount = BitbucketGetInodeReferenceCount(rootdir);
        munit_assert(predictedRefCount == refcount);
    }

    for (unsigned index = 0; index < subdir_count; index++) {
        refcount = BitbucketGetInodeReferenceCount(subdir_data[index].inode);
        munit_assert(4 == refcount); // table + lookup + 2 dir

        status = BitbucketDeleteDirectoryEntry(rootdir, subdir_data[index].UuidString);
        munit_assert(0 == status);

        refcount = BitbucketGetInodeReferenceCount(subdir_data[index].inode);
        munit_assert(3 == refcount); // table + lookup + 1 dir (".")

        status = BitbucketDeleteDirectory(subdir_data[index].inode);
        munit_assert(0 == status);
        refcount = BitbucketGetInodeReferenceCount(subdir_data[index].inode);
        munit_assert(1 == refcount); // lookup

        BitbucketDereferenceInode(subdir_data[index].inode, INODE_LOOKUP_REFERENCE);
        subdir_data[index].inode = NULL;
    }

    BitbucketDeleteRootDirectory(rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(1 == refcount);
    BitbucketDereferenceInode(rootdir, INODE_LOOKUP_REFERENCE);
    rootdir = NULL;
    
    BitbucketDestroyInodeTable(Table);

    return MUNIT_OK;
}

static MunitResult
test_enumerate_dir(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    bitbucket_inode_t *rootdir = NULL;
    uint64_t refcount;
    bitbucket_inode_table_t *Table = NULL;

    Table = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
    munit_assert(NULL != Table);

    rootdir = BitbucketCreateRootDirectory(Table);
    munit_assert(NULL != rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(5 == refcount); // table + lookup + 2 dir entries + parent ref

    BitbucketDeleteRootDirectory(rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(1 == refcount);
    BitbucketDereferenceInode(rootdir, INODE_LOOKUP_REFERENCE);
    rootdir = NULL;
    
    BitbucketDestroyInodeTable(Table);

    return MUNIT_OK;
}

static const MunitTest dir_tests[] = {
        TEST("/null", test_null, NULL),
        TEST("/create", test_create_dir, NULL),
        TEST("/subdir", test_create_subdir, NULL),
        TEST("/enumerate", test_enumerate_dir, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite dir_suite = {
    .prefix = (char *)(uintptr_t)"/dir",
    .tests = (MunitTest *)(uintptr_t)dir_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};


