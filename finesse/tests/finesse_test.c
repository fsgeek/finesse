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

    // too fast and startup hasn't finished
    sleep(1);

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

    // Free the buffer
    FinesseFreeTestResponse(fch, message);

    // cleanup    
    status = FinesseStopClientConnection(fch);
    munit_assert(0 == status);
    
    status = FinesseStopServerConnection(fsh);
    munit_assert(0 == status);

    return MUNIT_OK;
}

static MunitResult
test_msg_namemap (
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int status;
    finesse_server_handle_t fsh;
    finesse_client_handle_t fch;
    fincomm_message message1, message2;
    finesse_msg *test_message1 = NULL;
    finesse_msg *test_message2 = NULL;
    fincomm_message fm_server1 = NULL;
    fincomm_message fm_server2 = NULL;
    void *client1;
    void *client2;
    fincomm_message request1, request2;
    char *name1 = (char *)(uintptr_t)"/foo";
    char *name2 = (char *)(uintptr_t)"/bar";
    uuid_t key1, key2;
    uuid_t out_key1, out_key2;
    
    status = FinesseStartServerConnection(&fsh);
    munit_assert(0 == status);
    munit_assert(NULL != fsh);

    status = FinesseStartClientConnection(&fch);
    munit_assert(0 == status);
    munit_assert(NULL != fch);

    // client sends requests
    status = FinesseSendNameMapRequest(fch, name1, &message1);
    munit_assert(0 == status);
    status = FinesseSendNameMapRequest(fch, name2, &message2);
    munit_assert(0 == status);

    // server gets requests
    status = FinesseGetRequest(fsh, &client1, &request1);
    assert(0 == status);
    assert(NULL != request1);
    fm_server1 = (fincomm_message)request1;
    munit_assert(FINESSE_REQUEST == fm_server1->MessageType);
    test_message1 = (finesse_msg *)fm_server1->Data;
    munit_assert(FINESSE_MESSAGE_VERSION == test_message1->Version);
    munit_assert(FINESSE_NATIVE_MESSAGE == test_message1->MessageClass);
    munit_assert(FINESSE_NATIVE_REQ_MAP == test_message1->Message.Native.Request.NativeRequestType);
    munit_assert((0 == strcmp(name1, test_message1->Message.Native.Request.Parameters.Map.Name)) ||
                 (0 == strcmp(name2, test_message1->Message.Native.Request.Parameters.Map.Name)));

    status = FinesseGetRequest(fsh, &client2, &request2);
    assert(0 == status);
    assert(NULL != request2);
    fm_server2 = (fincomm_message)request2;
    munit_assert(FINESSE_REQUEST == fm_server2->MessageType);
    test_message2 = (finesse_msg *)fm_server2->Data;
    munit_assert(FINESSE_MESSAGE_VERSION == test_message2->Version);
    munit_assert(FINESSE_NATIVE_MESSAGE == test_message2->MessageClass);
    munit_assert(FINESSE_NATIVE_REQ_MAP == test_message2->Message.Native.Request.NativeRequestType);
    munit_assert((0 == strcmp(name1, test_message2->Message.Native.Request.Parameters.Map.Name)) ||
                 (0 == strcmp(name2, test_message2->Message.Native.Request.Parameters.Map.Name)));

    // server responds
    uuid_generate(key2);
    status = FinesseSendNameMapResponse(fsh, client2, fm_server2, &key2, 0);
    munit_assert(0 == status);
    uuid_generate(key1);
    status = FinesseSendNameMapResponse(fsh, client1, fm_server1, &key1, 0);
    munit_assert(0 == status);

    // client gets the response (for message 2)
    status = FinesseGetNameMapResponse(fch, message2, &out_key2);
    munit_assert(0 == status);
    munit_assert(0 == uuid_compare(key2, out_key2));  
    FinesseFreeNameMapResponse(fch, message2);

    // client gets the response (for message 1)
    status = FinesseGetNameMapResponse(fch, message1, &out_key1);
    munit_assert(0 == status);
    munit_assert(0 == uuid_compare(key1, out_key1));
    FinesseFreeNameMapResponse(fch, message1);

    // cleanup    
    status = FinesseStopClientConnection(fch);
    munit_assert(0 == status);
    
    status = FinesseStopServerConnection(fsh);
    munit_assert(0 == status);

    return MUNIT_OK;
}

