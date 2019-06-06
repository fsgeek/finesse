/*
 * Copyright (c) 2017, Tony Mason. All rights reserved.
 */

#include "../finesse.h"

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

#include "../finesse.pb-c.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif // 

static MunitResult
test_one(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    return MUNIT_OK;
}

static MunitResult
test_server_connect(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    finesse_server_handle_t handle = NULL;
    int status;

    status = FinesseStartServerConnection(&handle);
    munit_assert(0 == status);
    munit_assert_not_null(handle);

    status = FinesseStopServerConnection(handle);
    munit_assert(0 == status);

    return MUNIT_OK;
}

static MunitResult
test_client_connect(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    finesse_client_handle_t handle = NULL;
    int status;

    status = FinesseStartClientConnection(&handle);
    munit_assert(0 != status); // no server yet
    munit_assert_null(handle);
    status = FinesseStopClientConnection(handle);
    munit_assert(0 == status);

    return MUNIT_OK;
}

static MunitResult
test_full_connect(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    finesse_server_handle_t server_handle = NULL;
    finesse_client_handle_t client_handle = NULL;
    int status;

    status = FinesseStartServerConnection(&server_handle);
    munit_assert(0 == status);
    munit_assert_not_null(server_handle);

    status = FinesseStartClientConnection(&client_handle);
    munit_assert(0 == status);
    munit_assert_not_null(client_handle);

    status = FinesseStopClientConnection(client_handle);
    munit_assert(0 == status);
    client_handle = NULL;

    status = FinesseStopServerConnection(server_handle);
    munit_assert(0 == status);
    server_handle = NULL;

    return MUNIT_OK;
}


static MunitResult
test_message_test(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    finesse_server_handle_t server_handle = NULL;
    finesse_client_handle_t client_handle = NULL;
    int status;
    uint64_t request_id;
    void *request;
    size_t request_length;
    Finesse__FinesseRequest *finesse_req;

    status = FinesseStartServerConnection(&server_handle);
    munit_assert(0 == status);
    munit_assert_not_null(server_handle);

    status = FinesseStartClientConnection(&client_handle);
    munit_assert(0 == status);
    munit_assert_not_null(client_handle);

    /* send a test (null) message */
    status = FinesseSendTestRequest(client_handle, &request_id);
    munit_assert(0 == status);

    /// get the test (null) reqeust */
    status = FinesseGetRequest(server_handle, &request, &request_length);
    munit_assert(0 == status);

    finesse_req = finesse__finesse_request__unpack(NULL, request_length, request);
    munit_assert_not_null(finesse_req);
    munit_assert(FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__TEST == finesse_req->header->op);
    
    status = FinesseSendTestResponse(server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, 0);
    munit_assert(0 == status);

    status = FinesseGetTestResponse(client_handle, request_id);
    munit_assert(0 == status);

    status = FinesseStopClientConnection(client_handle);
    munit_assert(0 == status);
    client_handle = NULL;

    status = FinesseStopServerConnection(server_handle);
    munit_assert(0 == status);
    server_handle = NULL;

    return MUNIT_OK;
}

