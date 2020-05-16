/*
 * Copyright (c) 2017, Tony Mason. All rights reserved.
 */

#include "finesse.h"

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

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif // 

static MunitResult
test_one(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    return MUNIT_OK;
}

#define TEST(_name, _func, _params)             \
    {                                           \
        .name = (_name),                        \
        .test = (_func),                        \
        .setup = NULL,                          \
        .tear_down = NULL,                      \
        .options = MUNIT_TEST_OPTION_NONE,      \
        .parameters = (_params),                     \
    }

int
main(
    int argc,
    char **argv)
{
    static MunitTest tests[] = {
        TEST((char *)(uintptr_t)"/one", test_one, NULL),
#if 0
        TEST((char *)(uintptr_t)"/server/connect", test_server_connect, NULL),
        TEST((char *)(uintptr_t)"/client/connect", test_client_connect, NULL),
        TEST((char *)(uintptr_t)"/connect", test_full_connect, NULL),
        TEST((char *)(uintptr_t)"/message/test", test_message_test, NULL), 
        TEST((char *)(uintptr_t)"/message/name_map", test_message_name_map, NULL),
        TEST((char *)(uintptr_t)"/message/search_path", test_message_search_path, NULL),
        TEST((char *)(uintptr_t)"/message/fstatfs", test_message_fstatfs, NULL),
#endif // 0
	    TEST(NULL, NULL, NULL),
    };
    static const MunitSuite suite = {
        .prefix = (char *)(uintptr_t)"/finesse",
        .tests = tests,
        .suites = NULL,
        .iterations = 1,
        .options = MUNIT_SUITE_OPTION_NONE,
    };

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
