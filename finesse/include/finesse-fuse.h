#ifndef __FINESSE_FUSE_H__
#define __FINESSE_FUSE_H__

#include "finesse.h"

//
// Finesse must be able to look up by inode number and UUID (the two sources of handles)
// see lookup.c
//
typedef struct _finesse_object
{
    fuse_ino_t inode;
    uuid_t uuid;
    // TODO: we may need additional data here
} finesse_object_t;

finesse_object_t *finesse_object_lookup_by_ino(fuse_ino_t inode);
finesse_object_t *finesse_object_lookup_by_uuid(uuid_t *uuid);
void finesse_object_release(finesse_object_t *object);
finesse_object_t *finesse_object_create(fuse_ino_t inode, uuid_t *uuid);
uint64_t finesse_object_get_table_size(void);


extern int finesse_send_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count, int free_req);
extern void finesse_notify_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count);

#endif // __FINESSE_FUSE_H__
