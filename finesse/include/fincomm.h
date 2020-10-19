//
// (C) Copyright 2020 Tony Mason
// All Rights Reserved
//
#pragma once

#if !defined(__FINESSE_FINCOMM_H__)
#define __FINESSE_FINCOMM_H__ (1)

#include <aio.h>
#include <assert.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <uuid/uuid.h>
#if !defined(FUSE_USE_VERSION)
#define FUSE_USE_VERSION FUSE_VERSION
#endif
#include <fuse_lowlevel.h>

#define FINESSE_SERVICE_PREFIX "/tmp/finesse"

#define OPTIMAL_ALIGNMENT_SIZE (64)  // this should be whatever the best choice is for laying out cache line optimal data structures
#define MAX_SHM_PATH_NAME \
    (128)  // secondary shared memory regions must fit within a buffer of this size (including a null terminator)
#define SHM_MESSAGE_COUNT (64)  // this is the maximum number of parallel/simultaneous messages per client.
#define SHM_PAGE_SIZE (4096)    // this should be the page size of the underlying machine.

//
// The registration structure is how the client connects to the server
//
typedef struct {
    uuid_t    ClientId;
    u_int32_t ClientSharedMemPathNameLength;
    char      ClientSharedMemPathName[MAX_SHM_PATH_NAME];
    uuid_t    ClientArenaId;
    u_int32_t ClientArenaPathNameLength;
    char      ClientArenaPathName[MAX_SHM_PATH_NAME];
} fincomm_registration_info;

typedef struct {
    uuid_t    ServerId;
    size_t    ClientSharedMemSize;
    u_int32_t Result;
} fincomm_registration_confirmation;

typedef enum _FINESSE_MESSAGE_TYPE {
    FINESSE_REQUEST = 241,
    FINESSE_RESPONSE,
} FINESSE_MESSAGE_TYPE;

typedef enum _FINESSE_MESSAGE_CLASS {
    FINESSE_FUSE_MESSAGE = 251,
    FINESSE_NATIVE_MESSAGE,
} FINESSE_MESSAGE_CLASS;

//
// Each shared memory region consists of a set of communications blocks
//
typedef struct _fincomm_message_block {
    u_int64_t            RequestId;
    u_int32_t            Result;
    FINESSE_MESSAGE_TYPE MessageType;
    u_int8_t             Data[SHM_PAGE_SIZE - 16];
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
    char            Signature[8];
    uuid_t          ClientId;
    uuid_t          ServerId;
    u_int64_t       RequestBitmap;
    u_int64_t       RequestWaiters;
    pthread_mutex_t RequestMutex;
    pthread_cond_t  RequestPending;
    u_int8_t        align0[192 - ((2 * sizeof(uuid_t)) + 8 * sizeof(char) + (2 * sizeof(u_int64_t)) + sizeof(pthread_mutex_t) +
                           sizeof(pthread_cond_t))];
    u_int64_t       ResponseBitmap;
    pthread_mutex_t ResponseMutex;
    pthread_cond_t  ResponsePending;
    u_int8_t        align1[128 - (sizeof(u_int64_t) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t))];
    unsigned        LastBufferAllocated;  // allocation hint
    u_int64_t       AllocationBitmap;
    u_int64_t       RequestId;
    u_int64_t       ShutdownRequested;
    u_int8_t        align2[64 - (4 * sizeof(u_int64_t))];
    u_int8_t        UnusedRegion[4096 - (6 * 64)];
    fincomm_message_block Messages[SHM_MESSAGE_COUNT];
} fincomm_shared_memory_region;

extern const char FinesseSharedMemoryRegionSignature[8];

#define CHECK_SHM_SIGNATURE(fsmr) \
    assert(0 == memcmp(fsmr, FinesseSharedMemoryRegionSignature, sizeof(FinesseSharedMemoryRegionSignature)))

_Static_assert(0 == sizeof(fincomm_shared_memory_region) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, ResponseBitmap) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, LastBufferAllocated) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, UnusedRegion) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, Messages) % SHM_PAGE_SIZE, "Alignment wrong");
_Static_assert(0 == sizeof(fincomm_shared_memory_region) % SHM_PAGE_SIZE, "Length Wrong");

int GenerateServerName(const char *MountPath, char *ServerName, size_t ServerNameLength);
int GenerateClientSharedMemoryName(char *SharedMemoryName, size_t SharedMemoryNameLength, uuid_t ClientId);

