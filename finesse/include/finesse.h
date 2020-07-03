/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
*/

#include <fincomm.h>
#include <uuid/uuid.h>
#include <stdint.h>
#include <sys/statvfs.h>
#include <sys/statfs.h>

typedef void *finesse_server_handle_t;
typedef void *finesse_client_handle_t;
typedef uint64_t fuse_ino_t;

int FinesseStartServerConnection(const char *MountPoint, finesse_server_handle_t *FinesseServerHandle);
int FinesseStopServerConnection(finesse_server_handle_t FinesseServerHandle);
int FinesseGetRequest(finesse_server_handle_t FinesseServerHandle, void **Client,  fincomm_message *Request);
int FinesseSendResponse(finesse_server_handle_t FinesseServerHandle, void *Client, void *Response);
int FinesseGetMessageAuxBuffer(finesse_server_handle_t FinesseServerHandle,  void *Client, void *Message, void **Buffer, size_t *BufferSize);
const char *FinesseGetMessageAuxBufferName(finesse_server_handle_t FinesseServerHandle, void *Client, void *Message);
void FinesseDestroyFuseRequest(fuse_req_t req);
uint64_t FinesseGetActiveClientCount(finesse_server_handle_t FinesseServerHandle);

int FinesseStartClientConnection(finesse_client_handle_t *FinesseClientHandle, const char *MountPoint);
int FinesseStopClientConnection(finesse_client_handle_t FinesseClientHandle);

int FinesseSendTestRequest(finesse_client_handle_t FinesseClientHandle, fincomm_message *Message);
int FinesseSendTestResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, int Result);
int FinesseGetTestResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message);
void FinesseFreeTestResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);

int FinesseSendServerStatRequest(finesse_client_handle_t FinesseClientHandle, fincomm_message *Message);
int FinesseSendServerStatResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, FinesseServerStat *ServerStats, int Result);
int FinesseGetServerStatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, FinesseServerStat **ServerStat);
void FinesseFreeServerStatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);

int FinesseSendNameMapRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *ParentDir, const char *NameToMap, fincomm_message *Message);
int FinesseSendNameMapResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, uuid_t *MapKey, int Result);
int FinesseGetNameMapResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, uuid_t *MapKey);
void FinesseFreeNameMapResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);

int FinesseSendNameMapReleaseRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *MapKey, fincomm_message *Message);
int FinesseSendNameMapReleaseResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, int Result);
int FinesseGetNameMapReleaseResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message);
void FinesseFreeNameMapReleaseResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);

int FinesseSendPathSearchRequest(finesse_client_handle_t FinesseClientHandle, char **Files, char **Paths, fincomm_message *Message);
int FinesseSendPathSearchResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, char *Path, int Result);
int FinesseGetPathSearchResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, char **Path);
void FinesseFreePathSearchResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);

int FinesseSendDirMapRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Key, char *Path, fincomm_message *Message);
int FinesseSendDirMapResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, size_t DataLength, int Result);
int FinesseGetDirMapResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message);
void *FinesseGetDirMapResponseDataBuffer(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, size_t *BufferSize);
void FinesseFreeDirMapResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);

int FinesseSendUnlinkRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Parent, const char *NameToUnlink, fincomm_message *Message);
int FinesseSendUnlinkResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, int64_t Result);
int FinesseGetUnlinkResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message);
void FinesseFreeUnlinkResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);

int FinesseSendStatfsRequest(finesse_client_handle_t FinesseClientHandle, const char *path, fincomm_message *Message);
int FinesseSendStatfsResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, struct statvfs *buf, int Result);
int FinesseGetStatfsResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, struct statvfs *buf);
void FinesseFreeStatfsResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);

int FinesseSendFstatfsRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Inode, fincomm_message *Message);
int FinesseSendFstatfsResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, struct statvfs *buf, int Result);
int FinesseGetFstatfsResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, struct statvfs *buf);
void FinesseFreeFstatfsResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);

// For stat: NULL Parent, NULL Inode, 0 Follow Link, absolute path
// For fstat: NULL Parent, Inode, 0 Follow Link,
int FinesseSendCommonStatRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Parent, uuid_t *Inode, int Flags, const char *Path, fincomm_message *Message);
#define FinesseSendStatRequest(fch, path, msgptr) FinesseSendCommonStatRequest(fch, NULL, NULL, 0, path, msgptr)
#define FinesseSendFstatRequest(fch, key, msgptr) FinesseSendCommonStatRequest(fch, NULL, key, 0, NULL, msgptr)
#define FinesseSendLstatRequest(fch, path, msgptr) FinesseSendCommonStatRequest(fch, NULL, NULL, AT_SYMLINK_NOFOLLOW, path, msgptr)
#define FinesseSendFstatAtRquest(fch, parent, path, flags, msgptr) FinesseSendCommonStatRequest(fch, parent, NULL, flags, path, msgptr)
int FinesseSendStatResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, const struct stat *Stat, double Timeout, int Result);
int FinesseGetStatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, struct stat *Attr, double *Timeout, int *Result);
void FinesseFreeStatResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);

int FinesseSendAccessRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Parent, const char *Path, mode_t Mode, fincomm_message *Message);
int FinesseSendAccessResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, int Result);
int FinesseGetAccessResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, int *Result);
void FinesseFreeAccessResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);


int FinesseSendCreateRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *Parent, const char *Path, struct stat *Stat, fincomm_message *Message);
int FinesseSendCreateResponse(finesse_server_handle_t FinesseServerHandle, void *Client, fincomm_message Message, uuid_t *Key, uint64_t Generation, struct stat *Stat, double Timeout, int Result);
int FinesseGetCreateResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Message, uuid_t *Key, uint64_t *Generation, struct stat *Stat,  double *Timeout, int *Result);
void FinesseFreeCreateResponse(finesse_client_handle_t FinesseClientHandle, fincomm_message Response);



extern void (*finesse_init)(void);
finesse_client_handle_t *finesse_check_prefix(const char *name);
int finesse_open(const char *pathname, int flags, ...);
int finesse_creat(const char *pathname, mode_t mode);
int finesse_openat(int dirfd, const char *pathname, int flags, ...);
int finesse_close(int fd);
int finesse_unlink(const char *pathname);
int finesse_unlinkat(int dirfd, const char *pathname, int flags);
int finesse_statvfs(const char *path, struct statvfs *buf);
int finesse_fstatvfs(int fd, struct statvfs *buf);
int finesse_fstatfs(int fd, struct statfs *buf);
int finesse_statfs(const char *path, struct statfs *buf);
int finesse_stat(const char *file_name, struct stat *buf);
int finesse_fstat(int filedes, struct stat *buf);
int finesse_lstat(const char *pathname, struct stat *statbuf);
int finesse_fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags);

int finesse_mkdir(const char *path, mode_t mode);
int finesse_mkdirat(int fd, const char *path, mode_t mode);
int finesse_access(const char *pathname, int mode);
int finesse_faccessat(int dirfd, const char *pathname, int mode, int flags);
FILE *finesse_fopen(const char *pathname, const char *mode);
FILE *finesse_fdopen(int fd, const char *mode);
FILE *finesse_freopen(const char *pathname, const char *mode, FILE *stream);
