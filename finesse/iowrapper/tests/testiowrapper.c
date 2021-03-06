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

#include "../iowrapper.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif  //

#define TEST(_name, _func, _params)                                                                                               \
    {                                                                                                                             \
        .name = (char *)(uintptr_t)(_name), .test = (_func), .setup = NULL, .tear_down = NULL, .options = MUNIT_TEST_OPTION_NONE, \
        .parameters = (_params),                                                                                                  \
    }

extern const MunitSuite fincomm_suite;
extern const MunitSuite finesse_suite;
extern const MunitSuite testutils_suite;
extern const MunitSuite fuse_suite;

extern MunitResult test_null(const MunitParameter params[], void *prv);
extern MunitSuite *SetupMunitSuites(void);

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif  //

static void get_random_data(void *buffer, size_t size)
{
    static int fd = -1;
    ssize_t    bytes_read;

    if (-1 == fd) {
        fd = open("/dev/urandom", O_RDONLY);
    }

    munit_assert(fd >= 0);

    bytes_read = read(fd, buffer, size);
    munit_assert(bytes_read == size);

    return;
}

static int generate_temp_file(char *buffer, size_t buffer_size, size_t size)
{
    int     bytes = snprintf(buffer, buffer_size, "/tmp/log-%d-XXXXXXXX.tio", getpid());
    int     fd;
    size_t  written = 0;
    char    random_buffer[8192];
    ssize_t bytes_written;

    munit_assert(bytes > 0);
    munit_assert(bytes < buffer_size);

    fd = mkstemps(buffer, 4);
    munit_assert(fd >= 0);

    while (written < size) {
        size_t rdsize = sizeof(random_buffer);

        if (written + rdsize > size) {
            munit_assert(size > written);
            rdsize = size - written;
        }

        munit_assert(rdsize <= sizeof(random_buffer));
        get_random_data(random_buffer, rdsize);

        bytes_written = write(fd, random_buffer, rdsize);
        munit_assert(bytes_written == rdsize);
        written += bytes_written;
    }

    return fd;
}

MunitResult test_null(const MunitParameter params[] __notused, void *prv __notused)
{
    return MUNIT_OK;
}

#if 0
typedef struct _iowraptest_files {
    char   path[256];
    size_t size;
} iowraptest_files_t;

static iowraptest_files_t *generate_files(unsigned count, size_t min, size_t max)
{
    long int size;
    int      fd;

    fd = generate_temp_file(fname, sizeof(fname));
    munit_assert(fd >= 0);

    size = (min + random) % max;
}
#endif  // 0

static MunitResult test_open(const MunitParameter params[] __notused, void *prv __notused)
{
    char   fname[256];
    int    fd;
    size_t size = 8192;
    size_t min  = 8192;
    size_t max  = 20 * 1024 * 1024;  // 20 MB max

    get_random_data(&size, sizeof(size));

    size %= (max - min);
    size += min;

    fd = generate_temp_file(fname, sizeof(fname), size);
    if (fd >= 0) {
        close(fd);
    }

    fd = iowrap_open(fname, O_RDONLY);
    munit_assert(fd > 0);

    iowrap_close(fd);

    return MUNIT_OK;
}

extern MunitSuite *SetupMunitSuites(void);

int main(int argc, char **argv)
{
    MunitSuite suite;

    suite.prefix     = (char *)(uintptr_t) "/finesse";
    suite.tests      = NULL;
    suite.suites     = SetupMunitSuites();
    suite.iterations = 1;
    suite.options    = MUNIT_SUITE_OPTION_NONE;

    return munit_suite_main(&suite, NULL, argc, argv);
}

static MunitTest iowrap_tests[] = {
    TEST((char *)(uintptr_t) "/null", test_null, NULL),
    TEST((char *)(uintptr_t) "/open", test_open, NULL),
    TEST(NULL, NULL, NULL),
};

const MunitSuite iowrap_suite = {
    .prefix     = (char *)(uintptr_t) "/iowrap",
    .tests      = iowrap_tests,
    .suites     = NULL,
    .iterations = 1,
    .options    = MUNIT_SUITE_OPTION_NONE,
};

static MunitSuite testutils_suites[10];

MunitSuite *SetupMunitSuites()
{
    unsigned index = 0;

    memset(testutils_suites, 0, sizeof(testutils_suites));
    testutils_suites[index++] = iowrap_suite;
    return testutils_suites;
}
