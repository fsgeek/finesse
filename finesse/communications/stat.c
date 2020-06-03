
/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
*/

#include <fcinternal.h>

int FinesseSendCommonStatRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Parent, uuid_t *Inode, int Flags, const char *Path, fincomm_message *Message)
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
    message->Result = ENOSYS;
    fmsg = (finesse_msg *)message->Data;
    fmsg->Version = FINESSE_MESSAGE_VERSION;
    fmsg->MessageClass = FINESSE_FUSE_MESSAGE;
    fmsg->Message.Fuse.Request.Type = FINESSE_FUSE_REQ_STAT;
    if (NULL != Parent) {
        memcpy(&fmsg->Message.Fuse.Request.Parameters.Stat.ParentInode, Parent, sizeof(uuid_t));
    }
    else {
        memset(&fmsg->Message.Fuse.Request.Parameters.Stat.ParentInode, 0, sizeof(uuid_t));
    }
    if (NULL != Inode) {
        memcpy(&fmsg->Message.Fuse.Request.Parameters.Stat.Inode, Inode, sizeof(uuid_t));
    }
    else {
        memset(&fmsg->Message.Fuse.Request.Parameters.Stat.Inode, 0, sizeof(uuid_t));
    }
    fmsg->Message.Fuse.Request.Parameters.Stat.Flags = Flags;

    assert((NULL != Path) || (NULL != Inode)); // we either need a path (stat) or an inode (fstat)

    fmsg->Message.Fuse.Request.Parameters.Stat.Name[0] = '\0'; // ensure it's null terminated
    if (NULL != Path) {
        nameLength = strlen(Path);
        bufSize = SHM_PAGE_SIZE - offsetof(finesse_msg, Message.Fuse.Request.Parameters.Stat.Name);
        assert(nameLength < bufSize);
        memcpy(fmsg->Message.Fuse.Request.Parameters.Stat.Name, Path, nameLength+1);
    }

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
    
    Message->Result = 0;
    Message->MessageType = FINESSE_RESPONSE;

    assert(NULL != Stat);
    ffm = (finesse_msg *)Message->Data;
    memset(ffm, 0, sizeof(finesse_msg)); // not necessary for production
    ffm->Version = FINESSE_MESSAGE_VERSION;
    ffm->MessageClass = FINESSE_FUSE_MESSAGE;
    ffm->Result = Result;
    ffm->Message.Fuse.Response.Type = FINESSE_FUSE_RSP_ATTR;
    ffm->Message.Fuse.Response.Parameters.Attr.Attr = *Stat;
    ffm->Message.Fuse.Response.Parameters.Attr.AttrTimeout = Timeout;

    FinesseResponseReady(fsmr, Message, 0);

    return status;
}

int FinesseGetStatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, struct stat *Attr, double *Timeout, int *Result)
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
    *Timeout = fmsg->Message.Fuse.Response.Parameters.Attr.AttrTimeout;
    *Result = fmsg->Result;

    return status;

}

void FinesseFreeStatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response)
{
    FinesseFreeClientResponse(FinesseClientHandle, Response);
}
