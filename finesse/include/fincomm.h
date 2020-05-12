//
// (C) Copyright 2020 Tony Mason
// All Rights Reserved
//
#define _GNU_SOURCE

#pragma once

#if !defined(__FINESSE_FINCOMM_H__)
#define __FINESSE_FINCOMM_H__ (1)

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <aio.h>
#include <mqueue.h>
#include <stddef.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <finesse.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <dirent.h>
#include <sys/mman.h>
#if !defined(FUSE_USE_VERSION)
#define FUSE_USE_VERSION FUSE_VERSION
#endif
#include <fuse_lowlevel.h>


#define FINESSE_SERVICE_PREFIX "/tmp/finesse"


#define OPTIMAL_ALIGNMENT_SIZE (64) // this should be whatever the best choice is for laying out cache line optimal data structures
#define MAX_SHM_PATH_NAME (128)     // secondary shared memory regions must fit within a buffer of this size (including a null terminator)
#define SHM_MESSAGE_COUNT (64)         // this is the maximum number of parallel/simultaneous messages per client.
#define SHM_PAGE_SIZE (4096)        // this should be the page size of the underlying machine.

//
// The registration structure is how the client connects to the server
//
typedef struct {
    uuid_t      ClientId;
    u_int32_t   ClientSharedMemPathNameLength;
    char        ClientSharedMemPathName[MAX_SHM_PATH_NAME];
} fincomm_registration_info;

typedef struct {
    uuid_t      ServerId;
    size_t      ClientSharedMemSize;
    u_int32_t   Result;
} fincomm_registration_confirmation;

typedef enum {
    FINESSE_FUSE_MSG_REQUEST  = 241,
    FINESSE_FUSE_MSG_RESPONSE = 242,
} FINESSE_FUSE_MSG_TYPE;


//
// Each shared memory region consists of a set of communications blocks
//
typedef struct _fincomm_message_block {
    u_int64_t               RequestId;
    u_int32_t               Result;
    FINESSE_FUSE_MSG_TYPE   OperationType;
    u_int8_t                Data[SHM_PAGE_SIZE-16];
} fincomm_message_block;

_Static_assert(offsetof(fincomm_message_block, Data) == 16, "fincomm_message_block not packed properly");
_Static_assert(sizeof(fincomm_message_block) == SHM_PAGE_SIZE, "fincomm_message_block not packed properly");

typedef fincomm_message_block *fincomm_message;

// ensure we have proper cache line alignment
_Static_assert(0 == sizeof(fincomm_message_block) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(SHM_PAGE_SIZE == sizeof(fincomm_message_block), "Alignment wrong");


//
// The shared memory region has a header, followed by (page aligned)
// message blocks
//
typedef struct {
    uuid_t          ClientId;
    uuid_t          ServerId;
    u_int64_t       RequestBitmap;
    u_int64_t       RequestWaiters;
    pthread_mutex_t RequestMutex;
    pthread_cond_t  RequestPending;
    u_int8_t        align0[192-((2 * sizeof(uuid_t)) + (2*sizeof(u_int64_t)) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t))];
    u_int64_t       ResponseBitmap;
    pthread_mutex_t ResponseMutex;
    pthread_cond_t  ResponsePending;
    u_int8_t        align1[128-(sizeof(u_int64_t) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t))];
    char            secondary_shm_path[MAX_SHM_PATH_NAME];
    unsigned        LastBufferAllocated; // allocation hint
    u_int64_t       AllocationBitmap;
    u_int64_t       RequestId;
    u_int64_t       ShutdownRequested;
    u_int8_t        align2[64-(4 * sizeof(u_int64_t))];
    u_int8_t        Data[4096-(8*64)];
    fincomm_message_block   Messages[SHM_MESSAGE_COUNT];
} fincomm_shared_memory_region;

_Static_assert(0 == sizeof(fincomm_shared_memory_region) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, ResponseBitmap) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, secondary_shm_path) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, LastBufferAllocated) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, Data) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, Messages) % SHM_PAGE_SIZE, "Alignment wrong");
_Static_assert(0 == sizeof(fincomm_shared_memory_region) % SHM_PAGE_SIZE, "Length Wrong");

int GenerateServerName(char *ServerName, size_t ServerNameLength);
int GenerateClientSharedMemoryName(char *SharedMemoryName, size_t SharedMemoryNameLength, uuid_t ClientId);


typedef struct _client_connection_state {
    fincomm_registration_info       reg_info;
    int                             server_connection;
    struct sockaddr_un              server_sockaddr;
    int                             server_shm_fd;
    size_t                          server_shm_size;
    void *                          server_shm;
    int                             aux_shm_fd;
    int                             aux_shm_size;
    void *                          aux_shm;
    char                            aux_shm_path[MAX_SHM_PATH_NAME];
} client_connection_state_t;

