
/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
*/

#include "fincomm.h"

#define TEST_VERSION (0x10)

int FinesseSendTestRequest(finesse_client_handle_t FinesseClientHandle, uint64_t *RequestId)
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
    fmsg->Message.Native.Request.NativeRequestType = FINESSE_NATIVE_REQ_TEST;
    fmsg->Message.Native.Request.Parameters.Test.Version = TEST_VERSION;

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
    assert(FINESSE_REQUEST == message->MessageType);
    
    message->Result = Result;
    message->MessageType = FINESSE_RESPONSE;

    ffm = (finesse_msg *)message->Data;
    memset(ffm, 0, sizeof(finesse_msg)); // not necessary for production
    ffm->Version = FINESSE_MESSAGE_VERSION;
    ffm->MessageClass = FINESSE_NATIVE_MESSAGE;
    ffm->Message.Native.Response.NativeResponseType = FINESSE_NATIVE_RSP_TEST;
    ffm->Message.Native.Response.Parameters.Test.Version = TEST_VERSION;

    FinesseResponseReady((fincomm_shared_memory_region *)scs->client_shm, message, 0);

    return status;
}

int FinesseGetTestResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId)
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
    fmsg = (finesse_msg *)message->Data;

    assert(TEST_VERSION == fmsg->Message.Native.Response.Parameters.Test.Version);

    return status;
}
