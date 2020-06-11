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
    munit_assert(5 == refcount); // table + lookup + 2 dir entries + parent ref

    status = BitbucketLookupExtendedAttribute(rootdir, "selinux.security", &dataLength, &data);
    munit_assert(ENODATA == status);

    BitbucketDeleteRootDirectory(rootdir);
    refcount = BitbucketGetInodeReferenceCount(rootdir);
    munit_assert(1 == refcount);
    BitbucketDereferenceInode(rootdir, INODE_LOOKUP_REFERENCE);
    rootdir = NULL;
    
    BitbucketDestroyInodeTable(Table);

    return MUNIT_OK;
}



static const MunitTest xattr_tests[] = {
        TEST("/null", test_null, NULL),
        TEST("/lookup", test_lookup, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite xattr_suite = {
    .prefix = (char *)(uintptr_t)"/xattr",
    .tests = (MunitTest *)(uintptr_t)xattr_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};


