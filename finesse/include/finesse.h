/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
*/

#include <uuid/uuid.h>
#include <stdint.h>
#include <sys/statvfs.h>
#include <fincomm.h>

typedef void *finesse_server_handle_t;
typedef void *finesse_client_handle_t;
typedef uint64_t fuse_ino_t;

int FinesseStartServerConnection(finesse_server_handle_t *FinesseServerHandle);
int FinesseStopServerConnection(finesse_server_handle_t FinesseServerHandle);
int FinesseGetRequest(finesse_server_handle_t FinesseServerHandle, void **Client,  void **Request);
int FinesseSendResponse(finesse_server_handle_t FinesseServerHandle, void *Client, void *Response);
void FinesseFreeRequest(finesse_server_handle_t FinesseServerHandle, void *Request);

int FinesseStartClientConnection(finesse_client_handle_t *FinesseClientHandle);
int FinesseStopClientConnection(finesse_client_handle_t FinesseClientHandle);
int FinesseSendRequest(finesse_client_handle_t FinesseClientHandle, void *Request, size_t RequestLen);
int FinesseGetClientResponse(finesse_client_handle_t FinesseClientHandle, void **Response, size_t *ResponseLen);
void FinesseFreeClientResponse(finesse_client_handle_t FinesseClientHandle, void *Response);

int FinesseSendTestRequest(finesse_client_handle_t FinesseClientHandle, uint64_t *RequestId);
int FinesseSendTestResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, int64_t Result);
int FinesseGetTestResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId);

int FinesseSendNameMapRequest(finesse_client_handle_t FinesseClientHandle, char *NameToMap, uint64_t *RequestId);
int FinesseSendNameMapResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, uuid_t *MapKey, int64_t Result);
int FinesseGetNameMapResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, uuid_t *MapKey);

int FinesseSendNameMapReleaseRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *MapKey, uint64_t *RequestId);
int FinesseSendNameMapReleaseResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, int64_t Result);
int FinesseGetNameMapReleaseResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId);

int FinesseSendPathSearchRequest(finesse_client_handle_t FinesseClientHandle, char **Files, char **Paths, uint64_t *RequestId);
int FinesseSendPathSearchResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, char *Path, int64_t Result);
int FinesseGetPathSearchResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, char **Path);
void FinesseFreePathSearchResponse(finesse_client_handle_t FinesseClientHandle, char *PathToFree);

int FinesseSendDirMapRequest(finesse_client_handle_t FinesseClientHandle, uint64_t *RequestId, uuid_t *Key, char *Path);
int FinesseSendDirMapResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, char *Path, int64_t Result);
int FinesseGetDirMapResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId);

int FinesseSendUnlinkRequest(finesse_client_handle_t FinesseClientHandle, const char *NameToUnlink, uint64_t *RequestId);
int FinesseSendUnlinkResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, int64_t Result);
int FinesseGetUnlinkResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId);

int FinesseSendStatfsRequest(finesse_client_handle_t FinesseClientHandle, const char *path, uint64_t *RequestId);
int FinesseSendStatfsResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, struct statvfs *buf, int64_t Result);
int FinesseGetStatfsResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, struct statvfs *buf);

int FinesseSendFstatfsRequest(finesse_client_handle_t FinesseClientHandle, fuse_ino_t nodeid, uint64_t *RequestId);
int FinesseSendFstatfsResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, struct statvfs *buf, int64_t Result);
int FinesseGetFstatfsResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, struct statvfs *buf);

void (*finesse_init)(void);
int finesse_check_prefix(const char *pathname);
int finesse_open(const char *pathname, int flags, ...);
int finesse_creat(const char *pathname, mode_t mode);
int finesse_openat(int dirfd, const char *pathname, int flags, ...);
int finesse_close(int fd);
int finesse_unlink(const char *pathname);
int finesse_unlinkat(int dirfd, const char *pathname, int flags);
int finesse_statfs(const char *path, struct statvfs *buf);
int finesse_fstatfs(fuse_ino_t nodeid, struct statvfs *buf);
//int finesse_mkdir(const char *path, mode_t mode);
//int finesse_mkdirat(int fd, const char *path, mode_t mode);
