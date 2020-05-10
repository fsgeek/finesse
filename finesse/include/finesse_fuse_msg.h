/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
 */

#pragma once

#include <stdint.h>

#if !defined(__FINESSE_FUSE_REQ_H__)
#define __FINESSE_FUSE_REQ_H__

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
    FINESSE_FUSE_REQ_TYPE Type;
    union {
        struct {

        } Lookup;
        struct {
            
        } Forget;
        struct {
            
        } GetAttr;
        struct {

        } SetAttr;
        struct {

        } ReadLink;

        struct {

        } Mknod;

        struct {

        } Mkdir;

        struct {

        } Unlink;

        struct {

        } Rmdir;

        struct {

        } Symlink;

        struct {

        } Rename;

        struct {

        } Link;

        struct {

        } Open;

        struct {

        } Read;

        struct {

        } Write;

        struct {

        } Flush;

        struct {

        } Release;

        struct {

        } Fsync;

        struct {

        } Opendir;

        struct {

        } Readdir;

        struct {

        } Releasedir;

        struct {

        } Fsyncdir;

        struct {

        } Statfs;

        struct {

        } Setxattr;

        struct {

        } Getxattr;

        struct {

        } Listxattr;

        struct {

        } Removexattr;

        struct {

        } Access;

        struct {

        } Create;

        struct {

        } GetLk;

        struct {

        } SetLk;

        struct {

        } Bmap;

        struct {

        } Ioctl;

        struct {

        } Poll;
        
        struct {

        } WriteBuf;

        struct {
            
        } RetrieveReply;

        struct {

        } ForgetMulti;

        struct {

        } Flock;

        struct {
            
        } Fallocate;

        struct {

        } Readdirplus;

        struct {

        } CopyFileRange;

        struct {

        } Lseek;

        struct {
            
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
            uint64_t Version;
        } Test;
    } Response;
} finesse_fuse_response;

typedef enum {
    FINESSE_FUSE_MSG_REQUEST  = 241,
    FINESSE_FUSE_MSG_RESPONSE = 242,
} FINESSE_FUSE_MSG_TYPE;

// This is the base fuse message type
typedef struct _finesse_fuse_msg {
    uint64_t                Version;
    FINESSE_FUSE_MSG_TYPE   MessageType;
    uint8_t                 UsesAuxilliaryMap;   // boolean
    uint64_t                AuxilliaryMapLength; // size_t
    uint32_t                MessageLength;
    union {
        finesse_fuse_request  Request;
        finesse_fuse_response Response;
    } Message;
} finesse_fuse_msg;

#define FINESSE_FUSE_VERSION (0xbeefbeefbeefbeef)

#endif // __FINESSE_FUSE_REQ_H__