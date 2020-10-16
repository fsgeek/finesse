
/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <finesse.h>
#include <libgen.h>
#include <mntent.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include "../api/api-internal.h"
#include "fincomm.h"
#include "finesse_test.h"
#include "munit.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif  //

static const char *test_name = "/mnt/bitbucket";

static int ServerRunning = 0;

static int finesse_check(void)
{
    int                     status = 0;
    finesse_client_handle_t fch;

    if (0 == ServerRunning) {
        status = FinesseStartClientConnection(&fch, test_name);
        if (0 == status) {
            sleep(1);
            status = FinesseStopClientConnection(fch);
        }
        if (0 == status) {
            ServerRunning = 1;
        }
    }
    return status;
}

static const char *get_mountpoint(const char *name)
{
    FILE *         mfile;
    struct mntent *entry = NULL;
    static char    mountpoint[128];

    if (0 != mountpoint[0]) {
        return mountpoint;
    }

    mfile = fopen("/etc/mtab", "rt");
    munit_assert(NULL != mfile);

    entry = getmntent(mfile);
    while (NULL != entry) {
        if (NULL != strstr(entry->mnt_fsname, name)) {
            break;
        }
        entry = getmntent(mfile);
    }
    if (NULL == entry) {
        return NULL;
    }

    assert(strlen(entry->mnt_dir) < sizeof(mountpoint));
    strncpy(mountpoint, entry->mnt_dir, sizeof(mountpoint));

    return mountpoint;
}

// gack - this is a generated data file, it's long, and not very
// interesting.

#include "testbbfin_mkdir_data.c"

static MunitResult test_finesse_started(const MunitParameter params[] __notused, void *prv __notused)
{
    munit_assert(0 == finesse_check());
    munit_assert(0 != ServerRunning);

    munit_assert(NULL != get_mountpoint("bitbucket"));

    return MUNIT_OK;
}

static MunitResult test_directories(const MunitParameter params[] __notused, void *prv __notused)
{
    unsigned    index;
    struct stat statbuf;
    int         status;

    for (index = 0; index < sizeof(test_dir_data) / sizeof(test_dir_info_t); index++) {
        //
        // (1) stat the directory
        // (2) create the directory
        //
        switch (test_dir_data[index].Operation) {
            case TEST_DIR_OP_TYPE_STAT:
                memset(&statbuf, 0, sizeof(statbuf));
                status = finesse_stat(test_dir_data[index].Pathname, &statbuf);
                break;
            case TEST_DIR_OP_TYPE_MKDIR:
                status = finesse_mkdir(test_dir_data[index].Pathname, 0775);
                break;
            default:
                munit_assert(0);  // unsupported operation
        }

        munit_assert(test_dir_data[index].ExpectedStatus == status);
    }

    return MUNIT_OK;
}

static MunitTest fuse_tests[] = {
    TEST((char *)(uintptr_t) "/null", test_null, NULL),
    TEST((char *)(uintptr_t) "/started", test_finesse_started, NULL),
    TEST((char *)(uintptr_t) "/directories", test_directories, NULL),
    TEST(NULL, NULL, NULL),
};

const MunitSuite fuse_suite = {
    .prefix     = (char *)(uintptr_t) "/fuse",
    .tests      = fuse_tests,
    .suites     = NULL,
    .iterations = 1,
    .options    = MUNIT_SUITE_OPTION_NONE,
};

static MunitSuite testfuse_suites[10];

MunitSuite *SetupMunitSuites()
{
    memset(testfuse_suites, 0, sizeof(testfuse_suites));
    testfuse_suites[0] = fuse_suite;
    return testfuse_suites;
}
