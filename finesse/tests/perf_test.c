/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <uuid/uuid.h>
#include <pthread.h>
#include "munit.h"
#include <errno.h>
#include <finesse.h>
#include "finesse_test.h"
#include "fincomm.h"


#if !defined(__notused)
#define __notused __attribute__((unused))
#endif //

#define TEST_VERSION (0x10)


static const MunitTest perf_tests[] = {
        TEST("/null", test_null, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite perf_suite = {
    .prefix = (char *)(uintptr_t)"/perf",
    .tests = (MunitTest *)(uintptr_t)perf_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};

static MunitSuite perftest_suites[10];

MunitSuite *SetupMunitSuites()
{
    memset(perftest_suites, 0, sizeof(perftest_suites));
    perftest_suites[0] = perf_suite;
    return perftest_suites;
}
