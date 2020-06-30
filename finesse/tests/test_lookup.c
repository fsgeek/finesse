/*
 * Copyright (c) 2017-2020, Tony Mason. All rights reserved.
 */

#include "test_utils.h"
#include "../api/api-internal.h"
#include <signal.h>

#define __packed __attribute__((packed))
#define __notused __attribute__((unused))


static MunitResult test_lookup_table_create(const MunitParameter params[], void *arg)
{

    (void) params;
    (void) arg;

    munit_assert(0 == finesse_init_file_state_mgr());

    finesse_terminate_file_state_mgr();

    return MUNIT_OK;
}

typedef struct {
    int min_fd;
    int max_fd;
    unsigned lookup_iterations;
    finesse_client_handle_t client_handle;
} lt_test_params_t;

static void * ltworker(void *arg)
{
    finesse_file_state_t *fs = NULL;
    lt_test_params_t *params = (lt_test_params_t *)arg;
    char buf[128];
    char buf2[128];

    while (NULL != params) {

        // First - let's create them
        for (int index = params->min_fd; index <= params->max_fd; index++) {
            uuid_t uuid;

            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf), "/test/%016u", index);
            uuid_generate(uuid);
            fs = finesse_create_file_state(index, params->client_handle, (void *)&uuid, buf);

            assert (NULL != fs);
        }

        // now let's read them back
        for (unsigned count = 0; count < params->lookup_iterations; count++) {
            for (int index = params->min_fd; index <= params->max_fd; index++) {

                memset(buf2, 0, sizeof(buf2));
                snprintf(buf2, sizeof(buf2), "/test/%016u", index);
                fs = finesse_lookup_file_state(index);
                assert(NULL != fs);
                assert(fs->fd == index);
                assert(0 == strcmp(buf2, fs->pathname));        
            }
        }

        // mow let's delete them
        for (int index = params->min_fd; index <= params->max_fd; index++) {
            if (0 == (index & 0x1)) {
                continue; // skip evens
            }

            fs = finesse_lookup_file_state(index);
            assert(NULL != fs);
            finesse_delete_file_state(fs);

            fs = finesse_lookup_file_state(index);
            assert(NULL == fs);
        }

        // let's make sure we can still find the even ones
        for (int index = params->min_fd; index <= params->max_fd; index++) {
            if (0 != (index & 0x1)) {
                continue;
            }

            fs = finesse_lookup_file_state(index);
            assert(NULL != fs);
            finesse_delete_file_state(fs);
        }

        break;
        
    }

    // pthread_exit(NULL);
    return NULL;
}


static MunitResult test_lookup_table(const MunitParameter params[], void *arg)
{
    static const int fd_max_test = 65536;
    unsigned thread_count = 12;
    pthread_t threads[thread_count];
    pthread_attr_t thread_attr;
    int status;
    lt_test_params_t test_params[thread_count];

    (void) params;
    (void) arg;

    pthread_attr_init(&thread_attr);

    memset(&threads, 0, sizeof(threads));
    
    munit_assert(0 == finesse_init_file_state_mgr());

    for (unsigned index = 0; index < thread_count; index++) {
        if (index > 0) {
            test_params[index].min_fd = test_params[index-1].max_fd + 1;
        }
        else {
            test_params[index].min_fd = 0;
        }

        if (index < thread_count - 1) {
            test_params[index].max_fd = test_params[index].min_fd + fd_max_test / thread_count;
        }
        else {
            test_params[index].max_fd = fd_max_test;
        }
        test_params[index].lookup_iterations = 100;
        test_params[index].client_handle = (void *)(uintptr_t)(1 + index); // just can't be zero
        status = pthread_create(&threads[index], &thread_attr, ltworker, &test_params[index]);
        munit_assert(0 == status);
    }

    pthread_attr_destroy(&thread_attr);

    for (unsigned index = 0; index < thread_count; index++) {
        void *result;
        status = pthread_join(threads[index], &result);
        munit_assert(0 == status);
        munit_assert(NULL == result);
    }
    
    finesse_terminate_file_state_mgr();

    return MUNIT_OK;
}

static MunitTest tests[] = {
    TEST("/null", test_null, NULL),
    TEST("/lookup/create", test_lookup_table_create, NULL),
    TEST("/lookup/test_table", test_lookup_table, NULL),
    TEST(NULL, NULL, NULL),
};

const MunitSuite lookup_suite = {
    .prefix = (char *)(uintptr_t)"/lookup",
    .tests = tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};



/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
