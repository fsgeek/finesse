#include "finesse-internal.h"
#include <stdarg.h>
#include <uuid/uuid.h>
#include "finesse.pb-c.h"
#include "mqcomm.h"

static int fin_fstatfs_call(fuse_ino_t nodeid, struct statvfs *buf);

static int fin_fstatfs(fuse_ino_t nodeid, struct statvfs *buf)
{
    typedef int (*orig_fstatfs_t)(fuse_ino_t nodeid, struct statvfs *buf);
    static orig_fstatfs_t orig_fstatfs = NULL;

    if (NULL == orig_fstatfs) {
        orig_fstatfs = (orig_fstatfs_t)dlsym(RTLD_NEXT, "fstatfs");

        assert(NULL != orig_fstatfs);
        if (NULL == orig_fstatfs) {
            return EACCES;
        }
    }

    return orig_fstatfs(nodeid, buf);
}

int finesse_fstatfs(fuse_ino_t nodeid, struct statvfs *buf)
{
    int status;

    finesse_init();

    status = fin_fstatfs_call(nodeid, buf);

    if (0 > status) {
        status = fin_fstatfs(nodeid, buf);
    }

    return status;
}

static int fin_fstatfs_call(fuse_ino_t nodeid, struct statvfs *buf)
{
    int status;
    uint64_t req_id;

    status = FinesseSendFstatfsRequest(finesse_client_handle, nodeid, &req_id);
    while (0 == status) {
        status = FinesseGetFstatfsResponse(finesse_client_handle, req_id, buf);
        break;
    }

    return status;
}

static int fin_statfs_call(const char *path, struct statvfs *buf);

static int fin_statfs(const char *path, struct statvfs *buf)
{
    typedef int (*orig_statfs_t)(const char *path, struct statvfs *buf);
    static orig_statfs_t orig_statfs = NULL;

    if (NULL == orig_statfs) {
        orig_statfs = (orig_statfs_t)dlsym(RTLD_NEXT, "statfs");

        assert(NULL != orig_statfs);
        if (NULL == orig_statfs) {
            return EACCES;
        }
    }

    return orig_statfs(path, buf);
}

int finesse_statfs(const char *path, struct statvfs *buf)
{
    int status;

    finesse_init();

    status = fin_statfs_call(path, buf);

    if (0 > status) {
        status = fin_statfs(path, buf);
    }

    return status;
}

static int fin_statfs_call(const char *path, struct statvfs *buf)
{
    int status;
    uint64_t req_id;

    status = FinesseSendStatfsRequest(finesse_client_handle, path, &req_id);
    while (0 == status) {
        status = FinesseGetStatfsResponse(finesse_client_handle, req_id, buf);
        break;
    }

    return status;
}

int FinesseSendFstatfsRequest(finesse_client_handle_t FinesseClientHandle, fuse_ino_t NodeToStat, uint64_t *RequestId)
{
    Finesse__FinesseRequest req = FINESSE__FINESSE_REQUEST__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    Finesse__FinesseRequest__FStatfs fstatfsreq = FINESSE__FINESSE_REQUEST__FSTATFS__INIT;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;

    while (NULL == buffer) {
        finesse_set_client_message_header(FinesseClientHandle, &header, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__FSTATFS);

        req.header = &header;
        req.clientuuid.data = (uint8_t *)finesse_get_client_uuid(FinesseClientHandle);
        req.clientuuid.len = sizeof(uuid_t);
        req.request_case = FINESSE__FINESSE_REQUEST__REQUEST_FSTATFS_REQ;
	req.fstatfsreq = &fstatfsreq;

        fstatfsreq.nodeid = (fuse_ino_t)NodeToStat;

        buffer_len = finesse__finesse_request__get_packed_size(&req);

        // TODO: note that if it is too big we'll have to figure out a different way
        // to send this information across (and I *do* expect that to happen)
        // my expectation is that I'll need to create a shared memory buffer for this.
        if (buffer_len > FINESSE_MQ_MAX_MESSAGESIZE) {
            status = -EINVAL;
            break;
        }

        buffer = malloc(buffer_len);
        if (NULL == buffer) {
            status = -ENOMEM;
            break;
        }

        packed_buffer_len = finesse__finesse_request__pack(&req, (uint8_t *)buffer);
        assert(buffer_len == packed_buffer_len);

        status = FinesseSendRequest(FinesseClientHandle, buffer, buffer_len);

        break;
    }


    // cleanup
    if (NULL != buffer) {
        free(buffer);
        buffer = NULL;
    }

    *RequestId = header.messageid;
    return status;
}

int FinesseSendFstatfsResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, struct statvfs *buf, int64_t Result)
{
    Finesse__FinesseResponse rsp = FINESSE__FINESSE_RESPONSE__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    Finesse__FinesseResponse__FStatfs fstatfs = FINESSE__FINESSE_RESPONSE__FSTATFS__INIT;
    Finesse__StatFsStruc statfsstruc = FINESSE__STAT_FS_STRUC__INIT;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;
 
    if (buf == NULL) 
        return EFAULT; 
    
    while (NULL == buffer) {
        finesse_set_server_message_header(FinesseServerHandle, &header, RequestId, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__FSTATFS);

        rsp.header = &header;
        rsp.status = Result;
        rsp.response_case = FINESSE__FINESSE_RESPONSE__RESPONSE_FSTATFS_RSP;
	rsp.fstatfsrsp = &fstatfs;
	statfsstruc.f_bsize = buf->f_bsize;
	statfsstruc.f_blocks = buf->f_blocks;
	statfsstruc.f_bfree = buf->f_bfree;
	statfsstruc.f_bavail = buf->f_bavail;
	statfsstruc.f_files = buf->f_files;
	statfsstruc.f_ffree = buf->f_ffree;
	statfsstruc.f_fsid = buf->f_fsid;
	statfsstruc.f_frsize = buf->f_frsize;
	statfsstruc.f_flag = buf->f_flag;
	statfsstruc.f_namemax = buf->f_namemax;
	statfsstruc.f_favail = buf->f_favail;	
	rsp.fstatfsrsp->statfsstruc = &statfsstruc;

        buffer_len = finesse__finesse_response__get_packed_size(&rsp);

        if (buffer_len > finesse_get_max_message_size(FinesseServerHandle)) {
            status = -EINVAL;
            break;
        }

        buffer = malloc(buffer_len);
        if (NULL == buffer) {
            status = -ENOMEM;
            break;
        }

        packed_buffer_len = finesse__finesse_response__pack(&rsp, (uint8_t *)buffer);
        assert(buffer_len == packed_buffer_len);

        status = FinesseSendResponse(FinesseServerHandle, ClientUuid, buffer, buffer_len);

        break;
    }


    // cleanup
    if (NULL != buffer) {
        free(buffer);
        buffer = NULL;
    }

    return status;
}

int FinesseGetFstatfsResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, struct statvfs *buf)
{
    Finesse__FinesseResponse *rsp = NULL;
    void *buffer = NULL;
    size_t buffer_len = 0;
    int status;

    while (NULL == buffer) {
        status = FinesseGetClientResponse(FinesseClientHandle, &buffer, &buffer_len);

        if (0 != status)
        {
            break;
        }

        rsp = finesse__finesse_response__unpack(NULL, buffer_len, (const uint8_t *)buffer);

	if (NULL == rsp)
        {
            status = -EINVAL;
            break;
        }

        assert(rsp->header->messageid == RequestId);
        if (0 == rsp->status) {
	    assert(rsp->fstatfsrsp);
	    if (NULL == buf) {
	      status = -ENOMEM;
	      break;
	    }
	    buf->f_bsize = rsp->fstatfsrsp->statfsstruc->f_bsize; 
            buf->f_blocks = rsp->fstatfsrsp->statfsstruc->f_blocks;
            buf->f_bfree = rsp->fstatfsrsp->statfsstruc->f_bfree;
            buf->f_bavail = rsp->fstatfsrsp->statfsstruc->f_bavail;
            buf->f_files = rsp->fstatfsrsp->statfsstruc->f_files; 
            buf->f_ffree = rsp->fstatfsrsp->statfsstruc->f_ffree; 
            buf->f_fsid = rsp->fstatfsrsp->statfsstruc->f_fsid; 
            buf->f_frsize = rsp->fstatfsrsp->statfsstruc->f_frsize;
            buf->f_flag = rsp->fstatfsrsp->statfsstruc->f_flag; 
            buf->f_namemax = rsp->fstatfsrsp->statfsstruc->f_namemax;
            buf->f_favail = rsp->fstatfsrsp->statfsstruc->f_favail;
	}
	
        status = rsp->status;
        break;
    }

    if (NULL != buffer)
    {
        FinesseFreeClientResponse(FinesseClientHandle, buffer);
        buffer = NULL;
    }

    return status;
}


int FinesseSendStatfsRequest(finesse_client_handle_t FinesseClientHandle, const char *Path, uint64_t *RequestId)
{
    Finesse__FinesseRequest req = FINESSE__FINESSE_REQUEST__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    Finesse__FinesseRequest__Statfs statfsreq = FINESSE__FINESSE_REQUEST__STATFS__INIT;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;

    while (NULL == buffer) {
        finesse_set_client_message_header(FinesseClientHandle, &header, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__STATFS);

        req.header = &header;
        req.clientuuid.data = (uint8_t *)finesse_get_client_uuid(FinesseClientHandle);
        req.clientuuid.len = sizeof(uuid_t);
        req.request_case = FINESSE__FINESSE_REQUEST__REQUEST_STATFS_REQ;
	req.statfsreq = &statfsreq;

        statfsreq.path = (char *)Path;

        buffer_len = finesse__finesse_request__get_packed_size(&req);

        // TODO: note that if it is too big we'll have to figure out a different way
        // to send this information across (and I *do* expect that to happen)
        // my expectation is that I'll need to create a shared memory buffer for this.
        if (buffer_len > FINESSE_MQ_MAX_MESSAGESIZE) {
            status = -EINVAL;
            break;
        }

        buffer = malloc(buffer_len);
        if (NULL == buffer) {
            status = -ENOMEM;
            break;
        }

        packed_buffer_len = finesse__finesse_request__pack(&req, (uint8_t *)buffer);
        assert(buffer_len == packed_buffer_len);

        status = FinesseSendRequest(FinesseClientHandle, buffer, buffer_len);

        break;
    }


    // cleanup
    if (NULL != buffer) {
        free(buffer);
        buffer = NULL;
    }

    *RequestId = header.messageid;
    return status;
}

int FinesseSendStatfsResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, struct statvfs *buf, int64_t Result)
{
    Finesse__FinesseResponse rsp = FINESSE__FINESSE_RESPONSE__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    Finesse__FinesseResponse__Statfs statfs = FINESSE__FINESSE_RESPONSE__STATFS__INIT;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;
    
    if (buf == NULL) 
        return EFAULT; 

    while (NULL == buffer) {
        finesse_set_server_message_header(FinesseServerHandle, &header, RequestId, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__STATFS);

        rsp.header = &header;
        rsp.status = Result;
        rsp.response_case = FINESSE__FINESSE_RESPONSE__RESPONSE_STATFS_RSP;
	rsp.statfsrsp = &statfs;
	rsp.statfsrsp->statfsstruc->f_bsize = buf->f_bsize;
	rsp.statfsrsp->statfsstruc->f_blocks = buf->f_blocks;
	rsp.statfsrsp->statfsstruc->f_bfree = buf->f_bfree;
	rsp.statfsrsp->statfsstruc->f_bavail = buf->f_bavail;
	rsp.statfsrsp->statfsstruc->f_files = buf->f_files;
	rsp.statfsrsp->statfsstruc->f_ffree = buf->f_ffree;
	rsp.statfsrsp->statfsstruc->f_fsid = buf->f_fsid;
	rsp.statfsrsp->statfsstruc->f_frsize = buf->f_frsize;
	rsp.statfsrsp->statfsstruc->f_flag = buf->f_flag;
	rsp.statfsrsp->statfsstruc->f_namemax = buf->f_namemax;
	rsp.statfsrsp->statfsstruc->f_favail = buf->f_favail;	

        buffer_len = finesse__finesse_response__get_packed_size(&rsp);

        if (buffer_len > finesse_get_max_message_size(FinesseServerHandle)) {
            status = -EINVAL;
            break;
        }

        buffer = malloc(buffer_len);
        if (NULL == buffer) {
            status = -ENOMEM;
            break;
        }

        packed_buffer_len = finesse__finesse_response__pack(&rsp, (uint8_t *)buffer);
        assert(buffer_len == packed_buffer_len);

        status = FinesseSendResponse(FinesseServerHandle, ClientUuid, buffer, buffer_len);

        break;
    }


    // cleanup
    if (NULL != buffer) {
        free(buffer);
        buffer = NULL;
    }

    return status;
}

int FinesseGetStatfsResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, struct statvfs *buf)
{
    Finesse__FinesseResponse *rsp = NULL;
    void *buffer = NULL;
    size_t buffer_len = 0;
    int status;

    while (NULL == buffer)
    {
        status = FinesseGetClientResponse(FinesseClientHandle, &buffer, &buffer_len);

        if (0 != status)
        {
            break;
        }

        rsp = finesse__finesse_response__unpack(NULL, buffer_len, (const uint8_t *)buffer);

	if (NULL == rsp)
        {
            status = -EINVAL;
            break;
        }

        assert(rsp->header->messageid == RequestId);
        buf->f_bsize = rsp->statfsrsp->statfsstruc->f_bsize; 
        buf->f_blocks = rsp->statfsrsp->statfsstruc->f_blocks;
        buf->f_bfree = rsp->statfsrsp->statfsstruc->f_bfree;
        buf->f_bavail = rsp->statfsrsp->statfsstruc->f_bavail;
        buf->f_files = rsp->statfsrsp->statfsstruc->f_files; 
        buf->f_ffree = rsp->statfsrsp->statfsstruc->f_ffree; 
        buf->f_fsid = rsp->statfsrsp->statfsstruc->f_fsid; 
        buf->f_frsize = rsp->statfsrsp->statfsstruc->f_frsize;
        buf->f_flag = rsp->statfsrsp->statfsstruc->f_flag; 
        buf->f_namemax = rsp->statfsrsp->statfsstruc->f_namemax;
        buf->f_favail = rsp->statfsrsp->statfsstruc->f_favail;

        status = rsp->status;
        break;
    }

    if (NULL != buffer)
    {
        FinesseFreeClientResponse(FinesseClientHandle, buffer);
        buffer = NULL;
    }

    return status;
}