static MunitResult
test_message_name_map(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    finesse_server_handle_t server_handle = NULL;
    finesse_client_handle_t client_handle = NULL;
    int status;
    uint64_t request_id;
    void *request;
    size_t request_length;
    Finesse__FinesseRequest *finesse_req;
    uuid_t dummy_uuid;
    uuid_t key_uuid;

    status = FinesseStartServerConnection(&server_handle);
    munit_assert(0 == status);
    munit_assert_not_null(server_handle);

    status = FinesseStartClientConnection(&client_handle);
    munit_assert(0 == status);
    munit_assert_not_null(client_handle);

    // Send a name map request
    status = FinesseSendNameMapRequest(client_handle, (char *)(uintptr_t)"/test", &request_id);
    munit_assert(0 == status);

    /// get the name map reqeust */
    status = FinesseGetRequest(server_handle, &request, &request_length);
    munit_assert(0 == status);

    finesse_req = finesse__finesse_request__unpack(NULL, request_length, request);
    munit_assert_not_null(finesse_req);
    munit_assert(FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP == finesse_req->header->op);

    uuid_generate_time_safe(dummy_uuid);
    
    status = FinesseSendNameMapResponse(server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, &dummy_uuid, 0);
    munit_assert(0 == status);

    status = FinesseGetNameMapResponse(client_handle, request_id, &key_uuid);
    munit_assert(0 == status);
    assert(0 == memcmp(&dummy_uuid, &key_uuid, sizeof(uuid_t)));

    status = FinesseSendNameMapReleaseRequest(client_handle, (uuid_t *)&key_uuid, &request_id);
    munit_assert(0 == status);

    status = FinesseGetRequest(server_handle, &request, &request_length); 
    munit_assert(0 == status);

    finesse_req = finesse__finesse_request__unpack(NULL, request_length, request);
    munit_assert_not_null(finesse_req);
    munit_assert(FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP_RELEASE == finesse_req->header->op);

    status = FinesseSendNameMapReleaseResponse(server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, 0);
    munit_assert(0 == status);

    status = FinesseGetNameMapReleaseResponse(client_handle, request_id);
    munit_assert(0 == status);

    //
    // Let's make sure that the error path works as expected
    //
    // Send a name map request
    status = FinesseSendNameMapRequest(client_handle, (char *)(uintptr_t)"/nothere", &request_id);
    munit_assert(0 == status);

    /// get the name map reqeust */
    status = FinesseGetRequest(server_handle, &request, &request_length);
    munit_assert(0 == status);

    finesse_req = finesse__finesse_request__unpack(NULL, request_length, request);
    munit_assert_not_null(finesse_req);
    munit_assert(FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP == finesse_req->header->op);

    uuid_generate_time_safe(dummy_uuid);
    
    // set an error here
    status = FinesseSendNameMapResponse(server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, &dummy_uuid, ENOENT);
    munit_assert(0 == status);

    status = FinesseGetNameMapResponse(client_handle, request_id, &key_uuid);
    munit_assert(ENOENT == status);


    // Send a name map request
    status = FinesseSendNameMapRequest(client_handle, (char *)(uintptr_t)"/test", &request_id);
    munit_assert(0 == status);

    /// get the name map reqeust */
    status = FinesseGetRequest(server_handle, &request, &request_length);
    munit_assert(0 == status);

    finesse_req = finesse__finesse_request__unpack(NULL, request_length, request);
    munit_assert_not_null(finesse_req);
    munit_assert(FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP == finesse_req->header->op);

    uuid_generate_time_safe(dummy_uuid);
    
    status = FinesseSendNameMapResponse(server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, &dummy_uuid, 0);
    munit_assert(0 == status);

    status = FinesseGetNameMapResponse(client_handle, request_id, &key_uuid);
    munit_assert(0 == status);
    assert(0 == memcmp(&dummy_uuid, &key_uuid, sizeof(uuid_t)));

    // now let's send a name map release request

    status = FinesseSendNameMapReleaseRequest(client_handle, (uuid_t *)&key_uuid, &request_id);
    munit_assert(0 == status);

    status = FinesseGetRequest(server_handle, &request, &request_length); 
    munit_assert(0 == status);

    finesse_req = finesse__finesse_request__unpack(NULL, request_length, request);
    munit_assert_not_null(finesse_req);
    munit_assert(FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP_RELEASE == finesse_req->header->op);

    status = FinesseSendNameMapReleaseResponse(server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, EBADF);
    munit_assert(0 == status);

    status = FinesseGetNameMapReleaseResponse(client_handle, request_id);
    munit_assert(EBADF == status);

    // Done - cleanup
    status = FinesseStopClientConnection(client_handle);
    munit_assert(0 == status);
    client_handle = NULL;

    status = FinesseStopServerConnection(server_handle);
    munit_assert(0 == status);
    server_handle = NULL;

    return MUNIT_OK;
}

static char *ld_library_path[] = {
(char *)(uintptr_t)"/usr/lib/x86_64-linux-gnu/libfakeroot",
(char *)(uintptr_t)"/usr/lib/i686-linux-gnu",
(char *)(uintptr_t)"/lib/i386-linux-gnu",
(char *)(uintptr_t)"/usr/lib/i686-linux-gnu",
(char *)(uintptr_t)"/usr/lib/i386-linux-gnu/mesa",
(char *)(uintptr_t)"/usr/local/lib",
(char *)(uintptr_t)"/lib/x86_64-linux-gnu",
(char *)(uintptr_t)"/usr/lib/x86_64-linux-gnu",
(char *)(uintptr_t)"/usr/lib/x86_64-linux-gnu/mesa-egl",
(char *)(uintptr_t)"/usr/lib/x86_64-linux-gnu/mesa",
(char *)(uintptr_t)"/lib32",
(char *)(uintptr_t)"/usr/lib32",
(char *)(uintptr_t)"/libx32",
(char *)(uintptr_t)"/usr/libx32",
NULL,
};

