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
#include <libgen.h>
#include "../api/api-internal.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif //

#define TEST_MOUNT_PREFIX (char *)(uintptr_t)"mountprefix"
#define TEST_OPEN_FILE_PARAM_FILE (char *)(uintptr_t)"file"
#define TEST_OPEN_FILE_PARAM_DIR (char *)(uintptr_t)"dir"
#define TEST_FILE_COUNT (char *)(uintptr_t)"filecount"
#define TEST_FINESSE_OPTION (char *)(uintptr_t)"niccolum"

#define TEST_FINESSE_OPTION_TRUE (char *)(uintptr_t)"true"
#define TEST_FINESSE_OPTION_FALSE (char *)(uintptr_t)"false"

static const char *test_name = "/mnt/pt";

static int ServerRunning = 0;
static int finesse_enabled = 0;


static int finesse_check(void)
{
    int status = 0;
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

static char **test_files;

static void generate_files(const char *test_dir, const char *base_file_name, unsigned short filecount)
{
    size_t fn_length, table_length, td_length, bf_length;
    char **file_list = NULL;
    unsigned index;

    // 37 = size of a UUID - 32 digts + 4 dashes + 1 for the '\0' at the end
    td_length = strlen(test_dir);
    bf_length = strlen(base_file_name);
    fn_length = td_length + bf_length + 34 + 1;

    table_length = sizeof(char *) * (filecount + 1);
    file_list = (char **)malloc(table_length);
    munit_assert_not_null(file_list);

    for (index = 0; index < filecount; index++) {
        uuid_t test_uuid;
        char uuid_string[40];

        uuid_generate_time_safe(test_uuid);
        uuid_unparse_lower(test_uuid, uuid_string);
        munit_assert(strlen(uuid_string) == 36);
        file_list[index] = (char *)malloc(fn_length);
        munit_assert_not_null(file_list[index]);
        snprintf(file_list[index], fn_length, "%s/%s-%s", test_dir, base_file_name, uuid_string);
    }
    file_list[filecount] = NULL;

    test_files = file_list;
}


static int get_finesse_option(const MunitParameter params[])
{
    const char *finesse = munit_parameters_get(params, TEST_FINESSE_OPTION);
    int enabled = 0; // default is false

    if (NULL != finesse) {
        if (0 == strcmp(TEST_FINESSE_OPTION_TRUE, finesse)) {
            enabled = 1;
        }
    }

    return enabled;
}

static int mkdirpath(const char *dir)
{
    char *dirpath = strdup(dir);
    char *filepath = strdup(dir);
    char *dname = NULL;
    int fd = -1;
    int status;

    while (-1 == fd) {

        if (NULL == dir) {
            // can't open an empty path
            return fd;
        }

        fd = open(dirpath, O_RDONLY | __O_DIRECTORY);
        if (0 > fd) {
            // didn't exist, so let's break up the name
            dname = dirname(dirpath);
            fd = mkdirpath(dname);
            if (0 > fd) {
                // failed - can't move on at this point
                break;
            }

            // So dname exists, let's make bname
            status = mkdir(dir, 0755);
            munit_assert(0 == status);
            close(fd);
            fd = open(dirpath, O_RDONLY | __O_DIRECTORY);
            // no matter what, we are done at this point
            break;
        }
    }

    // cleanup
    if (NULL != dirpath) {
        free(dirpath);
    }

    if (NULL != filepath) {
        free(filepath);
    }

    return fd;
}

static void setup_test(const MunitParameter params[])
{
    const char *dir;
    const char *file;
    const char *filecount;
    unsigned file_count;
    int dfd;

    dir = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(dir);

    file = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_FILE);
    munit_assert_not_null(file);

    filecount = munit_parameters_get(params, TEST_FILE_COUNT);
    munit_assert_not_null(filecount);
    file_count = strtoul(filecount, NULL, 0);
    munit_assert(file_count > 0);
    munit_assert(file_count < 65536); // arbitrary

    finesse_enabled = get_finesse_option(params);

    dfd = mkdirpath(dir);
    munit_assert_int(dfd, >=, 0);
    munit_assert_int(close(dfd), >=, 0);

    generate_files(dir, file, file_count);

}


