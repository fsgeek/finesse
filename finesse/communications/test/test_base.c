/*
 * Copyright (c) 2017-2020, Tony Mason. All rights reserved.
 */

#include "finesse_test.h"


#if !defined(__notused)
#define __notused __attribute__((unused))
#endif // 

MunitResult
test_null(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    return MUNIT_OK;
}


int
main(
    int argc,
    char **argv)
{
    MunitSuite subtests[10];
    MunitSuite suite;

    memset(subtests, 0, sizeof(subtests));
    subtests[0] = finesse_suite;
    subtests[1] = fincomm_suite;

    suite.prefix = (char *)(uintptr_t)"/finesse";
    suite.tests = NULL;
    suite.suites = subtests;
    suite.iterations = 1;
    suite.options = MUNIT_SUITE_OPTION_NONE;   

    return munit_suite_main(&suite, NULL, argc, argv);
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
