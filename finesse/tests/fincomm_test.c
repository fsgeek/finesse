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
#include "../communications/fcinternal.h"
#include "fincomm.h"
#include "finesse_test.h"
#include "munit.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif  //

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

static MunitResult test_message(const MunitParameter params[] __notused, void *prv __notused)
{
    fincomm_shared_memory_region *fsmr;
    fincomm_message               fm;
    fincomm_message               fm_server;
    char                          test_message[64]  = "This is a test message";
    char                          test_response[64] = "This is a test response";
    u_int64_t                     request_id;
    int                           status;
    finesse_msg *                 fin_cmsg;
    finesse_msg *                 fin_smsg;

    fsmr = CreateInMemoryRegion();
    munit_assert_not_null(fsmr);

    //   (1) client allocates a request region (FinesseGetRequestBuffer)
    fm = FinesseGetRequestBuffer(fsmr);
    munit_assert_not_null(fm);
    fin_cmsg = (finesse_msg *)fm->Data;

    //   (2) client sets up the request (message->Data)
    memcpy(fin_cmsg->Message.Native.Request.Parameters.Test.Request, test_message, sizeof(test_message));

    //   (3) client asks for server notification (FinesseRequestReady)
    request_id = FinesseRequestReady(fsmr, fm);
    munit_assert(0 != request_id);

    //   (4) server waits until there's a response to process
    status = FinesseReadyRequestWait(fsmr);
    munit_assert(0 == status);

    //   (5) server retrieves message (FinesseGetReadyRequest) - note this is non-blocking!
    status = FinesseGetReadyRequest(fsmr, &fm_server);
    munit_assert(0 == status);
    munit_assert_not_null(fm_server);
    munit_assert(fm == fm_server);
    fin_smsg = (finesse_msg *)fm_server->Data;
    munit_assert(sizeof(test_message) <= sizeof(fin_smsg->Message.Native.Request.Parameters.Test.Request));
    munit_assert(0 == memcmp(test_message, fin_smsg->Message.Native.Request.Parameters.Test.Request, sizeof(test_message)));

    //   (6) server constructs response in-place
    munit_assert(sizeof(test_response) <= sizeof(fin_smsg->Message.Native.Response.Parameters.Test.Response));
    memcpy(fin_smsg->Message.Native.Response.Parameters.Test.Response, test_response, sizeof(test_response));

    //   (7) server notifies client (FinesseResponseReady)
    FinesseResponseReady(fsmr, fm_server, 0);

    //   (8) client can poll or block for response (FinesseGetResponse)
    status = FinesseGetResponse(fsmr, fm, 1);
    munit_assert(0 != status);  // boolean response
    fin_cmsg = (finesse_msg *)fm->Data;
    munit_assert(0 == memcmp(test_response, fin_cmsg->Message.Native.Response.Parameters.Test.Response, sizeof(test_response)));

    //   (9) client frees the request region (FinesseReleaseRequestBuffer)
    FinesseReleaseRequestBuffer(fsmr, fm);

    // cleanup
    DestroyInMemoryRegion(fsmr);
    fsmr = NULL;

    return MUNIT_OK;
}

struct cs_info {
    char                          request_message[64];
    char                          response_message[64];
    fincomm_shared_memory_region *shared_mem;
    pthread_mutex_t               mutex;
    pthread_cond_t                cond;
    unsigned                      count;
    unsigned                      ready;
};

static void *server_thread(void *param)
{
    struct cs_info *              cs_info = (struct cs_info *)param;
    fincomm_shared_memory_region *fsmr;
    fincomm_message               message;
    int                           status;
    unsigned                      count = 0;
    finesse_msg *                 fin_smsg;

    assert(NULL != param);
    fsmr = cs_info->shared_mem;
    assert(NULL != fsmr);
    // wait for the main thread to say it is ok to proceed
    status = pthread_mutex_lock(&cs_info->mutex);
    assert(0 == status);
    status = pthread_cond_wait(&cs_info->cond, &cs_info->mutex);
    assert(0 == status);
    status = pthread_mutex_unlock(&cs_info->mutex);
    assert(0 == status);

    for (;;) {
        // Wait for a request to process
        status = FinesseReadyRequestWait(fsmr);

        if (ENOTCONN == status) {
            // indicates a shutdown request
            break;
        }

        // Get the request
        status = FinesseGetReadyRequest(fsmr, &message);

        if (0 != status) {
            munit_logf(MUNIT_LOG_ERROR, "FinesseGetReadyRequest returned 0x%x (%d)", status, status);
        }
        munit_assert(NULL != message);
        munit_logf(MUNIT_LOG_INFO, "Server %lu has message at 0x%p\n", pthread_self(), (void *)message);

        fin_smsg = (finesse_msg *)message->Data;

        if (0 != memcmp(cs_info->request_message, fin_smsg->Message.Native.Request.Parameters.Test.Request,
                        sizeof(cs_info->request_message))) {
            munit_errorf("Mismatched request (0x%p) data, got %s, expected %s\n", (void *)message,
                         fin_smsg->Message.Native.Request.Parameters.Test.Request, cs_info->request_message);
            munit_assert(0);
        }

        munit_assert(sizeof(fin_smsg->Message.Native.Response.Parameters.Test.Response) >= sizeof(cs_info->response_message));
        memcpy(fin_smsg->Message.Native.Response.Parameters.Test.Response, cs_info->response_message,
               sizeof(cs_info->response_message));
        FinesseResponseReady(fsmr, message, 0);
        count++;
    }

    return NULL;
}

