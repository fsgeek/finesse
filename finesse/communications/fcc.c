/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
 */

#include "fcinternal.h"

static void CleanupClientConnectionState(client_connection_state_t *ccs)
{
    int status;

    if (NULL == ccs) {
        return;
    }

    if ((ccs->reg_info.ClientSharedMemPathNameLength > 0) && (strlen(ccs->reg_info.ClientSharedMemPathName) > 0)) {
        (void)unlink(ccs->reg_info.ClientSharedMemPathName);
        ccs->reg_info.ClientSharedMemPathNameLength = 0;
        ccs->reg_info.ClientSharedMemPathName[0]    = '\0';
    }

    if (ccs->server_connection >= 0) {
        status = close(ccs->server_connection);
        assert(0 == status);
        ccs->server_connection = -1;
    }

    if ((NULL != ccs->server_shm) && (ccs->server_shm_size > 0)) {
        // Shutdown the shared memory region - note the server may have threads
        // blocked, but this should terminate them so the shared memory region
        // can be disconnected.
        FinesseDestroyMemoryRegion(ccs->server_shm);

        status = munmap(ccs->server_shm, ccs->server_shm_size);
        assert(0 == status);
        ccs->server_shm      = NULL;
        ccs->server_shm_size = 0;
    }

    if (ccs->server_shm_fd >= 0) {
        status = close(ccs->server_shm_fd);
        assert(0 == status);
        ccs->server_shm_fd = -1;
    }

    free(ccs);
    ccs = NULL;
}

int FinesseStartClientConnection(finesse_client_handle_t *FinesseClientHandle, const char *MountPoint)
{
    int                               status = 0;
    client_connection_state_t *       ccs    = NULL;
    fincomm_registration_confirmation conf;

    while (0 == status) {
        ccs = (client_connection_state_t *)malloc(sizeof(client_connection_state_t));
        if (NULL == ccs) {
            status = ENOMEM;
            break;
        }
        memset(ccs, 0, sizeof(client_connection_state_t));

        uuid_generate(ccs->reg_info.ClientId);
        status = GenerateClientSharedMemoryName(ccs->reg_info.ClientSharedMemPathName,
                                                sizeof(ccs->reg_info.ClientSharedMemPathName), ccs->reg_info.ClientId);
        assert(0 == status);
        ccs->reg_info.ClientSharedMemPathNameLength = strlen(ccs->reg_info.ClientSharedMemPathName);
        assert(ccs->reg_info.ClientSharedMemPathNameLength > 0);

        ccs->server_shm_fd = shm_open(ccs->reg_info.ClientSharedMemPathName, O_RDWR | O_CREAT | O_EXCL, 0660);
        assert(ccs->server_shm_fd >= 0);

        ccs->server_shm_size = sizeof(fincomm_message_block) + (SHM_PAGE_SIZE * SHM_MESSAGE_COUNT);
        status               = ftruncate(ccs->server_shm_fd, ccs->server_shm_size);
        assert(0 == status);

        ccs->server_shm = mmap(NULL, ccs->server_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, ccs->server_shm_fd, 0);
        assert(MAP_FAILED != ccs->server_shm);
        memcpy(ccs->server_shm, FinesseSharedMemoryRegionSignature, sizeof(FinesseSharedMemoryRegionSignature));

        ccs->server_sockaddr.sun_family = AF_UNIX;
        status = GenerateServerName(MountPoint, ccs->server_sockaddr.sun_path, sizeof(ccs->server_sockaddr.sun_path));
        assert(0 == status);

        ccs->server_connection = socket(AF_UNIX, SOCK_SEQPACKET, 0);
        assert(ccs->server_connection >= 0);

        // If the client has a different UID than the server, this code won't work.  We add a
        // special case here, so that if the client is root, and the server is anything else,
        // we will adjust the UID on the client's shared memory to match the server.  This allows
        // the server to access it.
        struct stat fcs_stat;

        memset(&fcs_stat, 0, sizeof(fcs_stat));
        status = stat(ccs->reg_info.ClientSharedMemPathName, &fcs_stat);
        if (status < 0) {
            if (ENOENT != errno) {
                fprintf(stderr, "%s:%d - stat (%s) failed, errno = %d\n", __func__, __LINE__, ccs->server_sockaddr.sun_path, errno);
            }
            break;
        }

        if (fcs_stat.st_uid != getuid()) {
            fprintf(stderr, "%s:%d - UIDs different, program %d, server %d\n", __func__, __LINE__, getuid(), fcs_stat.st_uid);
            if (0 != getuid()) {
                fprintf(stderr, "%s:%d - UIDs different, not root\n", __func__, __LINE__);
                // Communications won't work in this case because we restrict things to prevent it.
                status = -1;
                errno  = EACCES;
                break;
            }

            status = chown(ccs->reg_info.ClientSharedMemPathName, fcs_stat.st_uid, fcs_stat.st_gid);
            if (status < 0) {
                fprintf(stderr, "%s:%d - chown failed, errno %d\n", __func__, __LINE__, errno);
                break;
            }
        }

        status = connect(ccs->server_connection, &ccs->server_sockaddr, sizeof(ccs->server_sockaddr));
        if (status < 0) {
            break;
        }

        status = send(ccs->server_connection, &ccs->reg_info, sizeof(ccs->reg_info), 0);
        assert(sizeof(ccs->reg_info) == status);

        memset(&conf, 0, sizeof(conf));
        status = recv(ccs->server_connection, &conf, sizeof(conf), 0);
        assert(status >= 0);
        if (sizeof(conf) != status) {
            fprintf(stderr, "%s:%d status = %d, expected %zu\n", __func__, __LINE__, status, sizeof(conf));
        }
        assert(sizeof(conf) == status);
        assert(conf.ClientSharedMemSize == ccs->server_shm_size);
        assert(0 == conf.Result);

        // Unlink shared memory so it will "go away" when the client/server go away
        status = shm_unlink(ccs->reg_info.ClientSharedMemPathName);
        assert(0 == status);

        // Done!
        break;
    }

    if (0 != status) {
        CleanupClientConnectionState(ccs);
        ccs = NULL;
    }

    *FinesseClientHandle = ccs;
    return status;
}