typedef struct server_connection_state {
    fincomm_registration_info       reg_info;
    int                             client_connection;
    int                             client_shm_fd;
    size_t                          client_shm_size;
    void *                          client_shm;
    int                             aux_shm_fd;
    int                             aux_shm_size;
    void *                          aux_shm;
    pthread_t                       monitor_thread;
    uint8_t                         monitor_thread_active;
    char                            aux_shm_path[MAX_SHM_PATH_NAME];
} server_connection_state_t;

// This declares the operations that correspond to various message types
typedef enum {
    FINESSE_FUSE_REQ_LOOKUP = 42,
    FINESSE_FUSE_REQ_FORGET = 43,
    FINESSE_FUSE_REQ_GETATTR = 44,
    FINESSE_FUSE_REQ_SETATTR = 45,
    FINESSE_FUSE_REQ_READLINK = 46,
    FINESSE_FUSE_REQ_MKNOD = 47,
    FINESSE_FUSE_REQ_MKDIR = 48,
    FINESSE_FUSE_REQ_UNLINK = 49,
    FINESSE_FUSE_REQ_RMDIR = 50,
    FINESSE_FUSE_REQ_SYMLINK = 51,
    FINESSE_FUSE_REQ_RENAME = 52,
    FINESSE_FUSE_REQ_LINK = 53,
    FINESSE_FUSE_REQ_OPEN = 54,
    FINESSE_FUSE_REQ_READ = 55,
    FINESSE_FUSE_REQ_WRITE = 56,
    FINESSE_FUSE_REQ_FLUSH = 57,
    FINESSE_FUSE_REQ_RELEASE = 58,
    FINESSE_FUSE_REQ_FSYNC = 59,
    FINESSE_FUSE_REQ_OPENDIR = 60,
    FINESSE_FUSE_REQ_READDIR = 61,
    FINESSE_FUSE_REQ_RELEASEDIR = 62,
    FINESSE_FUSE_REQ_FSYNCDIR = 63,
    FINESSE_FUSE_REQ_STATFS = 64,
    FINESSE_FUSE_REQ_SETXATTR = 65,
    FINESSE_FUSE_REQ_GETXATTR = 66,
    FINESSE_FUSE_REQ_LISTXATTR = 67,
    FINESSE_FUSE_REQ_REMOVEXATTR = 68,
    FINESSE_FUSE_REQ_ACCESS = 69,
    FINESSE_FUSE_REQ_CREATE = 70,
    FINESSE_FUSE_REQ_GETLK = 71,
    FINESSE_FUSE_REQ_SETLK = 72,
    FINESSE_FUSE_REQ_BMAP = 73,
    FINESSE_FUSE_REQ_IOCTL = 74,
    FINESSE_FUSE_REQ_POLL = 75,
    FINESSE_FUSE_REQ_WRITE_BUF = 76,
    FINESSE_FUSE_REQ_RETRIEVE_REPLY = 77,
    FINESSE_FUSE_REQ_FORGET_MULTI = 78,
    FINESSE_FUSE_REQ_FLOCK = 79,
    FINESSE_FUSE_REQ_FALLOCATE = 80,
    FINESSE_FUSE_REQ_READDIRPLUS = 81,
    FINESSE_FUSE_REQ_COPY_FILE_RANGE = 82,
    FINESSE_FUSE_REQ_LSEEK = 83,
    FINESSE_FUSE_REQ_MAP = 128,
    FINESSE_FUSE_REQ_TEST = 129,
} FINESSE_FUSE_REQ_TYPE;

typedef enum {
    FINESSE_FUSE_RSP_NONE = 512,
    FINESSE_FUSE_RSP_ERR,
    FINESSE_FUSE_RSP_ENTRY,
    FINESSE_FUSE_RSP_CREATE,
    FINESSE_FUSE_RSP_ATTR,
    FINESSE_FUSE_RSP_READLINK,
    FINESSE_FUSE_RSP_OPEN,
    FINESSE_FUSE_RSP_WRITE,
    FINESSE_FUSE_RSP_BUF,
    FINESSE_FUSE_RSP_DATA,
    FINESSE_FUSE_RSP_IOV,
    FINESSE_FUSE_RSP_STATFS,
    FINESSE_FUSE_RSP_XATTR,
    FINESSE_FUSE_RSP_LOCK,
    FINESSE_FUSE_RSP_BMAP,
    FINESSE_FUSE_RSP_IOCTL,
    FINESSE_FUSE_RSP_IOCTL_IOV,
    FINESSE_FUSE_RSP_LSEEK,
    FINESSE_FUSE_RSP_NOTIFY_POLL,
    FINESSE_FUSE_RSP_NOTIFY_INVAL_INODE,
    FINESSE_FUSE_RSP_NOTIFY_INVAL_ENTRY,
    FINESSE_FUSE_RSP_NOTIFY_DELETE,
    FINESSE_FUSE_RSP_NOTIFY_STORE,
    FINESSE_FUSE_RSP_NOTIFY_RETRIEVE,
} FINESSE_FUSE_RSP_TYPE;

