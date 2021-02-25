/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
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
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include "fincomm.h"
#include "finesse_test.h"
#include "munit.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif  //

#define TEST_VERSION (0x10)

#define TEST_MEMCPY_SIZE "SIZE"
#define TEST_MEMCPY_DIRECTION "DIRECTION"
#define TEST_MEMCPY_ACCESS "ACCESS"
#define TEST_MEMCPY_ITERATIONS "ITERATIONS"

static const char *TEST_MEMCPY_SIZE_OPTIONS[] = {
    "4096", "65536", "131072", "262144", "524288", "1048576", "2097152", "4194304", NULL,
};
static const char *TEST_MEMCPY_DIRECTION_OPTIONS[] = {"forward", "backward", NULL};
static const char *TEST_MEMCPY_ACCESS_OPTIONS[]    = {"sequential", "random", NULL};
static const char *TEST_MEMCPY_ITERATION_OPTIONS[] = {"1000", NULL};

static MunitParameterEnum memcpy_params[] = {
    {.name = (char *)(uintptr_t)TEST_MEMCPY_SIZE, .values = (char **)(uintptr_t)TEST_MEMCPY_SIZE_OPTIONS},
    {.name = (char *)(uintptr_t)TEST_MEMCPY_DIRECTION, .values = (char **)(uintptr_t)TEST_MEMCPY_DIRECTION_OPTIONS},
    {.name = (char *)(uintptr_t)TEST_MEMCPY_ACCESS, .values = (char **)(uintptr_t)TEST_MEMCPY_ACCESS_OPTIONS},
    {.name = (char *)(uintptr_t)TEST_MEMCPY_ITERATIONS, .values = (char **)(uintptr_t)TEST_MEMCPY_ITERATION_OPTIONS},
    {.name = NULL, .values = NULL},
};

static MunitParameterEnum memset_params[] = {
    {.name = (char *)(uintptr_t)TEST_MEMCPY_SIZE, .values = (char **)(uintptr_t)TEST_MEMCPY_SIZE_OPTIONS},
    {.name = (char *)(uintptr_t)TEST_MEMCPY_ITERATIONS, .values = (char **)(uintptr_t)TEST_MEMCPY_ITERATION_OPTIONS},
    {.name = NULL, .values = NULL},
};

static MunitParameterEnum lk_memcpy_params[] = {
    {.name = (char *)(uintptr_t)TEST_MEMCPY_SIZE, .values = (char **)(uintptr_t)TEST_MEMCPY_SIZE_OPTIONS},
};

#define TESTBUF_SIZE (1024 * 1024 * 1024)
static unsigned char        tb[TESTBUF_SIZE];
static const unsigned char *test_buffer = tb;
static unsigned char        scratch_buffer[TESTBUF_SIZE];

static size_t fill_testbuffer(void)
{
    size_t size = getrandom(tb, sizeof(tb), 0);

    while (size < sizeof(tb)) {
        size_t s = getrandom(&tb[size], sizeof(tb) - size, 0);
        munit_assert(0 != s);
        size += s;
    }

    return size;
}

static MunitResult test_memset(const MunitParameter params[], void *arg)
{
    const char *size_str   = NULL;
    const char *iter_str   = NULL;
    size_t      size       = 64 * 1024;
    int         iterations = 10000;

    (void)arg;

    size_str = munit_parameters_get(params, TEST_MEMCPY_SIZE);
    if (NULL != size_str) {
        sscanf(size_str, "%lu", &size);
        munit_assert(size >= 1);                   // minimum size
        munit_assert(size <= 1024 * 1024 * 1024);  // maximum size
    }

    iter_str = munit_parameters_get(params, TEST_MEMCPY_ITERATIONS);
    if (NULL != iter_str) {
        sscanf(iter_str, "%d", &iterations);
        munit_assert(iterations > 0);         // at least one
        munit_assert(iterations <= 1000000);  // maximum
    }

    munit_assert(sizeof(scratch_buffer) >= size);

    for (unsigned int index = 0; index < iterations; index++) {
        memset(scratch_buffer, index, size);
#if 0
        for (unsigned int index2 = size; index2 > 0; index2--) {
            if ((index & 0xFF) != scratch_buffer[index2 - 1]) {
                munit_logf(MUNIT_LOG_ERROR, "Mismatch buffer contents, index = %u, index2 = %u, value is %u, size is %lu\n", index,
                           index2, scratch_buffer[index2 - 1], size);
            }
            // munit_assert(index == scratch_buffer[index2-1]);
        }
#endif  // 0
    }

    return MUNIT_OK;
}