static MunitResult
test_msg_namemaprelease (
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
    uuid_t key;

    status = FinesseStartServerConnection(&fsh);
    munit_assert(0 == status);
    munit_assert(NULL != fsh);

    status = FinesseStartClientConnection(&fch);
    munit_assert(0 == status);
    munit_assert(NULL != fch);

    // client sends request
    uuid_generate(key);
    status = FinesseSendNameMapReleaseRequest(fch, &key, &message);
    munit_assert(0 == status);

    // server gets a request
    status = FinesseGetRequest(fsh, &client, &request);
    assert(0 == status);
    assert(NULL != request);
    fm_server = (fincomm_message)request;
    munit_assert(FINESSE_REQUEST == fm_server->MessageType);
    test_message = (finesse_msg *)fm_server->Data;
    munit_assert(FINESSE_MESSAGE_VERSION == test_message->Version);
    munit_assert(FINESSE_NATIVE_MESSAGE == test_message->MessageClass);
    munit_assert(FINESSE_NATIVE_REQ_MAP_RELEASE == test_message->Message.Native.Request.NativeRequestType);
    munit_assert(0 == uuid_compare(key, test_message->Message.Native.Request.Parameters.MapRelease.Key));

    // server responds
    status = FinesseSendNameMapReleaseResponse(fsh, client, fm_server, 0);
    munit_assert(0 == status);

    // client gets the response
    status = FinesseGetNameMapReleaseResponse(fch, message);
    munit_assert(0 == status);

    // Release the message
    FinesseFreeNameMapReleaseResponse(fch, message);

    // cleanup    
    status = FinesseStopClientConnection(fch);
    munit_assert(0 == status);
    
    status = FinesseStopServerConnection(fsh);
    munit_assert(0 == status);

    return MUNIT_OK;
}

static MunitResult
test_msg_statfs (
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
    uuid_t key;
    struct statvfs vfs, vfs2;

    status = FinesseStartServerConnection(&fsh);
    munit_assert(0 == status);
    munit_assert(NULL != fsh);

    status = FinesseStartClientConnection(&fch);
    munit_assert(0 == status);
    munit_assert(NULL != fch);

    // client sends request
    uuid_generate(key);
    status = FinesseSendStatfsRequest(fch, ".", &message);
    munit_assert(0 == status);

    // server gets a request
    status = FinesseGetRequest(fsh, &client, &request);
    assert(0 == status);
    assert(NULL != request);
    fm_server = (fincomm_message)request;
    munit_assert(FINESSE_REQUEST == fm_server->MessageType);
    test_message = (finesse_msg *)fm_server->Data;

    munit_assert(FINESSE_MESSAGE_VERSION == test_message->Version);
    munit_assert(FINESSE_FUSE_MESSAGE == test_message->MessageClass);
    munit_assert(FINESSE_FUSE_REQ_STATFS == test_message->Message.Fuse.Request.Type);
    munit_assert(STATFS == test_message->Message.Fuse.Request.Parameters.Statfs.StatFsType);

    status = statvfs(".", &vfs);
    munit_assert(0 == status);

    // server responds
    status = FinesseSendStatfsResponse(fsh, client, fm_server, &vfs, 0);
    munit_assert(0 == status);

    // client gets the response
    status = FinesseGetStatfsResponse(fch, message, &vfs2);
    munit_assert(0 == status);
    munit_assert(0 == memcmp(&vfs, &vfs2, sizeof(vfs)));

    // Release the message
    FinesseFreeStatfsResponse(fch, message);

    // Now check the inode based approach, albeit with a fake inode
    uuid_generate(key);
    status = FinesseSendFstatfsRequest(fch, &key, &message);
    munit_assert(0 == status);

    // server gets a request
    status = FinesseGetRequest(fsh, &client, &request);
    assert(0 == status);
    assert(NULL != request);
    fm_server = (fincomm_message)request;
    munit_assert(FINESSE_REQUEST == fm_server->MessageType);
    test_message = (finesse_msg *)fm_server->Data;

    munit_assert(FINESSE_MESSAGE_VERSION == test_message->Version);
    munit_assert(FINESSE_FUSE_MESSAGE == test_message->MessageClass);
    munit_assert(FINESSE_FUSE_REQ_STATFS == test_message->Message.Fuse.Request.Type);
    munit_assert(FSTATFS == test_message->Message.Fuse.Request.Parameters.Statfs.StatFsType);
    munit_assert(0 == uuid_compare(key, test_message->Message.Fuse.Request.Parameters.Statfs.Options.Inode));

    status = statvfs(".", &vfs);
    munit_assert(0 == status);

    // server responds
    FinesseSendFstatfsResponse(fsh, client, fm_server, &vfs, 0);
    munit_assert(0 == status);

    // client gets the response
    FinesseGetFstatfsResponse(fch, message, &vfs2);
    munit_assert(0 == status);
    munit_assert(0 == memcmp(&vfs, &vfs2, sizeof(vfs)));

    // Release the message
    FinesseFreeFstatfsResponse(fch, message);

    // cleanup    
    status = FinesseStopClientConnection(fch);
    munit_assert(0 == status);
    
    status = FinesseStopServerConnection(fsh);
    munit_assert(0 == status);

    return MUNIT_OK;
}