// see buffer.c
struct _fincomm_arena_handle;
typedef struct _fincomm_arena_handle *fincomm_arena_handle_t;

fincomm_arena_handle_t FincommCreateArena(char *Name, size_t BufferSize, size_t Count);
void *                 FincommAllocateBuffer(fincomm_arena_handle_t ArenaHandle);
void                   FincommFreeBuffer(fincomm_arena_handle_t ArenaHandle, void *Buffer);
void  FincommGetArenaInfo(fincomm_arena_handle_t Handle, char *Name, size_t NameSize, size_t *BufferSize, size_t *Count);
off_t FincommGetBufferOffset(fincomm_arena_handle_t Handle, void *Buffer);

typedef struct _client_connection_state {
    fincomm_registration_info reg_info;
    int                       server_connection;
    struct sockaddr_un        server_sockaddr;
    int                       server_shm_fd;
    size_t                    server_shm_size;
    void *                    server_shm;
    fincomm_arena_handle_t    arena;
} client_connection_state_t;

typedef struct server_connection_state {
    fincomm_registration_info reg_info;
    int                       client_connection;
    int                       client_shm_fd;
    size_t                    client_shm_size;
    void *                    client_shm;
    pthread_t                 monitor_thread;
    uint8_t                   monitor_thread_active;
    struct {
        uuid_t  AuxShmKey;                      // use UUIDs for the shared memory region
        int     AuxShmFd;                       // Open instance
        uint8_t AuxInUse;                       // indicates if this is currently in use
        void *  AuxShmMap;                      // location in server memory
        size_t  AuxShmSize;                     // size of the shared memory region
        char    AuxShmName[MAX_SHM_PATH_NAME];  // name for shared memory region
    } aux_shm_table[SHM_MESSAGE_COUNT];         // could need one per message
} server_connection_state_t;

// This declares the operations that correspond to various message types
typedef enum _FINESSE_FUSE_REQ_TYPE {
    FINESSE_FUSE_REQ_LOOKUP = 42,
    FINESSE_FUSE_REQ_FORGET,
    FINESSE_FUSE_REQ_STAT,
    FINESSE_FUSE_REQ_GETATTR,
    FINESSE_FUSE_REQ_SETATTR,
    FINESSE_FUSE_REQ_READLINK,
    FINESSE_FUSE_REQ_MKNOD,
    FINESSE_FUSE_REQ_MKDIR,
    FINESSE_FUSE_REQ_UNLINK,
    FINESSE_FUSE_REQ_RMDIR,
    FINESSE_FUSE_REQ_SYMLINK,
    FINESSE_FUSE_REQ_RENAME,
    FINESSE_FUSE_REQ_LINK,
    FINESSE_FUSE_REQ_OPEN,
    FINESSE_FUSE_REQ_READ,
    FINESSE_FUSE_REQ_WRITE,
    FINESSE_FUSE_REQ_FLUSH,
    FINESSE_FUSE_REQ_RELEASE,
    FINESSE_FUSE_REQ_FSYNC,
    FINESSE_FUSE_REQ_OPENDIR,
    FINESSE_FUSE_REQ_READDIR,
    FINESSE_FUSE_REQ_RELEASEDIR,
    FINESSE_FUSE_REQ_FSYNCDIR,
    FINESSE_FUSE_REQ_STATFS,
    FINESSE_FUSE_REQ_SETXATTR,
    FINESSE_FUSE_REQ_GETXATTR,
    FINESSE_FUSE_REQ_LISTXATTR,
    FINESSE_FUSE_REQ_REMOVEXATTR,
    FINESSE_FUSE_REQ_ACCESS,
    FINESSE_FUSE_REQ_CREATE,
    FINESSE_FUSE_REQ_GETLK,
    FINESSE_FUSE_REQ_SETLK,
    FINESSE_FUSE_REQ_BMAP,
    FINESSE_FUSE_REQ_IOCTL,
    FINESSE_FUSE_REQ_POLL,
    FINESSE_FUSE_REQ_WRITE_BUF,
    FINESSE_FUSE_REQ_RETRIEVE_REPLY,
    FINESSE_FUSE_REQ_FORGET_MULTI,
    FINESSE_FUSE_REQ_FLOCK,
    FINESSE_FUSE_REQ_FALLOCATE,
    FINESSE_FUSE_REQ_READDIRPLUS,
    FINESSE_FUSE_REQ_COPY_FILE_RANGE,
    FINESSE_FUSE_REQ_LSEEK,
    FINESSE_FUSE_REQ_MAX
} FINESSE_FUSE_REQ_TYPE;