static MunitResult test_memset_verify(const MunitParameter params[], void *arg)
{
    const char *size_str   = NULL;
    const char *iter_str   = NULL;
    size_t      size       = 64 * 1024;
    int         iterations = 10000;

    (void)arg;

    size_str = munit_parameters_get(params, TEST_MEMCPY_SIZE);
    if (NULL != size_str) {
        sscanf(size_str, "%lu", &size);
        munit_assert(size >= 1);                   // minimum size
        munit_assert(size <= 1024 * 1024 * 1024);  // maximum size
    }

    iter_str = munit_parameters_get(params, TEST_MEMCPY_ITERATIONS);
    if (NULL != iter_str) {
        sscanf(iter_str, "%d", &iterations);
        munit_assert(iterations > 0);         // at least one
        munit_assert(iterations <= 1000000);  // maximum
    }

    munit_assert(sizeof(scratch_buffer) >= size);

    for (unsigned int index = 0; index < iterations; index++) {
        memset(scratch_buffer, index, size);
        for (unsigned int index2 = size; index2 > 0; index2--) {
            if ((index & 0xFF) != scratch_buffer[index2 - 1]) {
                munit_logf(MUNIT_LOG_ERROR, "Mismatch buffer contents, index = %u, index2 = %u, value is %u, size is %lu\n", index,
                           index2, scratch_buffer[index2 - 1], size);
            }
        }
    }

    return MUNIT_OK;
}

typedef void *(*copy_test_t)(void *destination, const void *source, size_t size);

static void *test_memcpy_random(void *destination, const void *source, size_t size)
{
    // TODO: implement a random variant of this
    return memcpy(destination, source, size);
}

static void *test_memmove_random(void *destination, const void *source, size_t size)
{
    // TODO: implement a random variant of this
    return memmove(destination, source, size);
}

