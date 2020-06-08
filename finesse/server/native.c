//
// (C) Copyright 2020 Tony Mason
//
#include "fs-internal.h"

static const char *finesse_request_type_to_string(FINESSE_NATIVE_REQ_TYPE Type)
{
    const char *str = "Unknown Native Request Type";

    switch(Type) {
        case FINESSE_NATIVE_REQ_TEST:
            str = "Native Request Test";
            break;
        case FINESSE_NATIVE_REQ_SERVER_STAT:
            str = "Native Request Server Stat";
            break;
        case FINESSE_NATIVE_REQ_MAP:
            str = "Native Request Map";
            break;
        case FINESSE_NATIVE_REQ_MAP_RELEASE:
            str = "Native Request Map Release";
            break;
        case FINESSE_NATIVE_REQ_DIRMAP:
            str = "Native Request Dirmap";
            break;
        case FINESSE_NATIVE_REQ_DIRMAPRELEASE:
            str = "Native Request Dirmap Release";
            break;
        default:
            break; // use default string
    }
    return str;
}

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
    
    fuse_log(FUSE_LOG_DEBUG, "FINESSE %s: native request (0x%p) type %d (%s)\n", 
            __PRETTY_FUNCTION__, fmsg, fmsg->Message.Native.Request.NativeRequestType,
            finesse_request_type_to_string(fmsg->Message.Native.Request.NativeRequestType));

    FinesseCountNativeRequest(fmsg->Message.Native.Request.NativeRequestType);

    // Now the big long switch statement
    switch (fmsg->Message.Native.Request.NativeRequestType) {
        case FINESSE_NATIVE_REQ_TEST: {
            status = FinesseServerNativeTestRequest(fsh, Client, Message);
            assert(0 == status);
            break;

        }
        break;

        case FINESSE_NATIVE_REQ_SERVER_STAT: {
            status = FinesseServerNativeServerStatRequest(fsh, Client, Message);
            assert(0 == status);
            break;
        }
        
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
            fmsg->Result = ENOTSUP;
            FinesseSendResponse(fsh, Client, Message);
            FinesseCountNativeResponse(FINESSE_FUSE_RSP_ERR);
            break;
    }

    return 0;
}
