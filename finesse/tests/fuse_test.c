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
#include <mntent.h>

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif // 

static int ServerRunning;

static int finesse_check(void) 
{
    int status = 0;
    finesse_client_handle_t fch;

    if (0 == ServerRunning) {
        status = FinesseStartClientConnection(&fch);
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
    FILE *mfile;
    struct mntent *entry = NULL;
    static char mountpoint[128];

    if (0 != mountpoint[0]) {
        return mountpoint;
    }

    mfile = fopen("/etc/mtab", "rt");
    munit_assert(NULL != mfile);

    entry = getmntent(mfile);
    while (NULL != entry) {
        if(NULL != strstr(entry->mnt_fsname,name)) {
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

static MunitResult test_finess_started(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    munit_assert(0 == finesse_check());
    munit_assert(0 != ServerRunning);

    munit_assert(NULL != get_mountpoint("passthrough_ll"));

    return MUNIT_OK;
}


static MunitTest fuse_tests[] = {
    TEST((char *)(uintptr_t)"/null", test_null, NULL),
    TEST("/started", test_finess_started, NULL),
    TEST(NULL, NULL, NULL),
};


const MunitSuite fuse_suite = {
    .prefix = (char *)(uintptr_t)"/fuse",
    .tests = fuse_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};

static MunitSuite testfuse_suites[10];

MunitSuite *SetupMunitSuites()
{
    memset(testfuse_suites, 0, sizeof(testfuse_suites));
    testfuse_suites[0] = fuse_suite;
    return testfuse_suites;
}
