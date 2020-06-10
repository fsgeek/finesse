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


static const MunitTest xattr_tests[] = {
        TEST("/null", test_null, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite xattr_suite = {
    .prefix = (char *)(uintptr_t)"/xattr",
    .tests = (MunitTest *)(uintptr_t)xattr_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};