static void cleanup_files(void)
{
    unsigned index;

    while (NULL != test_files) {

        index = 0;
        while (NULL != test_files[index]) {
            free(test_files[index]);
            index++;
        }

        free(test_files);
        test_files = NULL;
    }
}

static void finesse_shutdown(void)
{
    cleanup_files();
}

static void cleanup_test(const MunitParameter params[])
{
    (void) params;
    const char *dir;
    int enabled = get_finesse_option(params);

    dir = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(dir);

    cleanup_files();
    if (enabled) {
        finesse_unlink(dir);
    }
    else {
        unlink(dir);
    }
    finesse_enabled = 0;
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

static MunitResult
test_open_dir(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int fd;
    const char *prefix;
    const char *pathname;
    char scratch[512];

    finesse_init();


    setup_test(params);

    prefix = munit_parameters_get(params, TEST_MOUNT_PREFIX);
    munit_assert_not_null(prefix);

    pathname = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(pathname);

    if (finesse_enabled)
        munit_assert(0 == finesse_init_file_state_mgr());

    assert(strlen(prefix) < sizeof(scratch));
    strncpy(scratch, prefix, sizeof(scratch) - 1);
    scratch[sizeof(scratch) - 1] = '\0';
    strncat(scratch, pathname, sizeof(scratch) - strlen(scratch) - 1);

    if (finesse_enabled) {
        fd = finesse_open(scratch, 0);
    }
    else {
        fd = open(scratch, 0);
    }
    munit_assert_int(fd, >=, 0);

    if (finesse_enabled) {
        munit_assert_int(finesse_close(fd), >=, 0);
        finesse_terminate_file_state_mgr();

    }
    else {
        munit_assert_int(close(fd), >=, 0);
    }

    cleanup_test(params);

    finesse_shutdown();

    return MUNIT_OK;
}

static MunitResult
test_open_existing_files(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int fd;
    const char *prefix;
    int index;
    char scratch[512];


    finesse_enabled = get_finesse_option(params);

    finesse_init();
    setup_test(params);

    prefix = munit_parameters_get(params, TEST_MOUNT_PREFIX);
    munit_assert_not_null(prefix);

    munit_assert_not_null(test_files);
    if (finesse_enabled) {
    	munit_assert(0 == finesse_init_file_state_mgr());
    }

    // create files
    index = 0;
    while (NULL != test_files[index]) {
        if (finesse_enabled) {
            fd = finesse_open(test_files[index], O_CREAT, 0664); // existing
        }
        else {
            fd = open(test_files[index], O_CREAT, 0664); // existing
        }
        munit_assert_int(fd, >=, 0);
        if (finesse_enabled) {
            munit_assert_int(finesse_close(fd), >=, 0);
        }
        else {
            munit_assert_int(close(fd), >=, 0);
        }
        index++;
    }

    // now open the files
    index = 0;
    while (NULL != test_files[index]) {
        assert(strlen(prefix) < sizeof(scratch));
        strncpy(scratch, prefix, sizeof(scratch) - 1);
        scratch[sizeof(scratch) - 1] = '\0';
        strncat(scratch, test_files[index], sizeof(scratch) - strlen(scratch) - 1);

        if (finesse_enabled) {
          fd = finesse_open(scratch, 0); // existing
        }
        else {
          fd = open(scratch, 0); // existing
        }
        if (0 > fd) {
            perror("finesse_open");
        }
        munit_assert_int(fd, >=, 0);
        if (finesse_enabled) {
            munit_assert_int(finesse_close(fd), >=, 0);
        }
        else {
            munit_assert_int(close(fd), >=, 0);
        }
        index++;
    }

    cleanup_test(params);

    if (finesse_enabled)
        finesse_terminate_file_state_mgr();

    finesse_shutdown();

    return MUNIT_OK;
}

static MunitResult
test_open_nonexistant_files(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int fd;
    const char *prefix;
    const char *dir;
    const char *file;
    const char *filecount;
    char scratch[512];
    unsigned index;
    unsigned long file_count;

    finesse_init();

    prefix = munit_parameters_get(params, TEST_MOUNT_PREFIX);
    munit_assert_not_null(prefix);

    dir = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(dir);

    file = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_FILE);
    munit_assert_not_null(file);

    filecount = munit_parameters_get(params, TEST_FILE_COUNT);
    munit_assert_not_null(filecount);
    file_count = strtoul(filecount, NULL, 0);
    munit_assert(file_count > 0);
    munit_assert(file_count < 65536); // arbitrary

    generate_files(dir, file, file_count);
    munit_assert(strlen(prefix) < 256);
    if (finesse_enabled)
        munit_assert(0 == finesse_init_file_state_mgr());

    index = 0;
    while (NULL != test_files[index]) {
        strncpy(scratch, prefix, sizeof(scratch) - 1);
        scratch[sizeof(scratch) - 1] = '\0';
        strncat(scratch, test_files[index], sizeof(scratch) - strlen(scratch) - 1);

        if (finesse_enabled) {
            fd = finesse_open(scratch, 0);
        }
        else {
            fd = open(scratch, 0);
        }
        munit_assert_int(fd, <, 0);
        munit_assert_int(errno, ==, ENOENT);
        index++;
    }

    if (finesse_enabled)
        finesse_terminate_file_state_mgr();


    finesse_shutdown();

    return MUNIT_OK;
}

