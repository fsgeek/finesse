//
// (C) Copyright 2020 Tony Mason
//
#include "fs-internal.h"


int FinesseServerHandleNativeRequest(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_msg *fmsg = NULL;
    int status = EINVAL;
    finesse_server_handle_t fsh = (finesse_server_handle_t)se->server_handle;

    if (NULL == fsh) {
        return ENOTCONN;
    }

    assert(FINESSE_REQUEST == Message->MessageType); // nothing else makes sense here
    assert(NULL != Message);
    fmsg = (finesse_msg *)Message->Data;
    assert(NULL != fmsg);
    assert(FINESSE_NATIVE_MESSAGE == fmsg->MessageClass);

    // Now the big long switch statement
    switch (fmsg->Message.Native.Request.NativeRequestType) {
        case FINESSE_NATIVE_REQ_TEST: {
            status = FinesseServerNativeTestRequest(fsh, Client, Message);
            break;

        }
        break;
        
        case FINESSE_NATIVE_REQ_MAP: {
            status = FinesseServerNativeMapRequest(se, Client, Message);
            assert(0 == status); // either that, or pass it back... 
        }
        break;
        
        case FINESSE_NATIVE_REQ_MAP_RELEASE: {
            status = FinesseServerNativeMapReleaseRequest(fsh, Client, Message);
            assert(0 == status);
        }
        break;

        default:
            fmsg->Message.Native.Response.NativeResponseType = FINESSE_FUSE_RSP_ERR;
            fmsg->Message.Native.Response.Parameters.Err.Result = ENOTSUP;
            FinesseSendResponse(fsh, Client, Message);
            break;
    }

    return 0;
}
