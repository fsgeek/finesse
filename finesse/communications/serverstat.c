
/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
 */

#include <fcinternal.h>

_Static_assert(sizeof(FinesseServerStat) <
                   SHM_PAGE_SIZE - (offsetof(fincomm_message_block, Data) +
                                    offsetof(finesse_msg, Message.Native.Response.Parameters.ServerStat.Data)),
               "Server Stat overflows available space");

int FinesseSendServerStatRequest(finesse_client_handle_t FinesseClientHandle, fincomm_message *Message)
{
    int                           status = 0;
    client_connection_state_t *   ccs    = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr;
    fincomm_message               message = NULL;
    finesse_msg *                 fmsg    = NULL;

    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);
    message = FinesseGetRequestBuffer(fsmr, FINESSE_NATIVE_MESSAGE, FINESSE_NATIVE_REQ_SERVER_STAT);
    assert(NULL != message);
    fmsg = (finesse_msg *)message->Data;

    fmsg->Message.Native.Request.Parameters.ServerStat.Version = FINESSE_SERVERSTATS_VERSION;

    status = FinesseRequestReady(fsmr, message);
    assert(0 != status);  // invalid request ID
    *Message = message;
    status   = 0;

    return status;
}

int FinesseSendServerStatResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message,
                                  FinesseServerStat *ServerStats, int Result)
{
    int                           status = 0;
    fincomm_shared_memory_region *fsmr   = NULL;
    finesse_msg *                 ffm;
    unsigned                      index = (unsigned)(uintptr_t)Client;

    fsmr = FcGetSharedMemoryRegion(FinesseServerHandle, index);
    assert(NULL != fsmr);
    assert(index < SHM_MESSAGE_COUNT);
    assert(0 != Message);
    assert(FINESSE_REQUEST == Message->MessageType);

    Message->Result      = Result;
    Message->MessageType = FINESSE_RESPONSE;

    ffm                                                       = (finesse_msg *)Message->Data;
    ffm->Version                                              = FINESSE_MESSAGE_VERSION;
    ffm->MessageClass                                         = FINESSE_NATIVE_MESSAGE;
    ffm->Message.Native.Response.NativeResponseType           = FINESSE_NATIVE_RSP_SERVER_STAT;
    ffm->Message.Native.Response.Parameters.ServerStat.Result = 0;
    memcpy(&ffm->Message.Native.Response.Parameters.ServerStat.Data, ServerStats, sizeof(FinesseServerStat));

    FinesseResponseReady(fsmr, Message, 0);

    return status;
}

int FinesseGetServerStatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message,
                                 FinesseServerStat **ServerStat)
{
    int                           status = 0;
    client_connection_state_t *   ccs    = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr   = NULL;
    finesse_msg *                 fmsg   = NULL;

    assert(NULL != ServerStat);
    *ServerStat = NULL;
    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);
    assert(0 != Message);

    // This is a blocking get
    status = FinesseGetResponse(fsmr, Message, 1);

    assert(0 != status);
    status = 0;  // FinesseGetResponse is a boolean return function
    assert(FINESSE_RESPONSE == Message->MessageType);
    fmsg = (finesse_msg *)Message->Data;

    assert(FINESSE_SERVER_STAT_VERSION == fmsg->Message.Native.Response.Parameters.ServerStat.Data.Version);
    assert(FINESSE_SERVER_STAT_LENGTH == fmsg->Message.Native.Response.Parameters.ServerStat.Data.Length);
    *ServerStat = &fmsg->Message.Native.Response.Parameters.ServerStat.Data;

    return status;
}

void FinesseFreeServerStatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response)
{
    FinesseFreeClientResponse(FinesseClientHandle, Response);
}