typedef enum _FINESSE_FUSE_RSP_TYPE {
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
    FINESSE_FUSE_RSP_MAX
} FINESSE_FUSE_RSP_TYPE;

typedef enum {
    FINESSE_NATIVE_REQ_TEST = 1024,
    FINESSE_NATIVE_REQ_SERVER_STAT,
    FINESSE_NATIVE_REQ_MAP,
    FINESSE_NATIVE_REQ_MAP_RELEASE,
    FINESSE_NATIVE_REQ_DIRMAP,
    FINESSE_NATIVE_REQ_DIRMAPRELEASE,
    FINESSE_NATIVE_REQ_MAX,
} FINESSE_NATIVE_REQ_TYPE;

typedef enum {
    FINESSE_NATIVE_RSP_ERR = 1152,
    FINESSE_NATIVE_RSP_TEST,
    FINESSE_NATIVE_RSP_SERVER_STAT,
    FINESSE_NATIVE_RSP_MAP,
    FINESSE_NATIVE_RSP_MAP_RELEASE,
    FINESSE_NATIVE_RSP_DIRMAP,
    FINESSE_NATIVE_RSP_DIRMAPRELEASE,
    FINESSE_NATIVE_RSP_MAX
} FINESSE_NATIVE_RSP_TYPE;

// Information about the Finesse Server
typedef struct _FinesseServerStat {
    uint16_t Version;
    uint16_t Length;
    uint16_t ClientConnectionCount;
    uint16_t Unused0;
    uint32_t ActiveNameMaps;
    uint32_t ErrorCount;
    uint64_t FuseRequests[FINESSE_FUSE_REQ_MAX - FINESSE_FUSE_REQ_LOOKUP];
    uint64_t FuseResponses[FINESSE_FUSE_RSP_MAX - FINESSE_FUSE_RSP_NONE];
    uint64_t NativeRequests[FINESSE_NATIVE_REQ_MAX - FINESSE_NATIVE_REQ_TEST];
    uint64_t NativeResponses[FINESSE_NATIVE_RSP_MAX - FINESSE_NATIVE_RSP_ERR];
    uint64_t NativeResponseCount;
} FinesseServerStat;

#define FINESSE_SERVER_STAT_VERSION (1)
#define FINESSE_SERVER_STAT_LENGTH (sizeof(FinesseServerStat))