typedef struct {
    FINESSE_FUSE_REQ_TYPE Type; // Message type
    union {
        struct {
            uuid_t  Parent;
            char Name[0];
        } Lookup;

        struct {
            uuid_t Inode;
            uint64_t Nlookup;
        } Forget;

        struct {
            uuid_t Inode;
        } GetAttr;

        struct {
            uuid_t Inode;
            struct stat Attr;
            int ToSet;
        } SetAttr;

        struct {
            uuid_t Inode;
        } ReadLink;

        struct {
            uuid_t Parent;
            mode_t Mode;
            dev_t  Dev;
            char Name[0];
        } Mknod;

        struct {
            uuid_t Parent;
            mode_t mode;
            char Name[0];
        } Mkdir;

        struct {
            uuid_t Parent;
            char Name[0];
        } Unlink;

        struct {
            uuid_t Parent;
            char Name[0];
        } Rmdir;

        struct {
            uuid_t Symlink;
            // Pair of null terminated strings
            char LinkAndName[0];
        } Symlink;

        struct {
            uuid_t Parent;
            uuid_t NewParent;
            unsigned int flags;
            // Pair of null terminated strings
            char OldAndNewName[0];
        } Rename;

        struct {
            uuid_t Inode;
            uuid_t NewParent;
            char NewName[0];
        } Link;

        struct {
            uuid_t Inode;
            int Flags;
        } Open;

        struct {
            uuid_t Inode;
            size_t Size;
            off_t  Offset;
        } Read;

        struct {
            uuid_t Inode;
            size_t Size;
            off_t  Offset;
            char SharedMemoryName[MAX_SHM_PATH_NAME+1];
        } LargeRead;

        struct {
            uuid_t   Inode;
            uint16_t Size;
            off_t    Offset;
            char Buffer[0];
        } SmallWrite;

        struct {
            uuid_t   Inode;
            uint64_t Size;
            off_t    Offset;
            char SharedMemoryName[MAX_SHM_PATH_NAME+1];
        } LargeWrite;

        struct {
            uuid_t  Inode;
        } Flush;

        struct {
            uuid_t Inode;
        } Release;

        struct {
            uuid_t Inode;
            int    DataSync;
        } Fsync;

        struct {
            uuid_t Inode;
        } Opendir;

        struct {
            uuid_t Inode;
            uint16_t Size;
            off_t    Offset;
        } Readdir;

        struct {
            uuid_t Inode;
        } Releasedir;

        struct {
            uuid_t Inode;
            int DataSync;
        } Fsyncdir;

        struct {
            uuid_t Inode;
        } Statfs;

        struct {
            uuid_t Inode;
            uint16_t Size;
            int      Flags;
            // two null-terminated strings
            char NameAndValue[0];
        } Setxattr;

        struct {
            uuid_t Inode;
            char Name;
            size_t MaxSize;
        } Getxattr;

        struct {
            uuid_t Inode;
            size_t MaxSize;
        } Listxattr;

        struct {
            uuid_t Inode;
            char Name[0];
        } Removexattr;

        struct {
            uuid_t Inode;
            int Mask;
        } Access;

        struct {
            uuid_t Parent;
            mode_t Mode;
            char Name[0];
        } Create;

        struct {
            uuid_t Inode;
        } GetLk;

        struct {
            uuid_t Inode;
            struct flock Lock;
            int Sleep;
        } SetLk;

        struct {
            uuid_t Inode;
            size_t BlockSize;
        } Bmap;

        struct {
            uuid_t Inode;
            int    Command;
            unsigned Flags;
            uint16_t InputSize;
            uint16_t  OutputSize;
            char     InputBuffer[0];
        } SmallIoctl;

        struct {
            uuid_t Inode;
            int    Command;
            unsigned Flags;
            size_t InputSize;
            size_t OutputSize;
            char InputSharedMemoryName[MAX_SHM_PATH_NAME+1];
            char OutputSharedMemoryName[MAX_SHM_PATH_NAME+1];
        } LargeIoctl;

        struct {
            uuid_t Inode;
        } Poll;
        
        struct {
            uuid_t Inode;
        } WriteBuf;

        struct {
            uuid_t Inode;
            uintptr_t   Cookie;
            off_t Offset;
        } RetrieveReply;

        struct {
            uint16_t Count;
            uuid_t   Inodes[250];
        } ForgetMulti;

        struct {
            uuid_t  Inode;
            int     Operation;
        } Flock;

        struct {
            uuid_t  Inode;
            int     Mode;
            off_t   Offset;
            off_t   Length;
        } Fallocate;

        struct {
            uuid_t Inode;
            uint16_t Size;
            off_t    Offset;
        } SmallReaddirplus;

        struct {
            uuid_t   Inode;
            uint16_t Size;
            off_t    Offset;
            char SharedMemoryName[MAX_SHM_PATH_NAME+1];
        } LargeReaddirplus;

        struct {
            uuid_t  InputInode;
            off_t   InputOffset;
            uuid_t  OutputInode;
            off_t   OutputOffset;
            size_t  Length;
            int     Flags;            
        } CopyFileRange;

        struct {
            uuid_t Inode;
            off_t  Offset;
            int    Whence;
        } Lseek;

        struct {
            uuid_t  Inode;
            char    Name[0];
        } Map;

        struct {
            uint64_t Version;
        } Test;

    } Request;
} finesse_fuse_request;

