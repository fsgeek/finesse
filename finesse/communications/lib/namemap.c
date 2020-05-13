/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
*/

#include <finesse.h>

// This is the replacement implementation that uses the
// shared memory message exchange API.
int FinesseSendNameMapRequest(finesse_client_handle_t FinesseClientHandle, char *NameToMap, uint64_t *RequestId)
{
    int status = 0;
    client_connection_state_t *ccs = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr;
    fincomm_message message = NULL;
    finesse_msg *fmsg = NULL;
    size_t nameLength;

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
    assert(nameLength < nameLength);
    strncpy(fmsg->Message.Native.Request.Parameters.Map.Name, NameToMap, sizeof(fmsg->Message.Native.Request.Parameters.Map.Name));
    memset(fmsg->Message.Native.Request.Parameters.Map.Parent, 0, sizeof(fmsg->Message.Native.Request.Parameters.Map.Parent));
    status = FinesseRequestReady(fsmr, message);
    assert(0 == status);

    *RequestId = (uint64_t)(uintptr_t)message;

    return status;
}

int FinesseSendNameMapResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, uuid_t *MapKey, int64_t Result)
{
    int status = 0;
    server_connection_state_t *scs = FinesseServerHandle;
    fincomm_message_block *message = (fincomm_message_block *)(uintptr_t)RequestId;
    finesse_msg *ffm;

    assert(NULL != scs);
    assert(NULL != ClientUuid);
    assert(0 != RequestId);
    assert(FINESSE_REQUEST == message->MessageType);
    
    message->Result = Result;
    message->MessageType = FINESSE_RESPONSE;

    ffm = (finesse_msg *)message->Data;
    memset(ffm, 0, sizeof(finesse_msg)); // not necessary for production
    ffm->Version = FINESSE_MESSAGE_VERSION;
    ffm->MessageClass = FINESSE_NATIVE_MESSAGE;
    ffm->Message.Native.Response.NativeResponseType = FINESSE_NATIVE_RSP_MAP;
    ffm->Message.Native.Response.Parameters.Map.Result = (int)Result;
    assert(Result == ffm->Message.Native.Response.Parameters.Map.Result); // ensure no loss of data
    memcpy(&ffm->Message.Native.Response.Parameters.Map.Key, MapKey, sizeof(uuid_t));

    FinesseResponseReady((fincomm_shared_memory_region *)scs->client_shm, message, 0);

    return status;
}

int FinesseGetNameMapResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, uuid_t *MapKey)
{
    int status = 0;
    client_connection_state_t *ccs = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr = NULL;
    fincomm_message_block *message = NULL;
    finesse_msg *fmsg = NULL;

    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);
    assert(0 != RequestId);
    message = (fincomm_message_block *)RequestId;
    assert(NULL != message);

    // This is a blocking get
    status = FinesseGetResponse(fsmr, message, 1);

    assert(0 == status);
    assert(FINESSE_RESPONSE == message->MessageType);
    assert(0 == message->Result);
    fmsg = (finesse_msg *)message->Data;

    assert(FINESSE_NATIVE_MESSAGE == fmsg->MessageClass);
    assert(FINESSE_NATIVE_RSP_MAP == fmsg->Message.Native.Response.NativeResponseType);
    assert(0 == fmsg->Message.Native.Response.Parameters.Map.Result);
    memcpy(MapKey, fmsg->Message.Native.Response.Parameters.Map.Key, sizeof(uuid_t));

    return status;

}

int FinesseSendNameMapReleaseRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *MapKey, uint64_t *RequestId)
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
    assert(0 == status);

    *RequestId = (uint64_t)(uintptr_t)message;

    return status;
}

int FinesseSendNameMapReleaseResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, int64_t Result)
{
    int status = 0;
    server_connection_state_t *scs = FinesseServerHandle;
    fincomm_message_block *message = (fincomm_message_block *)(uintptr_t)RequestId;
    finesse_msg *ffm;

    assert(NULL != scs);
    assert(NULL != ClientUuid);
    assert(0 != RequestId);
    assert(FINESSE_REQUEST == message->MessageType);
    
    message->Result = Result;
    message->MessageType = FINESSE_RESPONSE;

    ffm = (finesse_msg *)message->Data;
    memset(ffm, 0, sizeof(finesse_msg)); // not necessary for production
    ffm->Version = FINESSE_MESSAGE_VERSION;
    ffm->MessageClass = FINESSE_NATIVE_MESSAGE;
    ffm->Message.Native.Response.NativeResponseType = FINESSE_NATIVE_RSP_MAP_RELEASE;
    ffm->Message.Native.Response.Parameters.Map.Result = (int)Result;

    FinesseResponseReady((fincomm_shared_memory_region *)scs->client_shm, message, 0);

    return status;
}

int FinesseGetNameMapReleaseResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId)
{
    int status = 0;

    client_connection_state_t *ccs = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr = NULL;
    fincomm_message_block *message = NULL;
    finesse_msg *fmsg = NULL;

    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);
    assert(0 != RequestId);
    message = (fincomm_message_block *)RequestId;
    assert(NULL != message);

    // This is a blocking get
    status = FinesseGetResponse(fsmr, message, 1);
    assert(0 == status);

    assert(FINESSE_RESPONSE == message->MessageType);
    assert(0 == message->Result);
    fmsg = (finesse_msg *)message->Data;

    assert(FINESSE_NATIVE_MESSAGE == fmsg->MessageClass);
    assert(FINESSE_NATIVE_RSP_MAP_RELEASE == fmsg->Message.Native.Response.NativeResponseType);
    assert(0 == fmsg->Message.Native.Response.Parameters.MapRelease.Result);

    return status;
}



#if 0
int FinesseSendTestRequest(finesse_client_handle_t FinesseClientHandle, uint64_t *RequestId)
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

int FinesseSendTestResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, int64_t Result)
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

int FinesseGetTestResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId)
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