// This is the ERMS memcpy of the Linux kernel
// https://stackoverflow.com/questions/43343231/enhanced-rep-movsb-for-memcpy
// (I verified this is the current version of Linux as of 2020/02/23)
//
static void rep_movsb(void *dst, const void *src, size_t n)
{
    __asm__ __volatile__("rep movsb\n\t" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
}

static MunitResult test_lk_memcpy(const MunitParameter params[], void *arg)
{
    const char *size_str = NULL;
    size_t      size     = 4096;

    munit_assert(size >= 4096);
    munit_assert(size <= sizeof(tb));

    (void)arg;
    size_str = munit_parameters_get(params, TEST_MEMCPY_SIZE);

    if (NULL != size_str) {
        sscanf(size_str, "%lu", &size);
    }

    munit_assert(size >= 4096);
    munit_assert(size <= sizeof(tb));

    fill_testbuffer();

    for (off_t offset = 0; (offset + size) <= sizeof(tb); offset += size) {
        // So we're going to copy in the specified block sizes
        rep_movsb((void *)&scratch_buffer[offset], (void *)&test_buffer[offset], size);
    }

    return MUNIT_OK;
}

static MunitResult test_sanity_memcpy(const MunitParameter params[], void *arg)
{
    size_t         offset     = 0;
    const unsigned iterations = 1;
    const size_t   size       = 4096;

    (void)arg;
    (void)params;

    for (unsigned index = 0; index < iterations; index++) {
        offset = 0;
        while ((offset + size) <= sizeof(tb)) {
            memmove((void *)&scratch_buffer[offset], (void *)&tb[offset], size);
            offset += size;
        }
    }

    return MUNIT_OK;
}

static MunitResult test_sanity_lk_memcpy(const MunitParameter params[], void *arg)
{
    size_t         offset     = 0;
    const unsigned iterations = 1;
    const size_t   size       = 4096;

    (void)arg;
    (void)params;

    for (unsigned index = 0; index < iterations; index++) {
        offset = 0;
        while ((offset + size) <= sizeof(tb)) {
            rep_movsb((void *)&scratch_buffer[offset], (void *)&tb[offset], size);
            offset += size;
        }
    }

    return MUNIT_OK;
}

static copy_test_t copytests[2][2] = {  // axis 1 = access, axis 2 = direction
    {
        memcpy,              // sequential, forward
        test_memcpy_random,  // random, forward
    },
    {
        memmove,              // sequential, backward
        test_memmove_random,  // random, backward
    }};

static MunitResult test_memcpy(const MunitParameter params[], void *arg)
{
    const char *dir_str    = NULL;
    const char *size_str   = NULL;
    const char *access_str = NULL;
    const char *iter_str   = NULL;
    int         direction  = 0;
    size_t      size       = 64 * 1024;
    int         access     = 0;
    copy_test_t copy       = memcpy;
    int         iterations = 1000;

    (void)arg;

    size_str = munit_parameters_get(params, TEST_MEMCPY_SIZE);
    if (NULL != size_str) {
        sscanf(size_str, "%lu", &size);
    }

    munit_assert(sizeof(scratch_buffer) >= size);

    dir_str = munit_parameters_get(params, TEST_MEMCPY_DIRECTION);
    if (NULL != dir_str) {
        direction = -1;
        for (unsigned index = 0; NULL != TEST_MEMCPY_DIRECTION_OPTIONS[index]; index++) {
            if (0 == strcmp(dir_str, TEST_MEMCPY_DIRECTION_OPTIONS[index])) {
                direction = index;
            }
        }
        munit_assert(direction >= 0);
    }

    access_str = munit_parameters_get(params, TEST_MEMCPY_ACCESS);
    if (NULL != access_str) {
        access = -1;
        for (unsigned index = 0; NULL != TEST_MEMCPY_ACCESS_OPTIONS[index]; index++) {
            if (0 == strcmp(access_str, TEST_MEMCPY_ACCESS_OPTIONS[index])) {
                access = index;
            }
        }
        munit_assert(access >= 0);
    }

    iter_str = munit_parameters_get(params, TEST_MEMCPY_ITERATIONS);
    if (NULL != iter_str) {
        sscanf(iter_str, "%d", &iterations);
        munit_assert(iterations > 0);         // at least one
        munit_assert(iterations <= 1000000);  // maximum
    }

    copy = copytests[access][direction];

    if ((0 == access) && (0 == direction))
        munit_assert(copy == memcpy);
    if ((1 == access) && (0 == direction))
        munit_assert(copy == memmove);

    for (unsigned index = 0; index < iterations; index++) {
        munit_assert(scratch_buffer == copy(scratch_buffer, test_buffer, size));
    }

    return MUNIT_OK;
}

static MunitResult test_basic(const MunitParameter params[], void *arg)
{
    const int    iterations = 1;
    const size_t size       = sizeof(tb);

    (void)params;
    (void)arg;

    for (unsigned index = 0; index < iterations; index++) {
        munit_assert(sizeof(tb) == fill_testbuffer());
        munit_assert(scratch_buffer == memcpy(scratch_buffer, test_buffer, size));
    }

    return MUNIT_OK;
}

static MunitResult test_basic2(const MunitParameter params[], void *arg)
{
    const int    iterations = 1;
    const size_t size       = sizeof(tb);

    (void)params;
    (void)arg;

    for (unsigned index = 0; index < iterations; index++) {
        munit_assert(sizeof(tb) == fill_testbuffer());
        munit_assert(scratch_buffer == memmove(scratch_buffer, test_buffer, size));
    }

    return MUNIT_OK;
}

static MunitResult test_basic3(const MunitParameter params[], void *arg)
{
    const int    iterations = 1;
    const size_t size       = sizeof(tb);

    (void)params;
    (void)arg;

    for (unsigned index = 0; index < iterations; index++) {
        munit_assert(sizeof(tb) == fill_testbuffer());
        rep_movsb(scratch_buffer, test_buffer, size);
    }

    return MUNIT_OK;
}

static const MunitTest perf_tests[] = {
    TEST("/null", test_null, NULL),
    TEST("/basic", test_basic, NULL),
    TEST("/basic2", test_basic2, NULL),
    TEST("/basic3", test_basic3, NULL),
    TEST("/memset", test_memset, memset_params),
    TEST("/memset_verify", test_memset_verify, memset_params),
    TEST("/memcpy", test_memcpy, memcpy_params),
    TEST("/lk_memcpy", test_lk_memcpy, lk_memcpy_params),
    TEST("/sanity/memcpy", test_sanity_memcpy, NULL),
    TEST("/sanity/lk_memcpy", test_sanity_lk_memcpy, NULL),
    TEST(NULL, NULL, NULL),
};

const MunitSuite perf_suite = {
    .prefix     = (char *)(uintptr_t) "/perf",
    .tests      = (MunitTest *)(uintptr_t)perf_tests,
    .suites     = NULL,
    .iterations = 1,
    .options    = MUNIT_SUITE_OPTION_NONE,
};

static MunitSuite perftest_suites[10];

MunitSuite *SetupMunitSuites()
{
    munit_assert(sizeof(tb) == fill_testbuffer());
    memset(perftest_suites, 0, sizeof(perftest_suites));
    perftest_suites[0] = perf_suite;
    return perftest_suites;
}
