/*
 * Copyright (c) 2017-2020, Tony Mason. All rights reserved.
 */

#include "test_bitbucket.h"
#include <munit.h>


#if !defined(__notused)
#define __notused __attribute__((unused))
#endif // 

int debug_enabled = 0;
enum fuse_log_level debug_level = FUSE_LOG_ERR;

void fuse_log(enum fuse_log_level level, const char *fmt, ...)
{
	va_list ap;

    if (debug_enabled && (debug_level <= level)) {
        va_start(ap, fmt);
        fprintf(stderr, fmt, ap);
        va_end(ap);
    }
}

MunitResult
test_null(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    return MUNIT_OK;
}

extern MunitSuite *SetupMunitSuites(void);

int
main(
    int argc,
    char **argv)
{
    MunitSuite suite;

    suite.prefix = (char *)(uintptr_t)"/bitbucket";
    suite.tests = NULL;
    suite.suites = SetupMunitSuites();
    suite.iterations = 1;
    suite.options = MUNIT_SUITE_OPTION_NONE;   

    return munit_suite_main(&suite, NULL, argc, argv);
}


static MunitTest bitbucket_tests[] = {
    TEST((char *)(uintptr_t)"/null", test_null, NULL),
    TEST(NULL, NULL, NULL),
};


const MunitSuite bitbucket_suite = {
    .prefix = (char *)(uintptr_t)"/bitbucket",
    .tests = bitbucket_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};

static MunitSuite testbitbucket_suites[10];

MunitSuite *SetupMunitSuites()
{
    unsigned index = 0;

    memset(testbitbucket_suites, 0, sizeof(testbitbucket_suites));
    testbitbucket_suites[index++] = object_suite;
    testbitbucket_suites[index++] = inode_suite;
    testbitbucket_suites[index++] = dir_suite;
    testbitbucket_suites[index++] = xattr_suite;
    testbitbucket_suites[index++] = file_suite;
    return testbitbucket_suites;
}


/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
