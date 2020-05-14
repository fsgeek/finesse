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
#include "fincomm.h"


#if !defined(__notused)
#define __notused __attribute__((unused))
#endif //

#define TEST_VERSION (0x10)

static MunitResult
test_server_connect(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int status;
    finesse_server_handle_t fsh;

    status = FinesseStartServerConnection(&fsh);
    munit_assert(0 == status);

    status = FinesseStopServerConnection(fsh);
    munit_assert(0 == status);

    return MUNIT_OK;
}

static MunitResult
test_client_connect(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int status;
    finesse_server_handle_t fsh;
    finesse_client_handle_t fch;

    status = FinesseStartServerConnection(&fsh);
    munit_assert(0 == status);

    status = FinesseStartClientConnection(&fch);
    munit_assert(0 == status);

    // There is a race condition between start and stop.
    // So for now, I'll just add a sleep
    // TODO: fix the race.
    sleep(1); 

    status = FinesseStopClientConnection(fch);
    munit_assert(0 == status);
    

    status = FinesseStopServerConnection(fsh);
    munit_assert(0 == status);

    return MUNIT_OK;
}

static MunitResult
test_client_connect_without_server(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int status;
    finesse_client_handle_t fch;

    status = FinesseStartClientConnection(&fch);
    munit_assert(0 != status);

    return MUNIT_OK;
}

static MunitResult
test_msg_test(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int status;
    finesse_server_handle_t fsh;
    finesse_client_handle_t fch;
    fincomm_message message;
    finesse_msg *test_message = NULL;
    fincomm_message fm_server = NULL;
    void *client;
    fincomm_message request;

    status = FinesseStartServerConnection(&fsh);
    munit_assert(0 == status);
    munit_assert(NULL != fsh);

    status = FinesseStartClientConnection(&fch);
    munit_assert(0 == status);
    munit_assert(NULL != fch);

    // client sends request
    status = FinesseSendTestRequest(fch, &message);
    munit_assert(0 == status);

    // server gets a request
    status = FinesseGetRequest(fsh, &client, &request);
    assert(0 == status);
    assert(NULL != client);
    assert(NULL != request);
    fm_server = (fincomm_message)request;
    munit_assert(FINESSE_REQUEST == fm_server->MessageType);
    test_message = (finesse_msg *)fm_server->Data;
    munit_assert(FINESSE_MESSAGE_VERSION == test_message->Version);
    munit_assert(FINESSE_NATIVE_MESSAGE == test_message->MessageClass);
    munit_assert(FINESSE_NATIVE_REQ_TEST == test_message->Message.Native.Request.NativeRequestType);
    munit_assert(TEST_VERSION == test_message->Message.Native.Request.Parameters.Test.Version);

    // server responds
    status = FinesseSendTestResponse(fsh, client, fm_server, 0);
    munit_assert(0 == status);

    // client gets the response
    status = FinesseGetTestResponse(fch, message);
    munit_assert(0 == status);

    // cleanup    
    status = FinesseStopClientConnection(fch);
    munit_assert(0 == status);
    
    status = FinesseStopServerConnection(fsh);
    munit_assert(0 == status);

    return MUNIT_OK;
}


static const MunitTest finesse_tests[] = {
        TEST((char *)(uintptr_t)"/null", test_null, NULL),
        TEST((char* )(uintptr_t)"/server/connect", test_server_connect, NULL),
        TEST((char *)(uintptr_t)"/client/connect_without_server", test_client_connect_without_server, NULL),
        TEST((char *)(uintptr_t)"/client/connect", test_client_connect, NULL),
        TEST((char *)(uintptr_t)"/client/test_msg", test_msg_test, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite finesse_suite = {
    .prefix = (char *)(uintptr_t)"/api",
    .tests = (MunitTest *)(uintptr_t)finesse_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};

