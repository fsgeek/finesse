/*
 * Copyright (c) 2017-2020, Tony Mason. All rights reserved.
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


#if !defined(__notused)
#define __notused __attribute__((unused))
#endif // 

static MunitResult
test_server_connect(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int status;
    finesse_server_handle_t fsh;

    status = FinesseStartServerConnection(&fsh);
    munit_assert(0 == status);

    status = FinesseStopServerConnection(fsh);
    munit_assert(0 == status);

    return MUNIT_OK;
}

static MunitResult
test_client_connect(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int status;
    finesse_server_handle_t fsh;
    finesse_client_handle_t fch;

    status = FinesseStartServerConnection(&fsh);
    munit_assert(0 == status);

    status = FinesseStartClientConnection(&fch);
    munit_assert(0 == status);

    // There is a race condition between start and stop.
    // So for now, I'll just add a sleep
    // TODO: fix the race.
    sleep(1); 

    status = FinesseStopClientConnection(fch);
    munit_assert(0 == status);
    

    status = FinesseStopServerConnection(fsh);
    munit_assert(0 == status);

    return MUNIT_OK;
}

static MunitResult
test_client_connect_without_server(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int status;
    finesse_client_handle_t fch;

    status = FinesseStartClientConnection(&fch);
    munit_assert(0 != status);

    return MUNIT_OK;
}

static const MunitTest finesse_tests[] = {
        TEST((char *)(uintptr_t)"/null", test_null, NULL),
        TEST((char* )(uintptr_t)"/server/connect", test_server_connect, NULL),
        TEST((char *)(uintptr_t)"/client/connect_without_server", test_client_connect_without_server, NULL),
        TEST((char *)(uintptr_t)"/client/connect", test_client_connect, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite finesse_suite = {
    .prefix = (char *)(uintptr_t)"/api",
    .tests = (MunitTest *)(uintptr_t)finesse_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};

