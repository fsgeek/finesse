
/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
*/

#include <fcinternal.h>

int FinesseSendAccessRequest(finesse_client_handle_t FinesseClientHandle, const char *path, fincomm_message *Message)
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
    fmsg->Message.Fuse.Request.Type = FINESSE_FUSE_REQ_ACCESS;
    fmsg->Message.Fuse.Request.Parameters.Access.AccessType = ACCESS_NAME;

    assert(NULL != path);
    nameLength = strlen(path);
    bufSize = SHM_PAGE_SIZE - offsetof(finesse_msg, Message.Fuse.Request.Parameters.Access.Options.Name);
    assert(nameLength < bufSize);
    memcpy(fmsg->Message.Fuse.Request.Parameters.Access.Options.Name, path, nameLength+1);

    status = FinesseRequestReady(fsmr, message);
    assert(0 != status); // invalid request ID
    *Message = message;
    status = 0;

    return status;
}

int FinesseSendAccessResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, int Result)
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
    ffm->MessageClass = FINESSE_FUSE_MESSAGE;
    ffm->Message.Fuse.Response.Type = FINESSE_FUSE_RSP_ERR; // No data returned here
    ffm->Message.Fuse.Response.Parameters.ReplyErr.Err = Result;
    FinesseResponseReady(fsmr, Message, 0);

    return status;
}

int FinesseGetAccessResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, int *Result)
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
    assert(FINESSE_FUSE_RSP_ERR == fmsg->Message.Fuse.Response.Type);
    *Result = fmsg->Message.Fuse.Response.Parameters.ReplyErr.Err;

    return status;
}

void FinesseFreeAccessResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response)
{
    FinesseFreeClientResponse(FinesseClientHandle, Response);
}


int FinesseSendFaccessRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Inode, fincomm_message *Message)
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
    fmsg->MessageClass = FINESSE_FUSE_MESSAGE;
    fmsg->Message.Fuse.Request.Type = FINESSE_FUSE_REQ_ACCESS;
    fmsg->Message.Fuse.Request.Parameters.Access.AccessType = FSTATFS;

    assert(!uuid_is_null(*Inode));
    memcpy(&fmsg->Message.Fuse.Request.Parameters.Access.Options.Inode, Inode, sizeof(uuid_t));

    status = FinesseRequestReady(fsmr, message);
    assert(0 != status); // invalid request ID
    *Message = message;
    status = 0;

    return status;
}

int FinesseSendFaccessResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, int Result)
{
    return FinesseSendFaccessResponse(FinesseServerHandle, Client, Message, Result);
}

int FinesseGetFaccessResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, int *Result)
{
    return FinesseGetFaccessResponse(FinesseClientHandle, Message, Result);
}

void FinesseFreeFaccessResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response)
{
    FinesseFreeClientResponse(FinesseClientHandle, Response);
}
