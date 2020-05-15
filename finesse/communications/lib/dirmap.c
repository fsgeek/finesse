/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
*/

#include "fcinternal.h"

int FinesseSendDirMapRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Key, char *Path, fincomm_message *Message)
{
    (void) FinesseClientHandle;
    (void) Key;
    (void) Path;
    *Message = NULL;
    return ENOTSUP;
}

int FinesseSendDirMapResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, char *Path, int Result)
{
    (void) FinesseServerHandle;
    (void) Client;
    (void) Message;
    (void) Path;
    (void) Result;
    return ENOTSUP;

}

int FinesseGetDirMapResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message)
{
    (void) FinesseClientHandle;
    (void) Message;
    return ENOTSUP;
}

void FinesseFreeDirMapResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response)
{
    return FinesseFreeClientResponse(FinesseClientHandle, Response);
}