int FinesseStopClientConnection(finesse_client_handle_t FinesseClientHandle)
{
    int                        status = 0;
    client_connection_state_t *ccs    = FinesseClientHandle;
    assert(NULL != ccs);

    CleanupClientConnectionState(ccs);

    return status;
}

// TODO: should th is be in fincomm.c?
void FinesseReleaseRequestBuffer(fincomm_shared_memory_region *RequestRegion, fincomm_message Message)
{
    unsigned  index = (unsigned)((((uintptr_t)Message - (uintptr_t)RequestRegion) / SHM_PAGE_SIZE) - 1);
    u_int64_t bitmap;  // = AllocationBitmap;
    u_int64_t new_bitmap;

    assert(NULL != RequestRegion);
    assert(index < SHM_MESSAGE_COUNT);
    assert(NULL != Message);

    Message->RequestId = 0;  // invalid

    bitmap     = RequestRegion->AllocationBitmap;
    new_bitmap = bitmap & ~make_mask64(index);
    assert(bitmap != new_bitmap);  // freeing an unallocated message

    assert(&RequestRegion->Messages[index] == Message);

    while (!__sync_bool_compare_and_swap(&RequestRegion->AllocationBitmap, bitmap, new_bitmap)) {
        bitmap     = RequestRegion->AllocationBitmap;
        new_bitmap = (bitmap & ~make_mask64(index));
    }

    // fprintf(stderr, "%s (%s:%d): thread %d released index %u\n", __func__, __FILE__, __LINE__, gettid(), index);
}

void FinesseFreeClientResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response)
{
    client_connection_state_t *   ccs = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr;

    assert(NULL != ccs);
    fsmr = (fincomm_shared_memory_region *)ccs->server_shm;
    assert(NULL != fsmr);

    FinesseReleaseRequestBuffer(fsmr, Response);
}

// This is a general function that can be called from any specialized function where
// the only thing they want back is the result code from the operation.
int FinesseGetReplyErrResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, int *Result)
{
    int                           status = 0;
    client_connection_state_t *   ccs    = FinesseClientHandle;
    fincomm_shared_memory_region *fsmr   = NULL;
    finesse_msg *                 fmsg   = NULL;

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
    assert(FINESSE_MESSAGE_VERSION == fmsg->Version);
    assert(FINESSE_FUSE_MESSAGE == fmsg->MessageClass);
    assert(FINESSE_FUSE_RSP_ERR == fmsg->Message.Fuse.Response.Type);
    *Result = fmsg->Result;

    return status;
}