typedef struct {
    FINESSE_FUSE_REQ_TYPE Type;  // Message type
    union {
        struct {
            uuid_t Parent;
            char   Name[1];
        } Lookup;

        struct {
            uuid_t   Inode;
            uint64_t Nlookup;
        } Forget;

        struct {
            uuid_t ParentInode;
            uuid_t Inode;
            int    Flags;
            char   Name[1];
        } Stat;

        struct {
            enum {
                GETATTR_STAT = 301,
                GETATTR_FSTAT,
                GETATTR_LSTAT,
            } StatType;
            union {
                uuid_t Inode;
                struct {
                    uuid_t Parent;
                    char   Name[1];
                } Path;
            } Options;
        } GetAttr;

        struct {
            uuid_t      Inode;
            struct stat Attr;
            int         ToSet;
        } SetAttr;

        struct {
            uuid_t Inode;
        } ReadLink;

        struct {
            uuid_t Parent;
            mode_t Mode;
            dev_t  Dev;
            char   Name[1];
        } Mknod;

        struct {
            uuid_t Parent;
            mode_t mode;
            char   Name[1];
        } Mkdir;

        struct {
            uuid_t Parent;
            char   Name[1];
        } Unlink;

        struct {
            uuid_t Parent;
            char   Name[1];
        } Rmdir;

        struct {
            uuid_t Symlink;
            // Pair of null terminated strings
            char LinkAndName[1];
        } Symlink;

        struct {
            uuid_t       Parent;
            uuid_t       NewParent;
            unsigned int flags;
            // Pair of null terminated strings
            char OldAndNewName[1];
        } Rename;

        struct {
            uuid_t Inode;
            uuid_t NewParent;
            char   NewName[1];
        } Link;

        struct {
            uuid_t Inode;
            int    Flags;
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
            char   SharedMemoryName[MAX_SHM_PATH_NAME + 1];
        } LargeRead;

        struct {
            uuid_t   Inode;
            uint16_t Size;
            off_t    Offset;
            char     Buffer[1];
        } SmallWrite;

        struct {
            uuid_t   Inode;
            uint64_t Size;
            off_t    Offset;
            char     SharedMemoryName[MAX_SHM_PATH_NAME + 1];
        } LargeWrite;

        struct {
            uuid_t Inode;
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
            uuid_t   Inode;
            uint16_t Size;
            off_t    Offset;
        } Readdir;

        struct {
            uuid_t Inode;
        } Releasedir;

        struct {
            uuid_t Inode;
            int    DataSync;
        } Fsyncdir;

        struct {
            enum {
                STATFS  = 31,  // path name
                FSTATFS = 32   // inode number
            } StatFsType;
            union {
                uuid_t Inode;
                char   Name[1];
            } Options;
        } Statfs;

        struct {
            uuid_t   Inode;
            uint16_t Size;
            int      Flags;
            // two null-terminated strings
            char NameAndValue[1];
        } Setxattr;

        struct {
            uuid_t Inode;
            char   Name;
            size_t MaxSize;
        } Getxattr;

        struct {
            uuid_t Inode;
            size_t MaxSize;
        } Listxattr;

        struct {
            uuid_t Inode;
            char   Name[1];
        } Removexattr;

        struct {
            uuid_t ParentInode;
            char   Name[1];
            int    Mask;
        } Access;

        struct {
            uuid_t      Parent;
            struct stat Attr;
            char        Name[1];
        } Create;

        struct {
            uuid_t Inode;
        } GetLk;

        struct {
            uuid_t       Inode;
            struct flock Lock;
            int          Sleep;
        } SetLk;

        struct {
            uuid_t Inode;
            size_t BlockSize;
        } Bmap;

        struct {
            uuid_t   Inode;
            int      Command;
            unsigned Flags;
            uint16_t InputSize;
            uint16_t OutputSize;
            char     InputBuffer[1];
        } SmallIoctl;

        struct {
            uuid_t   Inode;
            int      Command;
            unsigned Flags;
            size_t   InputSize;
            size_t   OutputSize;
            char     InputSharedMemoryName[MAX_SHM_PATH_NAME + 1];
            char     OutputSharedMemoryName[MAX_SHM_PATH_NAME + 1];
        } LargeIoctl;

        struct {
            uuid_t Inode;
        } Poll;

        struct {
            uuid_t Inode;
        } WriteBuf;

        struct {
            uuid_t    Inode;
            uintptr_t Cookie;
            off_t     Offset;
        } RetrieveReply;

        struct {
            uint16_t Count;
            uuid_t   Inodes[250];
        } ForgetMulti;

        struct {
            uuid_t Inode;
            int    Operation;
        } Flock;

        struct {
            uuid_t Inode;
            int    Mode;
            off_t  Offset;
            off_t  Length;
        } Fallocate;

        struct {
            uuid_t   Inode;
            uint16_t Size;
            off_t    Offset;
        } SmallReaddirplus;

        struct {
            uuid_t   Inode;
            uint16_t Size;
            off_t    Offset;
            char     SharedMemoryName[MAX_SHM_PATH_NAME + 1];
        } LargeReaddirplus;

        struct {
            uuid_t InputInode;
            off_t  InputOffset;
            uuid_t OutputInode;
            off_t  OutputOffset;
            size_t Length;
            int    Flags;
        } CopyFileRange;

        struct {
            uuid_t Inode;
            off_t  Offset;
            int    Whence;
        } Lseek;

    } Parameters;
} finesse_fuse_request;

