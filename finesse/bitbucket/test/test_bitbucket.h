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

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif // 

#define TEST(_name, _func, _params)             \
    {                                           \
        .name = (char *)(uintptr_t)(_name),     \
        .test = (_func),                        \
        .setup = NULL,                          \
        .tear_down = NULL,                      \
        .options = MUNIT_TEST_OPTION_NONE,      \
        .parameters = (_params),                     \
    }

// extern const MunitSuite fincomm_suite;
extern const MunitSuite inode_suite;
extern const MunitSuite trie_suite;
extern const MunitSuite object_suite;
extern const MunitSuite dir_suite;
extern const MunitSuite xattr_suite;
extern const MunitSuite file_suite;

extern MunitResult test_null(const MunitParameter params[], void *prv);
extern MunitSuite *SetupMunitSuites(void);
