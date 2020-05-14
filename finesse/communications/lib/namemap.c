/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
*/

#include <fcinternal.h>

// This is the replacement implementation that uses the
// shared memory message exchange API.
int FinesseSendNameMapRequest(finesse_client_handle_t FinesseClientHandle, char *NameToMap, fincomm_message *Message)
{
    int status = 0;
    client_connection_state_t *ccs = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr;
    fincomm_message message = NULL;
    finesse_msg *fmsg = NULL;
    size_t nameLength, bufSize;

    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);
    message = FinesseGetRequestBuffer(fsmr);
    assert(NULL != message);
    message->MessageType = FINESSE_REQUEST;
    fmsg = (finesse_msg *)message->Data;
    fmsg->Version = FINESSE_MESSAGE_VERSION;
    fmsg->MessageClass = FINESSE_NATIVE_MESSAGE;
    fmsg->Message.Native.Request.NativeRequestType = FINESSE_NATIVE_REQ_MAP;
    assert(NULL != NameToMap);

    nameLength = strlen(NameToMap);
    bufSize = SHM_PAGE_SIZE - offsetof(finesse_msg, Message.Native.Request.Parameters.Map.Name);

    assert(nameLength < bufSize);
    strncpy(fmsg->Message.Native.Request.Parameters.Map.Name, NameToMap, bufSize);
    assert(strlen(fmsg->Message.Native.Request.Parameters.Map.Name) == nameLength);
    memset(fmsg->Message.Native.Request.Parameters.Map.Parent, 0, sizeof(fmsg->Message.Native.Request.Parameters.Map.Parent));

    status = FinesseRequestReady(fsmr, message);
    assert(0 != status); // invalid request ID
    *Message = message;
    status = 0;

    return status;
}

int FinesseSendNameMapResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, uuid_t *MapKey, int Result)
{
    int status = 0;
    fincomm_shared_memory_region *fsmr = NULL;
    finesse_msg *ffm;
    unsigned index = (unsigned)(uintptr_t)Client;

    fsmr = FcGetSharedMemoryRegion(FinesseServerHandle, index);
    assert(NULL != fsmr);
    assert (index < SHM_MESSAGE_COUNT);
    assert(0 != Message);
    assert(FINESSE_REQUEST == Message->MessageType);

    Message->Result = Result;
    Message->MessageType = FINESSE_RESPONSE;
    
    ffm = (finesse_msg *)Message->Data;
    memset(ffm, 0, sizeof(finesse_msg)); // not necessary for production
    ffm->Version = FINESSE_MESSAGE_VERSION;
    ffm->MessageClass = FINESSE_NATIVE_MESSAGE;
    ffm->Message.Native.Response.NativeResponseType = FINESSE_NATIVE_RSP_MAP;
    ffm->Message.Native.Response.Parameters.Map.Result = (int)Result;
    assert(Result == ffm->Message.Native.Response.Parameters.Map.Result); // ensure no loss of data
    memcpy(&ffm->Message.Native.Response.Parameters.Map.Key, MapKey, sizeof(uuid_t));

    FinesseResponseReady(fsmr, Message, 0);

    return status;
}

int FinesseGetNameMapResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, uuid_t *MapKey)
{
    int status = 0;
    client_connection_state_t *ccs = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr = NULL;
    finesse_msg *fmsg = NULL;

    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);
    assert(NULL != Message);

    // This is a blocking get
    status = FinesseGetResponse(fsmr, Message, 1);

    assert(0 != status);
    assert(FINESSE_RESPONSE == Message->MessageType);
    assert(0 == Message->Result);
    status = 0; // FinesseGetResponse is a boolean return function
    fmsg = (finesse_msg *)Message->Data;
    
    assert(FINESSE_NATIVE_MESSAGE == fmsg->MessageClass);
    assert(FINESSE_NATIVE_RSP_MAP == fmsg->Message.Native.Response.NativeResponseType);
    assert(0 == fmsg->Message.Native.Response.Parameters.Map.Result);
    memcpy(MapKey, fmsg->Message.Native.Response.Parameters.Map.Key, sizeof(uuid_t));

    return status;

}

int FinesseSendNameMapReleaseRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *MapKey, fincomm_message *Message)
{
    int status = 0;
    client_connection_state_t *ccs = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr;
    fincomm_message message = NULL;
    finesse_msg *fmsg = NULL;

    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);
    message = FinesseGetRequestBuffer(fsmr);
    assert(NULL != message);
    message->MessageType = FINESSE_REQUEST;
    fmsg = (finesse_msg *)message->Data;
    fmsg->Version = FINESSE_MESSAGE_VERSION;
    fmsg->MessageClass = FINESSE_NATIVE_MESSAGE;
    fmsg->Message.Native.Request.NativeRequestType = FINESSE_NATIVE_REQ_MAP_RELEASE;
    assert(NULL != MapKey);
    memcpy(&fmsg->Message.Native.Request.Parameters.MapRelease.Key, MapKey, sizeof(uuid_t));

    status = FinesseRequestReady(fsmr, message);
    assert(0 != status);

    *Message = message;
    status = 0; // request ready returns the request ID

    return status;
}

int FinesseSendNameMapReleaseResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, int Result)
{
    int status = 0;
    fincomm_shared_memory_region *fsmr = NULL;
    finesse_msg *ffm;
    unsigned index = (unsigned)(uintptr_t)Client;

    fsmr = FcGetSharedMemoryRegion(FinesseServerHandle, index);
    assert(NULL != fsmr);
    assert (index < SHM_MESSAGE_COUNT);
    assert(0 != Message);
    assert(FINESSE_REQUEST == Message->MessageType);
    Message->MessageType = FINESSE_RESPONSE;
    Message->Result = Result;

    ffm = (finesse_msg *)Message->Data;
    memset(ffm, 0, sizeof(finesse_msg)); // not necessary for production
    ffm->Version = FINESSE_MESSAGE_VERSION;
    ffm->MessageClass = FINESSE_RESPONSE;
    ffm->MessageClass = FINESSE_NATIVE_MESSAGE;
    ffm->Message.Native.Response.NativeResponseType = FINESSE_NATIVE_RSP_MAP_RELEASE;
    ffm->Message.Native.Response.Parameters.MapRelease.Result = (int)Result;

    FinesseResponseReady(fsmr, Message, 0);

    return status;
}

int FinesseGetNameMapReleaseResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message)
{
    int status = 0;

    client_connection_state_t *ccs = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr = NULL;
    finesse_msg *fmsg = NULL;

    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);
    assert(0 != Message);

    // This is a blocking get
    status = FinesseGetResponse(fsmr, Message, 1);
    assert(0 != status); // boolean return
    status = 0;

    assert(FINESSE_RESPONSE == Message->MessageType);
    assert(0 == Message->Result);
    fmsg = (finesse_msg *)Message->Data;

    assert(FINESSE_NATIVE_MESSAGE == fmsg->MessageClass);
    assert(FINESSE_NATIVE_RSP_MAP_RELEASE == fmsg->Message.Native.Response.NativeResponseType);
    assert(0 == fmsg->Message.Native.Response.Parameters.MapRelease.Result);

    return status;
}



#if 0
int FinesseSendTestRequest(finesse_client_handle_t FinesseClientHandle, fincomm_message Message)
{
    int status = 0;
    client_connection_state_t *ccs = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr;
    fincomm_message message = NULL;
    finesse_fuse_request *ffr = NULL;

    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);
    message = FinesseGetRequestBuffer(fsmr);
    assert(NULL != message);
    message->MessageType = FINESSE_FUSE_REQ_TEST;
    ffr = (finesse_fuse_request *)message->Data;
    ffr->Request.Test.Version = TEST_VERSION;

    status = FinesseRequestReady(fsmr, message);

    if (0 == status) {
        // It would be better to use an abstraction layer here, but for now, this
        // is good enough.
        *RequestId = (uint64_t)(uintptr_t)message;
    }
    else {
        FinesseReleaseRequestBuffer(fsmr, message);
    }

    return status;
}

int FinesseSendTestResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, int64_t Result)
{
    int status = 0;
    server_connection_state_t *scs = FinesseServerHandle;
    fincomm_message_block *message = (fincomm_message_block *)(uintptr_t)RequestId;
    finesse_msg *ffm;

    assert(NULL != scs);
    assert(NULL != ClientUuid);
    assert(0 != RequestId);
    assert(FINESSE_FUSE_MSG_REQUEST == message->MessageType);
    
    message->Result = Result;
    message->MessageType = FINESSE_FUSE_MSG_RESPONSE;

    ffm = (finesse_msg *)message->Data;
    memset(ffm, 0, sizeof(finesse_msg)); // not necessary for production
    ffm->Version = FINESSE_MESSAGE_VERSION;
    ffm->MessageType = FINESSE_FUSE_RSP_NONE;

    FinesseResponseReady(scs->client_shm, message, 0);

    return status;
}

int FinesseGetTestResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message)
{
    int status = 0;
    client_connection_state_t *ccs = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr = NULL;
    fincomm_message_block *message = NULL;

    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);
    assert(0 != RequestId);
    message = (fincomm_message_block *)RequestId;
    assert(NULL != message);

    // This is a blocking get
    status = FinesseGetResponse(fsmr, message, 1);

    return status;
}
#endif // 0
