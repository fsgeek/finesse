/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
*/

#include "finesse.pb-c.h"

#if !defined(FINESSE_MQ_MAX_MESSAGESIZE)
#define FINESSE_MQ_MAX_MESSAGESIZE (1024)
#endif


uuid_t *finesse_get_client_uuid(finesse_client_handle_t *FinesseClientHandle);
uint32_t finesse_get_request_id(finesse_client_handle_t *FinesseClientHandle);
size_t finesse_get_max_message_size(finesse_client_handle_t *FinesseClientHandle);

void finesse_set_client_message_header(finesse_client_handle_t *FinesseClientHandle,
                                       Finesse__FinesseMessageHeader *Header, 
                                       Finesse__FinesseMessageHeader__Operation Operation);
                    
void finesse_set_server_message_header(finesse_server_handle_t *FinesseClientHandle,
                                       Finesse__FinesseMessageHeader *Header,
                                       uint64_t MessageId, 
                                       Finesse__FinesseMessageHeader__Operation Operation);

void finesse_set_client_request_uuid(finesse_client_handle_t *FinesseClientHandle, Finesse__FinesseRequest *Request);