static MunitResult
test_fstatfs(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int fd;
    int status;
    const char *prefix;
    size_t prefix_length;
    const char *test_file;
    struct statvfs statfsstruc, statfsstruc2;
    char scratch[512];

    finesse_enabled = get_finesse_option(params);

    finesse_init();
    setup_test(params);

    prefix =  munit_parameters_get(params, TEST_MOUNT_PREFIX);
    munit_assert_not_null(prefix);
    prefix_length = strlen(prefix);

    munit_assert_not_null(test_files);
    test_file = test_files[0];
    munit_assert_not_null(test_file);

    if(finesse_enabled)
        munit_assert(0 == finesse_init_file_state_mgr());

    // create the file
    fd = open(test_file, O_CREAT, 0664);
    munit_assert_int(fd, >=, 0);
    close(fd);
    fd = -1;

    memset(&statfsstruc, 0, sizeof(struct statvfs));
    memset(&statfsstruc2, 0, sizeof(statfsstruc2));

    if (finesse_enabled && (prefix_length > 0)) {
        // Now using the passthrough name, open the file
        strcpy(scratch, prefix);
        strncat(scratch, test_file, sizeof(scratch) - strlen(scratch) - 1);

        fd = finesse_open(scratch, O_RDONLY);
        munit_assert_int(fd, >=, 0);

        status = statvfs(test_file, &statfsstruc);
        munit_assert(0 == status);

        status = finesse_fstatvfs(fd, &statfsstruc2);
        munit_assert_int(status, ==, 0);
        munit_assert(0 == memcmp(&statfsstruc, &statfsstruc2, offsetof(struct statvfs, f_favail)));

        // for some reason, linux strips away some of the data the file system gives back!
        // compare the fields that seem to be the same
        status = finesse_statvfs(scratch, &statfsstruc2);
        munit_assert_int(status, ==, 0);
        munit_assert(0 == memcmp(&statfsstruc, &statfsstruc2, offsetof(struct statvfs, f_favail)));

        memset(&statfsstruc2, 0, sizeof(statfsstruc2));
        status = finesse_fstatvfs(fd, &statfsstruc2);
        munit_assert_int(status, ==, 0);
        munit_assert(0 == memcmp(&statfsstruc, &statfsstruc2, offsetof(struct statvfs, f_favail)));

    }
    else {
        fd = finesse_open(test_file, O_RDONLY);
        munit_assert_int(fd, >=, 0);

        status = statvfs(test_file, &statfsstruc);
        munit_assert(0 == status);

        status = fstatvfs(fd, &statfsstruc2);
        munit_assert(0 == status);
        munit_assert(0 == memcmp(&statfsstruc, &statfsstruc2, sizeof(statfsstruc)));
    }

    munit_assert(0 <= fd);

    if (finesse_enabled) {
        munit_assert_int(finesse_close(fd), >=, 0);
    }
    else {
       munit_assert_int(close(fd), >=, 0);
    }

    cleanup_test(params);

    if (finesse_enabled){
        finesse_terminate_file_state_mgr();
    }

    finesse_shutdown();
    return MUNIT_OK;
}

