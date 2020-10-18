
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
#include "testbbfin_data.h"

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

static void cleanup_test_dir(const char *dirname)
{
    int         status;
    struct stat statbuf;
    int         childpid;
    int         retries;

    if (0 == stat(dirname, &statbuf)) {
        // The test directory exists, so make sure we delete it first.
        // Note I'm not checking to see if it is a directory... TODO

        childpid = vfork();
        munit_assert(childpid >= 0);
        if (0 == childpid) {
            char *const rmargs[] = {(char *)(uintptr_t) "rm", (char *)(uintptr_t) "-rf", (char *)(uintptr_t)dirname, NULL};
            status               = execv("/bin/rm", rmargs);
            // code that shouldn't ever run...
            munit_assert(0 == status);
            exit(0);
        }

        // wait up to 10 seconds for the directory to go away...
        retries = 0;
        while (retries < 10) {
            if (0 > stat(dirname, &statbuf)) {
                // directory seems to be gone!
                break;
            }
            sleep(1);
            retries++;
        }
        munit_assert(retries < 10);
    }
}

static MunitResult test_finesse_started(const MunitParameter params[] __notused, void *prv __notused)
{
    munit_assert(0 == finesse_check());
    munit_assert(0 != ServerRunning);

    munit_assert(NULL != get_mountpoint("bitbucket"));

    return MUNIT_OK;
}

static void execute_operations(const testbbfin_info_t *operations)
{
    unsigned    index;
    struct stat statbuf;
    int         status;

    for (index = 0; TEST_DIR_OP_TYPE_END != operations[index].Operation; index++) {
        //
        // (1) stat the directory
        // (2) create the directory
        //
        switch (test_dir_data[index].Operation) {
            case TEST_DIR_OP_TYPE_OPEN:
                status = finesse_open(operations[index].Pathname, 0);
                break;

            case TEST_DIR_OP_TYPE_STAT:
                memset(&statbuf, 0, sizeof(statbuf));
                status = finesse_stat(operations[index].Pathname, &statbuf);
                break;

            case TEST_DIR_OP_TYPE_MKDIR:
                status = finesse_mkdir(operations[index].Pathname, 0775);
                break;

            case TEST_DIR_OP_TYPE_CREATE:
                munit_assert(0);
                break;

            case TEST_DIR_OP_TYPE_UNLINK:
                munit_assert(0);
                break;

            case TEST_DIR_OP_TYPE_CLOSE:
                munit_assert(0);
                break;

            default:
                munit_assert(0);  // unsupported operation
        }

        if (status != operations[index].ExpectedStatus) {
            fprintf(stderr, "%s: %d, Unexpected result index %d, expected %d, got %d\n", __func__, __LINE__, index,
                    operations[index].ExpectedStatus, status);
        }
        munit_assert(operations[index].ExpectedStatus == status);
    }
}

static MunitResult test_directories(const MunitParameter params[] __notused, void *prv __notused)
{
    cleanup_test_dir(test_dir_data[0].Pathname);

    execute_operations(test_dir_data);

    return MUNIT_OK;
}

//
// This test is really a test of the Finesse API library that now handles various open options.
// It relies upon having a full file system (hence bitbucket) to ensure that we get the
// correct behavior.
//
static MunitResult test_open(const MunitParameter params[] __notused, void *prv __notused)
{
    const char *testdir = "/mnt/bitbucket/open_test";

    cleanup_test_dir(testdir);

    return MUNIT_OK;
}

static MunitResult test_openat(const MunitParameter params[] __notused, void *prv __notused)
{
    return MUNIT_OK;
}

static MunitResult test_unlinkat(const MunitParameter params[] __notused, void *prv __notused)
{
    return MUNIT_OK;
}

static MunitTest fuse_tests[] = {
    TEST((char *)(uintptr_t) "/null", test_null, NULL),
    TEST((char *)(uintptr_t) "/started", test_finesse_started, NULL),
    TEST((char *)(uintptr_t) "/directories", test_directories, NULL),
    TEST((char *)(uintptr_t) "/open", test_open, NULL),
    TEST((char *)(uintptr_t) "/openat", test_openat, NULL),
    TEST((char *)(uintptr_t) "/unlinkat", test_unlinkat, NULL),
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
