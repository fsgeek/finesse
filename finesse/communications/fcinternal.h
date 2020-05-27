//
// (C) Copyright 2020 Tony Mason
// All Rights Reserved
//
#define _GNU_SOURCE

#if !defined(__FCINTERNAL_H__)
#define __FCINTERNAL_H__ (1)

#include <finesse.h>

#if !defined(make_mask64)
#define make_mask64(index) (((u_int64_t)1)<<index)
#endif


fincomm_shared_memory_region *FcGetSharedMemoryRegion(finesse_server_handle_t ServerHandle, unsigned Index);
int FinesseSendRequest(finesse_client_handle_t FinesseClientHandle, fincomm_message Request, size_t RequestLen);
int FinesseGetClientResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message *Response, size_t *ResponseLen);
void FinesseFreeClientResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);
void FinesseReleaseRequestBuffer(fincomm_shared_memory_region *RequestRegion, fincomm_message Message);
int FinesseGetReplyErrResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, int *Result);


void *fincomm_get_aux_shm(finesse_server_handle_t ServerHandle, unsigned ClientIndex, unsigned MessageIndex, size_t *Size);
void fincomm_release_aux_shm(finesse_server_handle_t ServerHandle, unsigned ClientIndex, unsigned MessageIndex);
const char *fincomm_get_aux_shm_name(finesse_server_handle_t ServerHandle, unsigned ClientIndex, unsigned MessageIndex);

#endif // __FCINTERNAL_H__