static MunitResult
test_msg_unlink (
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
    uuid_t key;

    status = FinesseStartServerConnection(&fsh);
    munit_assert(0 == status);
    munit_assert(NULL != fsh);

    status = FinesseStartClientConnection(&fch);
    munit_assert(0 == status);
    munit_assert(NULL != fch);

    // client sends request
    memset(&key, 0, sizeof(key));
    status = FinesseSendUnlinkRequest(fch, &key, "foo", &message);
    munit_assert(0 == status);

    // server gets a request
    status = FinesseGetRequest(fsh, &client, &request);
    assert(0 == status);
    assert(NULL != request);
    fm_server = (fincomm_message)request;
    munit_assert(FINESSE_REQUEST == fm_server->MessageType);
    test_message = (finesse_msg *)fm_server->Data;

    munit_assert(FINESSE_MESSAGE_VERSION == test_message->Version);
    munit_assert(FINESSE_FUSE_MESSAGE == test_message->MessageClass);
    munit_assert(FINESSE_FUSE_REQ_UNLINK == test_message->Message.Fuse.Request.Type);
    munit_assert(uuid_is_null(test_message->Message.Fuse.Request.Parameters.Unlink.Parent));
    munit_assert(0 == strcmp("foo", test_message->Message.Fuse.Request.Parameters.Unlink.Name));
    munit_assert(0 == status);

    // server responds
    status = FinesseSendUnlinkResponse(fsh, client, fm_server, ENOENT);
    munit_assert(0 == status);

    // client gets the response
    status = FinesseGetUnlinkResponse(fch, message);
    munit_assert(0 == status);

    // Release the message
    FinesseFreeUnlinkMapResponse(fch, message);

    // cleanup    
    status = FinesseStopClientConnection(fch);
    munit_assert(0 == status);
    
    status = FinesseStopServerConnection(fsh);
    munit_assert(0 == status);

    return MUNIT_OK;
}

