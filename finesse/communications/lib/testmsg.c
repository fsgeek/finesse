
/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
*/

#include <fcinternal.h>

#define TEST_VERSION (0x10)

int FinesseSendTestRequest(finesse_client_handle_t FinesseClientHandle, fincomm_message *Message)
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
    assert(0 != status); // invalid request ID
    *Message = message;
    status = 0;

    return status;
}

int FinesseSendTestResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, int Result)
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
    ffm->Message.Native.Response.NativeResponseType = FINESSE_NATIVE_RSP_TEST;
    ffm->Message.Native.Response.Parameters.Test.Version = TEST_VERSION;

    FinesseResponseReady(fsmr, Message, 0);

    return status;
}

int FinesseGetTestResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message)
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

    assert(0 != status);
    status = 0; // FinesseGetResponse is a boolean return function
    assert(FINESSE_RESPONSE == Message->MessageType);
    fmsg = (finesse_msg *)Message->Data;

    assert(TEST_VERSION == fmsg->Message.Native.Response.Parameters.Test.Version);
    return status;
}