typedef struct {
    FINESSE_FUSE_RSP_TYPE Type;
    union {
        struct {
            int Err;
        } ReplyErr;

        struct {
            struct fuse_entry_param EntryParam;
        } EntryParam;

        struct {
            struct fuse_entry_param EntryParam;
            struct fuse_file_info FileInfo;
        } Create;

        struct {
            struct stat Attr;
            double AttrTimeout;
        } Attr;

        struct {
            char Link[0];
        } ReadLink;

        struct {
            struct fuse_file_info FileInfo;
        } Open;

        struct {
            size_t Count;
        } Write;

        struct {
            uint16_t Size;
            char Buffer[0];
        } SmallBuffer;

        struct {
            // Use this when what's being returned
            // won't fit.
            size_t Size;
            char SharedMemoryName[MAX_SHM_PATH_NAME+1];
        } LargeBuffer;

        struct {
            struct statvfs StatBuffer;
        } StatFs;

        struct {
            uint16_t Size;
            char Data[0];
        } Xattr;

        struct flock Flock;

        struct {
            int Result;
            uint16_t Size;
            char Buffer[0];
        } Ioctl;

        struct {
            int Result;
            size_t Size;
            char SharedMemoryName[MAX_SHM_PATH_NAME+1];
        } LargeIoctl;

        struct {
            unsigned Revents;
        } Poll;

        struct {
            off_t Offset;
        } Lseek;

        struct {
            uint64_t Version;

        } Test;
    } Response;
} finesse_fuse_response;

// This is the base fuse message type
typedef struct _finesse_fuse_msg {
    uint64_t                Version;
    FINESSE_FUSE_MSG_TYPE   MessageType;
    union {
        finesse_fuse_request  Request;
        finesse_fuse_response Response;
    } Message;
} finesse_fuse_msg;

// Make sure this all fits.
_Static_assert(sizeof(finesse_fuse_msg) <= (sizeof(fincomm_message_block)-offsetof(fincomm_message_block,Data)), "finesse_fuse_msg is too big to fit");

#define FINESSE_FUSE_VERSION (0xbeefbeefbeefbeef)


//
// This is the shared memory protocol:
//   (1) client allocates a request region (FinesseGetRequestBuffer)
//   (2) client sets up the request (message->Data)
//   (3) client asks for server notification (FinesseRequestReady)
//   (4) server retrieves message (FinesseGetReadyRequest)
//   (5) server constructs response in-place
//   (6) server notifies client (FinesseResponseReady)
//   (7) client can poll or block for response (FinesseGetResponse)
//   (8) client frees the request region (FinesseReleaseRequestBuffer)
//
// The goal is, as much as possible, to avoid synchronization. While I'm using condition variables
// now, I was thinking it might be better to use the IPC channel for sending messages, but
// I'm not going to address that today.
//
fincomm_message FinesseGetRequestBuffer(fincomm_shared_memory_region *RequestRegion);
u_int64_t FinesseRequestReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message);
void FinesseResponseReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, uint32_t Response);
int FinesseGetResponse(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, int wait);
int FinesseGetReadyRequest(fincomm_shared_memory_region *RequestRegion, fincomm_message *message);
int FinesseReadyRequestWait(fincomm_shared_memory_region *RequestRegion);
void FinesseReleaseRequestBuffer(fincomm_shared_memory_region *RequestRegion, fincomm_message Message);
int FinesseInitializeMemoryRegion(fincomm_shared_memory_region *Fsmr);
int FinesseDestroyMemoryRegion(fincomm_shared_memory_region *Fsmr);

#endif // __FINESSE_FINCOMM_H__

