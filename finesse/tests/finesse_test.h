/*
 * Copyright (c) 2017-2020, Tony Mason. All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <finesse.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include "munit.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif  //

#define TESTEX(_name, _func, _setup, _tear_down, _options, _params)                                                         \
    {                                                                                                                       \
        .name = (char *)(uintptr_t)(_name), .test = (_func), .setup = _setup, .tear_down = _tear_down, .options = _options, \
        .parameters = (_params),                                                                                            \
    }

#define TEST(_name, _func, _params) TESTEX(_name, _func, NULL, NULL, MUNIT_TEST_OPTION_NONE, _params)

extern const MunitSuite fincomm_suite;
extern const MunitSuite finesse_suite;
extern const MunitSuite testutils_suite;
extern const MunitSuite fuse_suite;

extern MunitResult test_null(const MunitParameter params[], void *prv);
extern MunitSuite *SetupMunitSuites(void);