static void *client_thread(void *param)
{
    struct cs_info *              cs_info = (struct cs_info *)param;
    fincomm_shared_memory_region *fsmr;
    fincomm_message               message;
    int                           status;
    u_int64_t                     request_id;
    unsigned                      count = 0;
    finesse_msg *                 fin_cmsg;

    assert(NULL != param);
    fsmr = cs_info->shared_mem;
    assert(NULL != fsmr);
    // wait for the main thread to say it is ok to proceed
    status = pthread_mutex_lock(&cs_info->mutex);
    assert(0 == status);
    while (0 == cs_info->ready) {
        status = pthread_cond_wait(&cs_info->cond, &cs_info->mutex);
        assert(0 == status);
    }
    status = pthread_mutex_unlock(&cs_info->mutex);
    assert(0 == status);

    while (count < cs_info->count) {
        message = FinesseGetRequestBuffer(fsmr);
        munit_assert_not_null(message);
        munit_logf(MUNIT_LOG_INFO, "Client %lu has message at 0x%p\n", pthread_self(), (void *)message);
        fin_cmsg = (finesse_msg *)message->Data;
        munit_assert(sizeof(fin_cmsg->Message.Native.Request.Parameters.Test.Request) <= sizeof(cs_info->request_message));
        memcpy(fin_cmsg->Message.Native.Request.Parameters.Test.Request, cs_info->request_message,
               sizeof(cs_info->request_message));

        //   (3) client asks for server notification (FinesseRequestReady)
        request_id = FinesseRequestReady(fsmr, message);
        munit_assert(0 != request_id);

        //   (7) client can poll or block for response (FinesseGetResponse)
        status = FinesseGetResponse(fsmr, message, 1);
        munit_assert(0 != status);  // boolean response
        munit_assert(0 == memcmp(cs_info->response_message, fin_cmsg->Message.Native.Response.Parameters.Test.Response,
                                 sizeof(cs_info->response_message)));

        //   (8) client frees the request region (FinesseReleaseRequestBuffer)
        memset(fin_cmsg->Message.Native.Response.Parameters.Test.Response, 'A',
               sizeof(fin_cmsg->Message.Native.Response.Parameters.Test.Response));
        FinesseReleaseRequestBuffer(fsmr, message);

        count++;
    }

    return NULL;
}

static MunitResult test_client_server(const MunitParameter params[] __notused, void *prv __notused)
{
    struct cs_info                cs_info;
    fincomm_shared_memory_region *fsmr;
    pthread_t                     client, server;
    int                           status;

    fsmr = CreateInMemoryRegion();
    munit_assert_not_null(fsmr);

    strcpy(cs_info.request_message, "This is a request message");
    strcpy(cs_info.response_message, "This is a response message");
    cs_info.shared_mem = fsmr;
    status             = pthread_mutex_init(&cs_info.mutex, NULL);
    assert(0 == status);
    status = pthread_cond_init(&cs_info.cond, NULL);
    assert(0 == status);
    cs_info.count = 1;
    cs_info.ready = 0;

    status = pthread_create(&server, NULL, server_thread, &cs_info);
    assert(0 == status);
    sleep(1);

    status = pthread_mutex_lock(&cs_info.mutex);
    assert(0 == status);

    status = pthread_create(&client, NULL, client_thread, &cs_info);
    assert(0 == status);

    cs_info.ready = 1;

    status = pthread_cond_broadcast(&cs_info.cond);
    assert(0 == status);

    status = pthread_mutex_unlock(&cs_info.mutex);
    assert(0 == status);

    sleep(1);

    // shutdown the threads
    // cs_info.shutdown = 1;

    status = pthread_join(client, NULL);
    assert(0 == status);

    // cleanup
    DestroyInMemoryRegion(fsmr);
    fsmr = NULL;

    return MUNIT_OK;
}

