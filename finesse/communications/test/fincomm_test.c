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
#include "finesse_test.h"
#include "../lib/fincomm.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif // 

#if 0
typedef struct {
    uuid_t          ClientId;
    uuid_t          ServerId;
    u_int64_t       RequestBitmap;
    pthread_mutex_t RequestMutex;
    pthread_cond_t  RequestPending;
    u_char          align0[128-((2 * sizeof(uuid_t)) + sizeof(u_int64_t) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t))];
    u_int64_t       ResponseBitmap;
    u_int64_t       ResponseStatus;
    pthread_mutex_t ResponseMutex;
    pthread_cond_t  ResponsePending;
    u_char          align1[128-(2 * sizeof(u_int64_t) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t))];
    char            secondary_shm_path[MAX_SHM_PATH_NAME];
    u_char          Data[4096-(3*128)];
    fincomm_message_block   Messages[SHM_MESSAGE_COUNT];
} fincomm_shared_memory_region;
#endif // 0

static fincomm_shared_memory_region *CreateInMemoryRegion(void)
{
    int status;
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

#if 0
//
// This is the shared memory protocol:
//   (1) client allocates a request region (FinesseGetRequestBuffer)
//   (2) client sets up the request (message->Data)
//   (3) client asks for server notification (FinesseRequestReady)
//   (4) server retrieves message (FinesseGetReadyRequest)
//   (5) server constructs response in-place
//   (6) server notifies client (FinesseResponseReady)
//   (7) client can poll or block for response (FinesseGetResponse)
//   (8) client frees the request region (FinesseReleaseRequestBuffer)
//
// The goal is, as much as possible, to avoid synchronization. While I'm using condition variables
// now, I was thinking it might be better to use the IPC channel for sending messages, but
// I'm not going to address that today.
//
fincomm_message FinesseGetRequestBuffer(fincomm_shared_memory_region *RequestRegion);
u_int64_t FinesseRequestReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message);
void FinesseResponseReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, uint32_t Response);
int FinesseGetResponse(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, int wait);
fincomm_message FinesseGetReadyRequest(fincomm_shared_memory_region *RequestRegion);
void FinesseReleaseRequestBuffer(fincomm_shared_memory_region *RequestRegion, fincomm_message Message);

#endif // 0

static MunitResult
test_message(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    fincomm_shared_memory_region *fsmr;
    fincomm_message fm;
    fincomm_message fm_server;
    char test_message[64] = "This is a test message";
    char test_response[64] = "This is a test response";
    u_int64_t request_id;
    int status;

    fsmr = CreateInMemoryRegion();
    munit_assert_not_null(fsmr);

    //   (1) client allocates a request region (FinesseGetRequestBuffer)
    fm = FinesseGetRequestBuffer(fsmr);
    munit_assert_not_null(fm);

    //   (2) client sets up the request (message->Data)
    memcpy(fm->Data, test_message, sizeof(test_message));

    //   (3) client asks for server notification (FinesseRequestReady)
    request_id = FinesseRequestReady(fsmr, fm);
    munit_assert(0 != request_id);

    //   (4) server retrieves message (FinesseGetReadyRequest)
    fm_server = FinesseGetReadyRequest(fsmr);
    munit_assert_not_null(fm_server);
    munit_assert(0 == memcmp(test_message, fm_server->Data, sizeof(test_message)));

    //   (5) server constructs response in-place
    memcpy(fm_server->Data, test_response, sizeof(test_response));

    //   (6) server notifies client (FinesseResponseReady)
    FinesseResponseReady(fsmr, fm_server, 0);

    //   (7) client can poll or block for response (FinesseGetResponse)
    status = FinesseGetResponse(fsmr, fm, 1);
    munit_assert(0 != status); // boolean response
    munit_assert(0 == memcmp(test_response, fm->Data, sizeof(test_response)));

    //   (8) client frees the request region (FinesseReleaseRequestBuffer)
    FinesseReleaseRequestBuffer(fsmr, fm);

    // cleanup
    DestroyInMemoryRegion(fsmr);
    fsmr = NULL;

    return MUNIT_OK;
}
 
static MunitResult
test_client_server(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    return MUNIT_OK;
}


static MunitTest fincomm_tests[] = {
    TEST((char *)(uintptr_t)"/null", test_null, NULL),
    TEST((char *)(uintptr_t)"/simple", test_message, NULL),
    TEST((char *)(uintptr_t)"/client-server", test_client_server, NULL),
    TEST(NULL, NULL, NULL),
};


const MunitSuite fincomm_suite = {
    .prefix = (char *)(uintptr_t)"/fincomm",
    .tests = fincomm_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};

#if 0
int
main(
    int argc,
    char **argv)
{
    static MunitTest fincomm_tests[] = {
        TEST((char *)(uintptr_t)"/null", test_null, NULL),
    	TEST(NULL, NULL, NULL),
    };

    static const MunitSuite suite = {
        .prefix = (char *)(uintptr_t)"/finesse",
        .tests = finesse_tests,
        .suites = NULL,
        .iterations = 1,
        .options = MUNIT_SUITE_OPTION_NONE,
    };

    static const MunitSuite fincomm_suite = {
        .prefix = (char *)(uintptr_t)"/fincomm",
        .tests = fincomm_tests,
        .suites = NULL,
        .iterations = 1,
        .options = MUNIT_SUITE_OPTION_NONE,
    };

    int status;

    status = munit_suite_main(&suite, NULL, argc, argv);

    if (0 != status) {
        exit(0);
    }

    status = munit_suite_main(&fincomm_suite, NULL, argc, argv);

    return status;
}
#endif // 0

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