static char *file_list[] = {
(char *)(uintptr_t)"libc-2.23.so",
(char *)(uintptr_t)"libc.a",
(char *)(uintptr_t)"libc.so",
(char *)(uintptr_t)"libc.so.6",
NULL
};


static MunitResult
test_message_search_path(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    finesse_server_handle_t server_handle = NULL;
    finesse_client_handle_t client_handle = NULL;
    int status;
    uint64_t request_id;
    void *request;
    size_t request_length;
    Finesse__FinesseRequest *finesse_req;
    char *path_found = NULL;

    status = FinesseStartServerConnection(&server_handle);
    munit_assert(0 == status);
    munit_assert_not_null(server_handle);

    status = FinesseStartClientConnection(&client_handle);
    munit_assert(0 == status);
    munit_assert_not_null(client_handle);

    // Send a path search request
    status = FinesseSendPathSearchRequest(client_handle, file_list, ld_library_path, &request_id);
    munit_assert(0 == status);

    // get the path search reqeust
    status = FinesseGetRequest(server_handle, &request, &request_length);
    munit_assert(0 == status);

    finesse_req = finesse__finesse_request__unpack(NULL, request_length, request);
    munit_assert_not_null(finesse_req);
    munit_assert(FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__PATH_SEARCH == finesse_req->header->op);

    // TODO: make sure we get everything (done in the debugger, let's automate it)

    // Let's send a response
    status = FinesseSendPathSearchResponse(server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, (char *)(uintptr_t)"/bin/bash", 0);
    munit_assert(0 == status);

    finesse__finesse_request__free_unpacked(finesse_req, NULL);

    status = FinesseGetPathSearchResponse(client_handle, request_id, &path_found);
    munit_assert(0 == status);

    //
    // now let's make sure the error path works
    //
    status = FinesseSendPathSearchRequest(client_handle, file_list, ld_library_path, &request_id);
    munit_assert(0 == status);

    status = FinesseGetRequest(server_handle, &request, &request_length);
    munit_assert(0 == status);

    finesse_req = finesse__finesse_request__unpack(NULL, request_length, request);
    munit_assert_not_null(finesse_req);
    munit_assert(FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__PATH_SEARCH == finesse_req->header->op);

    status = FinesseSendPathSearchResponse(server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, NULL, ENOENT);
    munit_assert(0 == status);

    finesse__finesse_request__free_unpacked(finesse_req, NULL);

    status = FinesseGetPathSearchResponse(client_handle, request_id, &path_found);
    munit_assert(ENOENT == status);

    status = FinesseStopClientConnection(client_handle);
    munit_assert(0 == status);
    client_handle = NULL;

    status = FinesseStopServerConnection(server_handle);
    munit_assert(0 == status);
    server_handle = NULL;

    return MUNIT_OK;
}


#define TEST(_name, _func, _params)             \
    {                                           \
        .name = (_name),                        \
        .test = (_func),                        \
        .setup = NULL,                          \
        .tear_down = NULL,                      \
        .options = MUNIT_TEST_OPTION_NONE,      \
        .parameters = (_params),                     \
    }

int
main(
    int argc,
    char **argv)
{
    static MunitTest tests[] = {
        TEST((char *)(uintptr_t)"/one", test_one, NULL),
        TEST((char *)(uintptr_t)"/server/connect", test_server_connect, NULL),
        TEST((char *)(uintptr_t)"/client/connect", test_client_connect, NULL),
        TEST((char *)(uintptr_t)"/connect", test_full_connect, NULL),
        TEST((char *)(uintptr_t)"/message/test", test_message_test, NULL), 
        TEST((char *)(uintptr_t)"/message/name_map", test_message_name_map, NULL),
        TEST((char *)(uintptr_t)"/message/search_path", test_message_search_path, NULL),
        TEST(NULL, NULL, NULL),
    };
    static const MunitSuite suite = {
        .prefix = (char *)(uintptr_t)"/finesse",
        .tests = tests,
        .suites = NULL,
        .iterations = 1,
        .options = MUNIT_SUITE_OPTION_NONE,
    };

    return munit_suite_main(&suite, NULL, argc, argv);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
