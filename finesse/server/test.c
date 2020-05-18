/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"

int FinesseServerNativeTestRequest(finesse_server_handle_t Fsh, void *Client, fincomm_message Message)
{
    int status;

    status = FinesseSendTestResponse(Fsh, Client, Message, 0);

    return status;
}
