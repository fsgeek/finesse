/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
 */

#include <fcinternal.h>

// static int bitbucket_internal_ioctl(fuse_req_t req, fuse_ino_t ino, unsigned int cmd, void *arg, struct fuse_file_info *fi,
//                                    unsigned flags, const void *in_buf, size_t in_bufsz, size_t out_bufsz)

int FinesseSendIoctlRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Inode, unsigned int cmd, void *arg,
                            size_t arg_length, unsigned flags, const void *in_buf, size_t in_bufsz);
int FinesseSendIoctlResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, int Result);

int FinesseGetIoctlResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, void **outbuf,
                            size_t **outbuf_size, int *Result);

void FinesseFreeIoctlResponse(finesse_client_handle_t FinesseGetClientHandle, fincomm_message Response);

int FinesseSendIoctlRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Inode, unsigned int cmd, void *arg,
                            size_t arg_length, unsigned flags, const void *in_buf, size_t in_bufsz)
{
    (void)Inode;
    (void)cmd;
    (void)arg;
    (void)arg_length;
    (void)flags;
    (void)in_buf;
    (void)in_bufsz;
    (void)FinesseClientHandle;
#if 0
    client_connection_state_t *ccs = (client_connection_state_t *)FinesseClientHandle;
    void *buffer;

    buffer = FincommAllocateBuffer(ccs->arena);
#endif  // 0
    // Initially, I'm only going to code the big IOCTL method.

    return ENOSYS;
}

int FinesseSendIoctlResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, int Result)
{
    (void)FinesseServerHandle;
    (void)Client;
    (void)Message;
    (void)Result;

    return ENOSYS;
}

int FinesseGetIoctlResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, void **outbuf,
                            size_t **outbuf_size, int *Result)
{
    (void)FinesseClientHandle;
    (void)Message;
    assert(NULL != outbuf);
    *outbuf = NULL;
    assert(NULL != outbuf_size);
    *outbuf_size = 0;
    assert(NULL != Result);
    Result = 0;

    return ENOSYS;
}

void FinesseFreeIoctlResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response)
{
    FinesseFreeClientResponse(FinesseClientHandle, Response);
}