static MunitResult
test_msg_stat (
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
    uuid_t key;
    uuid_t parent;
    struct stat statbuf1, statbuf2;

    status = FinesseStartServerConnection(&fsh);
    munit_assert(0 == status);
    munit_assert(NULL != fsh);

    status = FinesseStartClientConnection(&fch);
    munit_assert(0 == status);
    munit_assert(NULL != fch);

    // client sends request
    memset(&parent, 0, sizeof(uuid_t));
    status = FinesseSendStatRequest(fch, &parent, "/foo", &message);
    munit_assert(0 == status);

    // server gets a request
    status = FinesseGetRequest(fsh, &client, &request);
    assert(0 == status);
    assert(NULL != request);
    fm_server = (fincomm_message)request;
    munit_assert(FINESSE_REQUEST == fm_server->MessageType);
    test_message = (finesse_msg *)fm_server->Data;

    munit_assert(FINESSE_MESSAGE_VERSION == test_message->Version);
    munit_assert(FINESSE_FUSE_MESSAGE == test_message->MessageClass);
    munit_assert(FINESSE_FUSE_REQ_GETATTR == test_message->Message.Fuse.Request.Type);
    munit_assert(GETATTR_STAT == test_message->Message.Fuse.Request.Parameters.GetAttr.StatType);
    munit_assert(uuid_is_null(test_message->Message.Fuse.Request.Parameters.GetAttr.Options.Path.Parent));
    munit_assert(0 == strcmp("/foo", test_message->Message.Fuse.Request.Parameters.GetAttr.Options.Path.Name));

    // server responds
    status = stat(".", &statbuf1);
    munit_assert(0 == status);
    status = FinesseSendStatResponse(fsh, client, fm_server, &statbuf1, 1.0, 0);
    munit_assert(0 == status);

    // client gets the response
    status = FinesseGetStatResponse(fch, message, &statbuf2);
    munit_assert(0 == status);
    munit_assert(0 == memcmp(&statbuf1, &statbuf2, sizeof(statbuf1)));

    // Release the message
    FinesseFreeStatfsResponse(fch, message);

    // Now check the inode based approach, albeit with a fake inode
    uuid_generate(key);
    status = FinesseSendFstatRequest(fch, &key, &message);
    munit_assert(0 == status);

    // server gets a request
    status = FinesseGetRequest(fsh, &client, &request);
    assert(0 == status);
    assert(NULL != request);
    fm_server = (fincomm_message)request;
    munit_assert(FINESSE_REQUEST == fm_server->MessageType);
    test_message = (finesse_msg *)fm_server->Data;

    munit_assert(FINESSE_MESSAGE_VERSION == test_message->Version);
    munit_assert(FINESSE_FUSE_MESSAGE == test_message->MessageClass);
    munit_assert(FINESSE_FUSE_REQ_GETATTR == test_message->Message.Fuse.Request.Type);
    munit_assert(GETATTR_FSTAT == test_message->Message.Fuse.Request.Parameters.GetAttr.StatType);
    munit_assert(0 == uuid_compare(key, test_message->Message.Fuse.Request.Parameters.GetAttr.Options.Inode));
    munit_assert(0 == status);

    // server responds
    FinesseSendFstatResponse(fsh, client, fm_server, &statbuf1, 1.0, 0);
    munit_assert(0 == status);

    // client gets the response
    memset(&statbuf2, 0, sizeof(statbuf2));
    FinesseGetFstatResponse(fch, message, &statbuf2);
    munit_assert(0 == status);
    munit_assert(0 == memcmp(&statbuf1, &statbuf2, sizeof(statbuf1)));

    // Release the message
    FinesseFreeFstatfsResponse(fch, message);

    // Now... to test LSTAT

    // client sends request
    memset(&parent, 0, sizeof(uuid_t));
    status = FinesseSendLstatRequest(fch, &parent, "/foo", &message);
    munit_assert(0 == status);

    // server gets a request
    status = FinesseGetRequest(fsh, &client, &request);
    assert(0 == status);
    assert(NULL != request);
    fm_server = (fincomm_message)request;
    munit_assert(FINESSE_REQUEST == fm_server->MessageType);
    test_message = (finesse_msg *)fm_server->Data;

    munit_assert(FINESSE_MESSAGE_VERSION == test_message->Version);
    munit_assert(FINESSE_FUSE_MESSAGE == test_message->MessageClass);
    munit_assert(FINESSE_FUSE_REQ_GETATTR == test_message->Message.Fuse.Request.Type);
    munit_assert(GETATTR_LSTAT == test_message->Message.Fuse.Request.Parameters.GetAttr.StatType);
    munit_assert(uuid_is_null(test_message->Message.Fuse.Request.Parameters.GetAttr.Options.Path.Parent));
    munit_assert(0 == strcmp("/foo", test_message->Message.Fuse.Request.Parameters.GetAttr.Options.Path.Name));

    // server responds
    status = stat(".", &statbuf1);
    munit_assert(0 == status);
    status = FinesseSendStatResponse(fsh, client, fm_server, &statbuf1, 1.0, 0);
    munit_assert(0 == status);

    // client gets the response
    memset(&statbuf2, 0, sizeof(statbuf2));
    status = FinesseGetStatResponse(fch, message, &statbuf2);
    munit_assert(0 == status);
    munit_assert(0 == memcmp(&statbuf1, &statbuf2, sizeof(statbuf1)));

    // Release the message
    FinesseFreeStatfsResponse(fch, message);

    // cleanup    
    status = FinesseStopClientConnection(fch);
    munit_assert(0 == status);
    
    status = FinesseStopServerConnection(fsh);
    munit_assert(0 == status);

    return MUNIT_OK;
}

