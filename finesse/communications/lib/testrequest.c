
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
    finesse_fuse_request *ffr = NULL;

    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);
    message = FinesseGetRequestBuffer(fsmr);
    assert(NULL != message);
    message->RequestType = FINESSE_FUSE_REQ_TEST;
    ffr = (finesse_fuse_request *)message->Data;
    ffr->Size = sizeof(finesse_fuse_request);
    ffr->Request.Test.Version = TEST_VERSION;

    // It would be better to use an abstraction layer here, but for now, this
    // is good enough.
    *RequestId = (uint64_t)(uintptr_t)message;

    return status;
}

int FinesseSendTestResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, int64_t Result)
{
    int status = 0;
    server_connection_state_t *scs = FinesseServerHandle;
    fincomm_message message = (fincomm_message)(uintptr_t)RequestId;
    finesse_fuse_response *ffr;
    asert(NULL != scs);
    assert(NULL != ClientUuid);
    assert(0 != RequestId);
    ffr = (finesse_fuse_response *)message->Data;
    message->Response = Result;
    ffr->Type = FINESSE_FUSE_RSP_TEST;

    return status;
}

int FinesseGetTestResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId)
{
    int status = 0;
    client_connection_state_t *ccs = FinesseClientHandle;

    assert(NULL != ccs);
    assert(0 != RequestId);

    return status;
}
