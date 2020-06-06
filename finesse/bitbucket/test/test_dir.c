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
#if 0
    void *Table = NULL;

    Table = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
    munit_assert(NULL != Table);

    BitbucketDestroyInodeTable(Table);
#endif // 0

    return MUNIT_OK;
}


static const MunitTest dir_tests[] = {
        TEST("/null", test_null, NULL),
        TEST("/create", test_create_dir, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite dir_suite = {
    .prefix = (char *)(uintptr_t)"/dir",
    .tests = (MunitTest *)(uintptr_t)dir_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};


