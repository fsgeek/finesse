/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
*/
#include <finesse.h>
#include "fincomm.h"

static fincomm_shared_memory_region *CreateInMemoryRegion(void)
{
    int status;
    fincomm_shared_memory_region *fsmr = NULL;


}
//
// This is the new communications lib implementations of these APIs
//


int FinesseGetRequest(finesse_server_handle_t FinesseServerHandle, void **Request, size_t *RequestLen)
{

}

int FinesseSendResponse(finesse_server_handle_t FinesseServerHandle, const uuid_t *ClientUuid, void *Response, size_t ResponseLen)
{

}

void FinesseFreeRequest(finesse_server_handle_t FinesseServerHandle, void *Request)
{

}
