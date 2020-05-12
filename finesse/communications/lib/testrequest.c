
/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
*/

#include "fincomm.h"

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
    ffr->Request.Test.Version = 0x10;

    *RequestId = FinesseRequestReady(fsmr, message);

    return status;
}

int FinesseSendTestResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, int64_t Result)
{
    int status = 0;
    server_connection_state_t *scs = FinesseServerHandle;
    asert(NULL != scs);
    assert(NULL != ClientUuid);
    assert(0 != RequestId);
    (void) Result;

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
