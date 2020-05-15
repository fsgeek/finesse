
/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
*/

#include <fcinternal.h>

int FinesseSendStatRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Parent, const char *Path, fincomm_message *Message)
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
    fmsg->Message.Fuse.Request.Type = FINESSE_FUSE_REQ_GETATTR;
    fmsg->Message.Fuse.Request.Parameters.GetAttr.StatType = GETATTR_STAT;
    memcpy(&fmsg->Message.Fuse.Request.Parameters.GetAttr.Options.Path.Parent, Parent, sizeof(uuid_t));

    assert(NULL != Path);
    nameLength = strlen(Path);
    bufSize = SHM_PAGE_SIZE - offsetof(finesse_msg, Message.Fuse.Request.Parameters.GetAttr.Options.Path.Name);
    assert(nameLength < bufSize);
    strncpy(fmsg->Message.Fuse.Request.Parameters.GetAttr.Options.Path.Name, Path, bufSize);

    status = FinesseRequestReady(fsmr, message);
    assert(0 != status); // invalid request ID
    *Message = message;
    status = 0;

    return status;
}

int FinesseSendStatResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, struct stat *Stat, double Timeout, int Result)
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
    ffm->Message.Fuse.Response.Type = FINESSE_FUSE_RSP_ATTR;
    ffm->Message.Fuse.Response.Parameters.Attr.Attr = *Stat;
    ffm->Message.Fuse.Response.Parameters.Attr.AttrTimeout = Timeout;

    FinesseResponseReady(fsmr, Message, 0);

    return status;
}

int FinesseGetStatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, struct stat *Attr)
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
    assert(FINESSE_FUSE_RSP_ATTR == fmsg->Message.Fuse.Response.Type);
    *Attr = fmsg->Message.Fuse.Response.Parameters.Attr.Attr;

    return status;

}

void FinesseFreeStatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response)
{
    FinesseFreeClientResponse(FinesseClientHandle, Response);
}


int FinesseSendLstatRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Parent, const char *Path, fincomm_message *Message)
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
    fmsg->Message.Fuse.Request.Type = FINESSE_FUSE_REQ_GETATTR;
    fmsg->Message.Fuse.Request.Parameters.GetAttr.StatType = GETATTR_LSTAT;
    memcpy(&fmsg->Message.Fuse.Request.Parameters.GetAttr.Options.Path.Parent, Parent, sizeof(uuid_t));

    assert(NULL != Path);
    nameLength = strlen(Path);
    bufSize = SHM_PAGE_SIZE - offsetof(finesse_msg, Message.Fuse.Request.Parameters.GetAttr.Options.Path.Name);
    assert(nameLength < bufSize);
    strncpy(fmsg->Message.Fuse.Request.Parameters.GetAttr.Options.Path.Name, Path, bufSize);

    status = FinesseRequestReady(fsmr, message);
    assert(0 != status); // invalid request ID
    *Message = message;
    status = 0;

    return status;
}

int FinesseSendLstatResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, struct stat *Stat, double Timeout, int Result)
{
    return FinesseSendStatResponse(FinesseServerHandle, Client, Message, Stat, Timeout, Result);
}

int FinesseGetLstatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, struct stat *Attr)
{
    return FinesseGetStatResponse(FinesseClientHandle, Message, Attr);
}

void FinesseFreeLstatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response)
{
    FinesseFreeClientResponse(FinesseClientHandle, Response);
}


int FinesseSendFstatRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Inode, fincomm_message *Message)
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
    fmsg->Message.Fuse.Request.Type = FINESSE_FUSE_REQ_GETATTR;
    fmsg->Message.Fuse.Request.Parameters.GetAttr.StatType = GETATTR_FSTAT;
    memcpy(&fmsg->Message.Fuse.Request.Parameters.GetAttr.Options.Inode, Inode, sizeof(uuid_t));

    status = FinesseRequestReady(fsmr, message);
    assert(0 != status); // invalid request ID
    *Message = message;
    status = 0;

    return status;
}

int FinesseSendFstatResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, struct stat *Stat, double Timeout, int Result)
{
    return FinesseSendStatResponse(FinesseServerHandle, Client, Message, Stat, Timeout, Result);
}

int FinesseGetFstatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, struct stat *Attr)
{
    return FinesseGetLstatResponse(FinesseClientHandle, Message, Attr);
}

void FinesseFreeFstatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response)
{
    FinesseFreeClientResponse(FinesseClientHandle, Response);
}