static MunitResult
test_msg_create (
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int status = 0;
    finesse_server_handle_t fsh;
    finesse_client_handle_t fch;
    fincomm_message message;
    finesse_msg *test_message = NULL;
    fincomm_message fm_server = NULL;
    void *client;
    fincomm_message request;
    uuid_t key;
    uuid_t parent;
    struct stat statbuf;
    char *fname = (char *)(uintptr_t)"/foo";
    uuid_t outkey;
    struct stat statbuf_out;
    double timeout;
    uint64_t generation;

    status = FinesseStartServerConnection(&fsh);
    munit_assert(0 == status);
    munit_assert(NULL != fsh);

    status = FinesseStartClientConnection(&fch);
    munit_assert(0 == status);
    munit_assert(NULL != fch);

    // client sends request
    uuid_generate(key);
    memset(&parent, 0, sizeof(parent));
    status = FinesseSendCreateRequest(fch, &parent, fname, &statbuf, &message);
    munit_assert(0 == status);

    // server gets a request
    status = FinesseGetRequest(fsh, &client, &request);
    assert(0 == status);
    assert(NULL != request);
    fm_server = (fincomm_message)request;
    munit_assert(FINESSE_REQUEST == fm_server->MessageType);
    test_message = (finesse_msg *)fm_server->Data;
    munit_assert(FINESSE_MESSAGE_VERSION == test_message->Version);
    munit_assert(FINESSE_FUSE_MESSAGE == test_message->MessageClass);
    munit_assert(FINESSE_FUSE_REQ_CREATE == test_message->Message.Fuse.Request.Type);
    munit_assert(uuid_is_null(test_message->Message.Fuse.Request.Parameters.Create.Parent));
    munit_assert(0 == strcmp(fname, test_message->Message.Fuse.Request.Parameters.Create.Name));   
    munit_assert(0 == memcmp(&test_message->Message.Fuse.Request.Parameters.Create.Attr, &statbuf, sizeof(statbuf)));
    munit_assert(0 == status);

    // server responds
    status = FinesseSendCreateResponse(fsh, client, fm_server, &key, 0, &statbuf, 1.0, 0);
    munit_assert(0 == status);

    // client gets the response
    memset(&outkey, 0, sizeof(outkey));
    memset(&statbuf_out, 0, sizeof(statbuf_out));
    timeout = 0.0;

    status = FinesseGetCreateResponse(fch, message, &outkey, &generation, &statbuf_out, &timeout);
    munit_assert(0 == status);
    munit_assert(0 == memcmp(&statbuf, &statbuf_out, sizeof(statbuf)));

    // Release the message
    FinesseFreeCreateResponse(fch, message);

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
        TEST((char *)(uintptr_t)"/client/msg", test_msg_test, NULL),
        TEST((char *)(uintptr_t)"/client/map", test_msg_namemap, NULL),
        TEST((char *)(uintptr_t)"/client/map_release", test_msg_namemaprelease, NULL),
        TEST((char *)(uintptr_t)"/client/statfs", test_msg_statfs, NULL),
        TEST((char *)(uintptr_t)"/client/unlink", test_msg_unlink, NULL),
        TEST((char *)(uintptr_t)"/client/stat", test_msg_stat, NULL),
        TEST((char *)(uintptr_t)"/client/create", test_msg_create, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite finesse_suite = {
    .prefix = (char *)(uintptr_t)"/api",
    .tests = (MunitTest *)(uintptr_t)finesse_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};

