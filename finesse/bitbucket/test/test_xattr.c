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
test_insert(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    bitbucket_inode_t *rootdir = NULL;
    int status = 0;
    uint64_t refcount;
    bitbucket_inode_table_t *Table = NULL;
    size_t dataLength;
    const void *data;
    bitbucket_inode_t *testfile = NULL;
    const char *testattrname = "testattr";
    char testdata[] = "This is some test data\0And some more";

    Table = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
    munit_assert(NULL != Table);

    rootdir = BitbucketCreateRootDirectory(Table);
    munit_assert(NULL != rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(4 == refcount); // lookup + 2 dir entries + parent ref

    testfile = BitbucketCreateFile(rootdir, "testfile", NULL);
    munit_assert(NULL != testfile);

    BitbucketLockInode(testfile, 1);
    status = BitbucketInsertExtendedAttribute(testfile, testattrname, sizeof(testdata), testdata);
    assert(0 == status);
    BitbucketUnlockInode(testfile);

    dataLength = 1;
    status = BitbucketLookupExtendedAttribute(testfile, testattrname, &dataLength, &data);
    munit_assert(0 == status);
    munit_assert(sizeof(testdata) == dataLength);
    assert(0 == memcmp(data, testdata, sizeof(testdata)));

    status = BitbucketDeleteDirectoryEntry(rootdir, "testfile");
    assert(0 == status);
    BitbucketDereferenceInode(testfile, INODE_LOOKUP_REFERENCE, 1);
    testfile = NULL;
    

    BitbucketDeleteRootDirectory(rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(1 == refcount);
    BitbucketDereferenceInode(rootdir, INODE_LOOKUP_REFERENCE, 1);
    rootdir = NULL;
    
    BitbucketDestroyInodeTable(Table);

    return MUNIT_OK;

    return MUNIT_OK;
}


static 
MunitResult
test_lookup(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    bitbucket_inode_t *rootdir = NULL;
    int status = 0;
    uint64_t refcount;
    bitbucket_inode_table_t *Table = NULL;
    size_t dataLength;
    const void *data;

    Table = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
    munit_assert(NULL != Table);

    rootdir = BitbucketCreateRootDirectory(Table);
    munit_assert(NULL != rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(4 == refcount); // lookup + 2 dir entries + parent ref

    status = BitbucketLookupExtendedAttribute(rootdir, "selinux.security", &dataLength, &data);
    munit_assert(ENODATA == status);

    BitbucketDeleteRootDirectory(rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(1 == refcount);
    BitbucketDereferenceInode(rootdir, INODE_LOOKUP_REFERENCE, 1);
    rootdir = NULL;
    
    BitbucketDestroyInodeTable(Table);

    return MUNIT_OK;
}



static const MunitTest xattr_tests[] = {
        TEST("/null", test_null, NULL),
        TEST("/lookup", test_lookup, NULL),
        TEST("/insert", test_insert, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite xattr_suite = {
    .prefix = (char *)(uintptr_t)"/xattr",
    .tests = (MunitTest *)(uintptr_t)xattr_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};