static MunitResult test_invalid_message_request(const MunitParameter params[] __notused, void *prv __notused)
{
    fincomm_shared_memory_region *fsmr;
    fincomm_message               message;
    u_int64_t                     request_id;

    fsmr = CreateInMemoryRegion();
    munit_assert_not_null(fsmr);

    message = FinesseGetRequestBuffer(fsmr);
    munit_assert_not_null(message);

    FinesseReleaseRequestBuffer(fsmr, message);

    //   (3) client asks for server notification (FinesseRequestReady)
    request_id = FinesseRequestReady(fsmr, message);
    munit_assert(0 == request_id);

    // cleanup
    DestroyInMemoryRegion(fsmr);
    fsmr = NULL;

    return MUNIT_OK;
}

static MunitResult test_multi_client(const MunitParameter params[] __notused, void *prv __notused)
{
    struct cs_info                cs_info;
    fincomm_shared_memory_region *fsmr;
    pthread_t                     server, clients[128];
    int                           status;
    unsigned                      client_count = 8;

    memset(clients, 0, sizeof(clients));

    fsmr = CreateInMemoryRegion();
    munit_assert_not_null(fsmr);

    strcpy(cs_info.request_message, "This is a request message");
    strcpy(cs_info.response_message, "This is a response message");
    cs_info.shared_mem = fsmr;
    status             = pthread_mutex_init(&cs_info.mutex, NULL);
    assert(0 == status);
    status = pthread_cond_init(&cs_info.cond, NULL);
    assert(0 == status);
    cs_info.count = 10;

    status = pthread_create(&server, NULL, server_thread, &cs_info);
    assert(0 == status);
    sleep(1);

    for (unsigned index = 0; index < client_count; index++) {
        status = pthread_create(&clients[index], NULL, client_thread, &cs_info);
        assert(0 == status);
    }
    sleep(1);

    // Tell the threads it's OK to proceed
    status = pthread_mutex_lock(&cs_info.mutex);
    assert(0 == status);
    cs_info.ready = 1;
    status        = pthread_cond_broadcast(&cs_info.cond);
    assert(0 == status);
    status = pthread_mutex_unlock(&cs_info.mutex);
    assert(0 == status);

    sleep(1);

    for (unsigned index = 0; index < client_count; index++) {
        status = pthread_join(clients[index], NULL);
        assert(0 == status);
    }

    // cleanup
    DestroyInMemoryRegion(fsmr);
    fsmr = NULL;

    return MUNIT_OK;
}

static MunitResult test_buffer(const MunitParameter params[] __notused, void *prv __notused)
{
    char *                 test_name = (char *)(uintptr_t) "finesse_test";
    fincomm_arena_handle_t arena;
    void *                 buffer = NULL;
    char                   name_buffer[64];
    size_t                 size;
    size_t                 count;
    fincomm_arena_handle_t arena2;

    arena  = FincommCreateArena(test_name, 64 * 1024 * 1024, 64);
    buffer = FincommAllocateBuffer(arena);
    munit_assert(NULL != buffer);
    FincommFreeBuffer(arena, buffer);

    FincommGetArenaInfo(arena, name_buffer, sizeof(name_buffer), &size, &count);
    fprintf(stderr, "test name is %s\n", name_buffer);
    munit_assert(0 == strcmp(test_name, name_buffer));
    munit_assert(64 * 1024 * 1024 == size);
    munit_assert(64 == count);

    arena2 = FincommCreateArena(test_name, 64 * 1024 * 1024, 64);
    FincommReleaseArena(arena2);
    FincommReleaseArena(arena);

    return MUNIT_OK;
}

static MunitResult test_namemap(const MunitParameter params[] __notused, void *prv __notused)
{
    return MUNIT_OK;
}

static MunitTest fincomm_tests[] = {
    TEST((char *)(uintptr_t) "/null", test_null, NULL),
    TEST((char *)(uintptr_t) "/simple", test_message, NULL),
    TEST((char *)(uintptr_t) "/client-server", test_client_server, NULL),
    TEST((char *)(uintptr_t) "/invalid-message", test_invalid_message_request, NULL),
    TEST((char *)(uintptr_t) "/multi-client", test_multi_client, NULL),
    TEST((char *)(uintptr_t) "/buffer", test_buffer, NULL),
    TEST((char *)(uintptr_t) "/namemap", test_namemap, NULL),
    TEST(NULL, NULL, NULL),
};

const MunitSuite fincomm_suite = {
    .prefix     = (char *)(uintptr_t) "/fincomm",
    .tests      = fincomm_tests,
    .suites     = NULL,
    .iterations = 1,
    .options    = MUNIT_SUITE_OPTION_NONE,
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
