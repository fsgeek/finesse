/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
 */

#include "fcinternal.h"

int FinesseSendDirMapRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Key, char *Path, fincomm_message *Message)
{
    int                           status = 0;
    client_connection_state_t *   ccs    = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr;
    fincomm_message               message = NULL;
    finesse_msg *                 fmsg    = NULL;
    size_t                        nameLength, bufSize;

    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);
    message = FinesseGetRequestBuffer(fsmr, FINESSE_NATIVE_MESSAGE, FINESSE_NATIVE_REQ_DIRMAP);
    assert(NULL != message);

    fmsg = (finesse_msg *)message->Data;
    memcpy(&fmsg->Message.Native.Request.Parameters.Dirmap.Parent, &Key, sizeof(uuid_t));

    assert(NULL != Path);
    nameLength = strlen(Path);
    bufSize    = SHM_PAGE_SIZE - offsetof(finesse_msg, Message.Native.Request.Parameters.Dirmap.Name);
    assert(nameLength < bufSize);
    memcpy(fmsg->Message.Native.Request.Parameters.Dirmap.Name, Path, nameLength + 1);

    status = FinesseRequestReady(fsmr, message);
    assert(0 != status);  // invalid request ID
    *Message = message;
    status   = 0;

    return status;
}

void *FinesseGetDirMapResponseDataBuffer(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message,
                                         size_t *BufferSize)
{
    void *buffer;
    int   status = 0;

    status = FinesseGetMessageAuxBuffer(FinesseServerHandle, Client, Message, &buffer, BufferSize);
    assert(0 == status);
    assert(NULL != buffer);

    return buffer;
}

int FinesseSendDirMapResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, size_t DataLength,
                              int Result)
{
    int                           status = 0;
    fincomm_shared_memory_region *fsmr   = NULL;
    finesse_msg *                 ffm;
    unsigned                      index    = (unsigned)(uintptr_t)Client;
    const char *                  shm_name = NULL;
    size_t                        nameLength, bufSize;

    fsmr = FcGetSharedMemoryRegion(FinesseServerHandle, index);
    assert(NULL != fsmr);
    assert(index < SHM_MESSAGE_COUNT);
    assert(0 != Message);
    assert(FINESSE_REQUEST == Message->MessageType);

    Message->Result      = Result;
    Message->MessageType = FINESSE_RESPONSE;

    ffm                                                   = (finesse_msg *)Message->Data;
    ffm->Version                                          = FINESSE_MESSAGE_VERSION;
    ffm->MessageClass                                     = FINESSE_FUSE_MESSAGE;
    ffm->Message.Native.Response.NativeResponseType       = FINESSE_NATIVE_RSP_DIRMAP;
    ffm->Message.Native.Response.Parameters.DirMap.Length = DataLength;
    ffm->Message.Native.Response.Parameters.DirMap.Inline = 0;

    shm_name = FinesseGetMessageAuxBufferName(FinesseServerHandle, Client, Message);

    assert(NULL != shm_name);
    nameLength = strlen(shm_name);
    bufSize    = SHM_PAGE_SIZE - offsetof(finesse_msg, Message.Native.Response.Parameters.DirMap.Data);
    assert(nameLength < bufSize);
    memcpy(ffm->Message.Native.Response.Parameters.DirMap.Data, shm_name, nameLength + 1);

    FinesseResponseReady(fsmr, Message, 0);

    return status;
}

int FinesseGetDirMapResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message)
{
    (void)FinesseClientHandle;
    (void)Message;
    return ENOTSUP;
}

void FinesseFreeDirMapResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response)
{
    FinesseFreeClientResponse(FinesseClientHandle, Response);
}
