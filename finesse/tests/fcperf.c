/*
 * Copyright (c) 2021, Tony Mason. All rights reserved.
 *
 * Test fincomm performance versus message queue
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <finesse.h>
#include <mqueue.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include "../communications/fcinternal.h"
#include "fincomm.h"
#include "finesse_test.h"
#include "munit.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif  //

static const unsigned fcperf_test_message_count = 1024000;

#if 0
typedef MunitResult (* MunitTestFunc)(const MunitParameter params[], void* user_data_or_fixture);
typedef void*       (* MunitTestSetup)(const MunitParameter params[], void* user_data);
typedef void        (* MunitTestTearDown)(void* fixture);
#endif  // 0

static fincomm_shared_memory_region *CreateInMemoryRegion(void)
{
    int                           status;
    fincomm_shared_memory_region *fsmr = (fincomm_shared_memory_region *)malloc(sizeof(fincomm_shared_memory_region));
    assert(NULL != fsmr);

    status = FinesseInitializeMemoryRegion(fsmr);
    assert(0 == status);

    return fsmr;
}

static void DestroyInMemoryRegion(fincomm_shared_memory_region *Fsmr)
{
    int status;

    status = FinesseDestroyMemoryRegion(Fsmr);
    assert(0 == status);

    free(Fsmr);
}

static void *fc_start(const MunitParameter params[], void *user_data)
{
    fincomm_shared_memory_region *fsmr = NULL;

    (void)params;
    (void)user_data;

    // Logic needed to set up the finesse communications layer
    fprintf(stderr, "Start fincomm communications layer\n");

    fsmr = CreateInMemoryRegion();
    munit_assert(NULL != fsmr);

    return (void *)fsmr;
}

static void fc_stop(void *fixture)
{
    // Logic needed to shut down the finesse communications layer
    fprintf(stderr, "Shutdown fincomm communications layer\n");

    munit_assert(NULL != fixture);

    DestroyInMemoryRegion((fincomm_shared_memory_region *)fixture);

    return;
}

typedef struct _mq_info {
    char           name[256];
    uuid_t         uuid;
    mqd_t          queue;
    struct mq_attr attr;
} __mq_info_t;

static void *mq_start(const MunitParameter params[], void *user_data)
{
    __mq_info_t *mqi = NULL;

    (void)params;
    (void)user_data;

    // Logic needed to set up the finesse communications layer
    fprintf(stderr, "Start mq communications layer\n");

    mqi = malloc(sizeof(__mq_info_t));
    munit_assert_not_null(mqi);
    memset(mqi, 0, sizeof(__mq_info_t));

    uuid_generate_time_safe(mqi->uuid);
    uuid_unparse_lower(mqi->uuid, &mqi->name[1]);
    mqi->name[0] = '/';

    mqi->attr.mq_maxmsg  = 1;
    mqi->attr.mq_msgsize = 64;

    mqi->queue = mq_open(mqi->name, O_CREAT | O_EXCL | O_RDWR, 0600, &mqi->attr);

    if (((mqd_t)-1) == mqi->queue) {
        fprintf(stderr, "errno is %d\n", errno);
        perror("mq_open");
    }
    munit_assert(((mqd_t)-1) != mqi->queue);

    return mqi;
}

static void mq_stop(void *fixture)
{
    __mq_info_t *mqi = (__mq_info_t *)fixture;
    // Logic needed to shut down the finesse communications layer
    fprintf(stderr, "Shutdown mq communications layer\n");

    munit_assert_not_null(mqi);

    mq_close(mqi->queue);
    mqi->queue = -1;
    mq_unlink(mqi->name);
    free(mqi);

    return;
}

static MunitResult test_mq(const MunitParameter params[], void *arg)
{
    __mq_info_t *mqi = (__mq_info_t *)arg;
    char         in_buf[64];
    char         out_buf[64];
    int          code;
    unsigned int prio;
    ssize_t      size;

    memset(in_buf, 0xdeadbeef, sizeof(in_buf));
    munit_assert(sizeof(in_buf) == sizeof(out_buf));
    munit_assert(sizeof(in_buf) <= mqi->attr.mq_msgsize);
    (void)params;

    munit_assert(NULL != mqi);

    munit_assert(fcperf_test_message_count > 0);

    for (unsigned index = 0; index < fcperf_test_message_count; index++) {
        memset(out_buf, 0, sizeof(out_buf));
        // Send a message
        code = mq_send(mqi->queue, in_buf, sizeof(in_buf), 0);
        munit_assert(0 == code);

        // Receive a message
        prio = 1;
        size = mq_receive(mqi->queue, out_buf, sizeof(out_buf), &prio);
        munit_assert(size > 0);
        munit_assert(0 == prio);
        munit_assert(0 == memcmp(in_buf, out_buf, sizeof(in_buf)));
    }

    return MUNIT_OK;
}

static MunitResult test_fc(const MunitParameter params[], void *arg)
{
    fincomm_shared_memory_region *fsmr      = (fincomm_shared_memory_region *)arg;
    fincomm_message               fm        = NULL;
    fincomm_message               fm_server = NULL;
    finesse_msg *                 fin_cmsg  = NULL;
    finesse_msg *                 fin_smsg  = NULL;
    int                           status    = 0;
    u_int64_t                     request_id;
    char                          in_buf[64];
    char                          out_buf[64];
    unsigned                      milestone = 1;  // debug aid

    memset(in_buf, 0xdeadbeef, sizeof(in_buf));
    memset(out_buf, 0xbeaddeaf, sizeof(out_buf));

    (void)params;
    (void)arg;

    munit_assert(fcperf_test_message_count > 0);

    for (unsigned index = 0; index < fcperf_test_message_count; index++) {
        //   (1) client allocates a request region (FinesseGetRequestBuffer)
        fprintf(stderr, "milestone %u (line = %d)\n", milestone++, __LINE__);
        fm = FinesseGetRequestBuffer(fsmr, FINESSE_NATIVE_MESSAGE, FINESSE_NATIVE_REQ_TEST);
        munit_assert_not_null(fm);
        fin_cmsg = (finesse_msg *)fm->Data;
        munit_assert(fin_cmsg->Stats.RequestType.Native != 0);

        //   (2) client sets up the request (message->Data)
        memcpy(fin_cmsg->Message.Native.Request.Parameters.Test.Request, in_buf, sizeof(in_buf));

        //   (3) client asks for server notification (FinesseRequestReady)
        fprintf(stderr, "milestone %u (line = %d)\n", milestone++, __LINE__);
        request_id = FinesseRequestReady(fsmr, fm);
        munit_assert(0 != request_id);
        munit_assert(fin_cmsg->Stats.RequestType.Native != 0);

        //   (4) server waits until there's a response to process
        fprintf(stderr, "milestone %u (line = %d)\n", milestone++, __LINE__);
        status = FinesseReadyRequestWait(fsmr);
        munit_assert(0 == status);
        munit_assert(fin_cmsg->Stats.RequestType.Native != 0);

        //   (5) server retrieves message (FinesseGetReadyRequest) - note this is non-blocking!
        fprintf(stderr, "milestone %u (line = %d)\n", milestone++, __LINE__);
        status = FinesseGetReadyRequest(fsmr, &fm_server);
        munit_assert(0 == status);
        munit_assert_not_null(fm_server);
        munit_assert(fm == fm_server);
        fin_smsg = (finesse_msg *)fm_server->Data;
        munit_assert(fin_cmsg->Stats.RequestType.Native != 0);
        munit_assert(sizeof(in_buf) >= sizeof(fin_smsg->Message.Native.Request.Parameters.Test.Request));
        munit_assert(0 == memcmp(in_buf, fin_smsg->Message.Native.Request.Parameters.Test.Request, sizeof(in_buf)));

        //   (6) server constructs response in-place
        munit_assert(sizeof(out_buf) >= sizeof(fin_smsg->Message.Native.Response.Parameters.Test.Response));
        memcpy(fin_smsg->Message.Native.Response.Parameters.Test.Response, out_buf, sizeof(out_buf));
        fin_smsg->Message.Native.Response.NativeResponseType = FINESSE_NATIVE_RSP_TEST;
        munit_assert(fin_cmsg->Stats.RequestType.Native != 0);

        //   (7) server notifies client (FinesseResponseReady)
        fprintf(stderr, "milestone %u (line = %d)\n", milestone++, __LINE__);
        FinesseResponseReady(fsmr, fm_server, 0);

        //   (8) client can poll or block for response (FinesseGetResponse)
        fprintf(stderr, "milestone %u (line = %d)\n", milestone++, __LINE__);
        status = FinesseGetResponse(fsmr, fm, 1);
        munit_assert(0 != status);  // boolean response
        fin_cmsg = (finesse_msg *)fm->Data;
        munit_assert(0 == memcmp(out_buf, fin_cmsg->Message.Native.Response.Parameters.Test.Response, sizeof(out_buf)));
        munit_assert(fin_cmsg->Stats.RequestType.Native != 0);

        //   (9) client frees the request region (FinesseReleaseRequestBuffer)
        fprintf(stderr, "milestone %u (line = %d)\n", milestone++, __LINE__);
        FinesseReleaseRequestBuffer(
            fsmr, fm);  // this is ugly, but we don't have a client control structure (ccs) we have the shared memory pointer
    }

    return MUNIT_OK;
}

static const MunitTest fcperf_tests[] = {
    TEST("/null", test_null, NULL),
    TESTEX("/mq", test_mq, mq_start, mq_stop, MUNIT_TEST_OPTION_NONE, NULL),
    TESTEX("/fc", test_fc, fc_start, fc_stop, MUNIT_TEST_OPTION_NONE, NULL),
    TEST(NULL, NULL, NULL),
};

const MunitSuite perf_suite = {
    .prefix     = (char *)(uintptr_t) "/fcperf",
    .tests      = (MunitTest *)(uintptr_t)fcperf_tests,
    .suites     = NULL,
    .iterations = 1,
    .options    = MUNIT_SUITE_OPTION_NONE,
};

static MunitSuite perftest_suites[10];

MunitSuite *SetupMunitSuites()
{
    memset(perftest_suites, 0, sizeof(perftest_suites));
    perftest_suites[0] = perf_suite;
    return perftest_suites;
}
