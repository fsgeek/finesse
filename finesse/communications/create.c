
/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
*/

#include <fcinternal.h>

int FinesseSendCreateRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Parent, const char *Path, struct stat *Stat, fincomm_message *Message)
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
    fmsg->MessageClass = FINESSE_FUSE_MESSAGE;
    fmsg->Message.Fuse.Request.Type = FINESSE_FUSE_REQ_CREATE;
    memcpy(&fmsg->Message.Fuse.Request.Parameters.Create.Parent, Parent, sizeof(uuid_t));
    fmsg->Message.Fuse.Request.Parameters.Create.Attr = *Stat;
    memcpy(&fmsg->Message.Fuse.Request.Parameters.Create.Parent, Parent, sizeof(uuid_t));

    assert(NULL != Path);
    nameLength = strlen(Path);
    bufSize = SHM_PAGE_SIZE - offsetof(finesse_msg, Message.Fuse.Request.Parameters.Create.Name);
    assert(nameLength < bufSize);
    memcpy(fmsg->Message.Fuse.Request.Parameters.Create.Name, Path, nameLength+1);

    status = FinesseRequestReady(fsmr, message);
    assert(0 != status); // invalid request ID
    *Message = message;
    status = 0;

    return status;
}


int FinesseSendCreateResponse(
    finesse_server_handle_t FinesseServerHandle, 
    void *Client, 
    fincomm_message Message,
    uuid_t *Key,
    uint64_t Generation,
    struct stat *Stat, 
    double Timeout, 
    int Result)
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

    assert(NULL != Stat);
    ffm = (finesse_msg *)Message->Data;
    memset(ffm, 0, sizeof(finesse_msg)); // not necessary for production
    ffm->Version = FINESSE_MESSAGE_VERSION;
    ffm->MessageClass = FINESSE_FUSE_MESSAGE;
    ffm->Message.Fuse.Response.Type = FINESSE_FUSE_RSP_CREATE;
    memcpy(&ffm->Message.Fuse.Response.Parameters.Create.Key, Key, sizeof(uuid_t));
    ffm->Message.Fuse.Response.Parameters.Create.Generation = Generation;
    ffm->Message.Fuse.Response.Parameters.Create.Attr = *Stat;
    ffm->Message.Fuse.Response.Parameters.Create.Timeout = Timeout;

    FinesseResponseReady(fsmr, Message, 0);

    return status;
}

int FinesseGetCreateResponse(
    finesse_client_handle_t FinesseClientHandle, 
    fincomm_message Message, 
    uuid_t *Key,
    uint64_t *Generation,
    struct stat *Stat, 
    double *Timeout
)
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
    assert(FINESSE_MESSAGE_VERSION == fmsg->Version);
    assert(FINESSE_FUSE_MESSAGE == fmsg->MessageClass);
    assert(FINESSE_FUSE_RSP_CREATE == fmsg->Message.Fuse.Response.Type);
    memcpy(Key, &fmsg->Message.Fuse.Response.Parameters.Create.Key, sizeof(uuid_t));
    *Generation = fmsg->Message.Fuse.Response.Parameters.Create.Generation;
    *Stat = fmsg->Message.Fuse.Response.Parameters.Create.Attr;
    *Timeout = fmsg->Message.Fuse.Response.Parameters.Create.Timeout;
    return status;

}

void FinesseFreeCreateResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response)
{
    FinesseFreeClientResponse(FinesseClientHandle, Response);
}