static MunitResult
test_openat(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int fd = -1;
    int dirfd = -1;
    int status = 0;
    const char *prefix = NULL;
    const char *test_file = NULL;
    const char *base_dir = NULL;

    finesse_enabled = get_finesse_option(params);

    if (!finesse_enabled) {
        // not interested in the native path here
        return MUNIT_OK;
    }

    finesse_init();
    setup_test(params);

    prefix =  munit_parameters_get(params, TEST_MOUNT_PREFIX);
    munit_assert_not_null(prefix);

    base_dir = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(prefix);

    munit_assert_not_null(test_files);
    test_file = test_files[0];
    munit_assert_not_null(test_file);

    if(finesse_enabled) {
        munit_assert(0 == finesse_init_file_state_mgr());
    }

    // open the root directory
    dirfd = finesse_open(base_dir, O_RDONLY);
    munit_assert_int(dirfd, >=, 0);

    // create the file
    fd = finesse_openat(dirfd, test_file, O_CREAT, 0664);
    munit_assert_int(fd, >=, 0);

    // now close it
    status = finesse_close(fd);
    munit_assert(0 == status);

    status = finesse_unlinkat(dirfd, test_file, 0);
    munit_assert(0 == status);

    // verify it is deleted
    fd = finesse_openat(dirfd, test_file, O_RDONLY);
    munit_assert(-1 == fd);
    munit_assert(ENOENT == errno);

    // done
    status = finesse_close(dirfd);
    munit_assert(0 == status);
    dirfd = -1;

    cleanup_test(params);

    if (finesse_enabled){
        finesse_terminate_file_state_mgr();
    }

    finesse_shutdown();

    return MUNIT_OK;
}


static const char *mount_prefix[] = {"", "/mnt/pt", NULL};
static const char *files[] = { "testfile1", NULL};
static const char *dirs[] = { "/tmp/nicfs/dir1", NULL};
static const char *file_counts[] = { "1", /* "100", "1000",  "10000",*/ NULL};
static char *TEST_FINESSE_OPTIONS[] = {/*TEST_FINESSE_OPTION_FALSE,*/ TEST_FINESSE_OPTION_TRUE, NULL};

MunitParameterEnum open_params[] =
{
    {.name = (char *)(uintptr_t)TEST_MOUNT_PREFIX, .values = (char **)(uintptr_t)mount_prefix},
    {.name = (char *)(uintptr_t)TEST_OPEN_FILE_PARAM_FILE, .values = (char **)(uintptr_t)files},
    {.name = (char *)(uintptr_t)TEST_OPEN_FILE_PARAM_DIR,  .values = (char **)(uintptr_t)dirs},
    {.name = (char *)(uintptr_t)TEST_FILE_COUNT, .values = (char **)(uintptr_t)file_counts},
    {.name = (char *)(uintptr_t)TEST_FINESSE_OPTION, .values = (char **)(uintptr_t)TEST_FINESSE_OPTIONS},
    {.name = NULL, .values = NULL},
};

static MunitTest fuse_tests[] = {
    TEST((char *)(uintptr_t)"/null", test_null, NULL),
    TEST("/started", test_finess_started, NULL),
    TEST("/opendir", test_open_dir, open_params),
    TEST("/openat", test_openat, open_params),
    TEST("/open/existing-files", test_open_existing_files, open_params),
    TEST("/open/nonexistant-files", test_open_nonexistant_files, open_params),
    TEST("/fstatfs", test_fstatfs, open_params),
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