typedef struct {
    FINESSE_FUSE_RSP_TYPE Type;
    union {
        struct {
            struct fuse_entry_param EntryParam;
        } EntryParam;

        struct {
            // derived from the fuse_entry_param structure
            uuid_t      Key;
            uint64_t    Generation;  // unique value for this file (NFS support)
            struct stat Attr;
            double      Timeout;
        } Create;

        struct {
            struct stat Attr;
            double      AttrTimeout;
        } Attr;

        struct {
            char Link[1];
        } ReadLink;

        struct {
            struct fuse_file_info FileInfo;
        } Open;

        struct {
            size_t Count;
        } Write;

        struct {
            uint16_t Size;
            char     Buffer[1];
        } SmallBuffer;

        struct {
            // Use this when what's being returned
            // won't fit.
            size_t Size;
            char   SharedMemoryName[MAX_SHM_PATH_NAME + 1];
        } LargeBuffer;

        struct {
            struct statvfs StatBuffer;
        } StatFs;

        struct {
            uint16_t Size;
            char     Data[1];
        } Xattr;

        struct flock Flock;

        struct {
            int      Result;
            uint16_t Size;
            char     Buffer[1];
        } Ioctl;

        struct {
            int    Result;
            size_t Size;
            char   SharedMemoryName[MAX_SHM_PATH_NAME + 1];
        } LargeIoctl;

        struct {
            unsigned Revents;
        } Poll;

        struct {
            off_t Offset;
        } Lseek;
    } Parameters;
} finesse_fuse_response;

typedef struct {
    FINESSE_NATIVE_REQ_TYPE NativeRequestType;

    union {
        struct {
            uuid_t Parent;  // if NULL , Name is absolute
            char   Name[1];
        } Map;

        struct {
            uuid_t Key;
        } MapRelease;

        struct {
            uint64_t Version;
        } Test;

        struct {
            enum {
                HARDLINK = 701,
                HARDLINKAT,
                SYMLINK,
                SYMLINKAT,
            } LinkType;
            union {
                struct {
                    uuid_t Parent1;
                    uuid_t Parent2;
                    char   Paths[1];  // two null-terminated paths.
                } Relative;
                struct {
                    char Paths[1];  // two null-terminated paths.
                } Absolute;
            } LinkData;
        } MakeLink;

        struct {
            uuid_t Parent;
            char   Name[1];
        } Dirmap;
    } Parameters;
} finesse_native_request;

typedef struct {
    FINESSE_NATIVE_RSP_TYPE NativeResponseType;
    union {
        struct {
            uuid_t  MapId;    // for tracking this specific mapping
            size_t  Length;   // Length of the data
            uint8_t Inline;   // boolean if the data is returned rather than a name
            char    Data[1];  // either inline data or the shared memory name
        } DirMap;

        struct {
            int    Result;
            uuid_t Key;
        } Map;

        struct {
            int Result;
        } MapRelease;

        struct {
            int               Result;
            FinesseServerStat Data;
        } ServerStat;
        struct {
            uint64_t Version;
        } Test;

    } Parameters;
} finesse_native_response;

// Each shared memory block indicates if the block is being used for
// a request or a response.  Each block then contains a message
// (the structure following this block).  That indicates what class
// of message this is (current "native" or "fuse").  Each class
// then has a request or response block, and each of those is just
// a union of different types of messages, identified by the first
// field in the respective request/response structure.
typedef struct _finesse_message {
    uint64_t              Version;
    FINESSE_MESSAGE_CLASS MessageClass;
    int                   Result;  // this is the result of the request; requester should set this to non-zero
    union {
        union {
            finesse_fuse_request  Request;
            finesse_fuse_response Response;
        } Fuse;
        union {
            finesse_native_request  Request;
            finesse_native_response Response;
        } Native;
    } Message;
} finesse_msg;

// Make sure this all fits.
_Static_assert(sizeof(finesse_msg) <= (sizeof(fincomm_message_block) - offsetof(fincomm_message_block, Data)),
               "finesse_msg is too big to fit");

#define FINESSE_MESSAGE_VERSION (0xbeefbeefbeefbeef)

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
u_int64_t       FinesseRequestReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message);
void            FinesseResponseReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, uint32_t Response);
int             FinesseGetResponse(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, int wait);
int             FinesseGetReadyRequest(fincomm_shared_memory_region *RequestRegion, fincomm_message *message);
int             FinesseReadyRequestWait(fincomm_shared_memory_region *RequestRegion);
int             FinesseInitializeMemoryRegion(fincomm_shared_memory_region *Fsmr);
int             FinesseDestroyMemoryRegion(fincomm_shared_memory_region *Fsmr);

#endif  // __FINESSE_FINCOMM_H__
