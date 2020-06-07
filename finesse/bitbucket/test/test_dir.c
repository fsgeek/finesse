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
    
    rootdir = BitbucketCreateDirectory(NULL, NULL);

    munit_assert(NULL != rootdir);

    BitbucketDereferenceInode(rootdir);

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

    rootdir = BitbucketCreateDirectory(NULL, NULL);
    munit_assert(NULL != rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(4 == refcount); // we have one, there are three internal (for '.', '..', and Directory.Parent)

    subdir_data = (struct _subdir_data *)malloc(sizeof(struct _subdir_data) * subdir_count);
    munit_assert(NULL != subdir_data);

    for (unsigned index = 0; index < subdir_count; index++) {
        uuid_generate_random(subdir_data[index].Uuid);
        uuid_unparse(subdir_data[index].Uuid, subdir_data[index].UuidString);
        subdir_data[index].inode = BitbucketCreateDirectory(rootdir, subdir_data[index].UuidString);
        munit_assert(NULL != subdir_data[index].inode);
    }

    for (unsigned index = 0; index < subdir_count; index++) {
        refcount = BitbucketGetInodeReferenceCount(subdir_data[index].inode);
        munit_assert(3 == refcount); // our reference + '.', and the root->subdir reference.

        status = BitbucketDeleteDirectoryEntry(rootdir, subdir_data[index].UuidString);
        munit_assert(0 == status);

        refcount = BitbucketGetInodeReferenceCount(subdir_data[index].inode);
        munit_assert(2 == refcount);

        status = BitbucketDeleteDirectory(subdir_data[index].inode);
        munit_assert(0 == status);
    }


    BitbucketDereferenceInode(rootdir);

    return MUNIT_OK;
}


static const MunitTest dir_tests[] = {
        TEST("/null", test_null, NULL),
        TEST("/create", test_create_dir, NULL),
        TEST("/subdir", test_create_subdir, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite dir_suite = {
    .prefix = (char *)(uintptr_t)"/dir",
    .tests = (MunitTest *)(uintptr_t)dir_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};


