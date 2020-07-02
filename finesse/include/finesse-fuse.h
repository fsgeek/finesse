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

typedef struct _finesse_object_table finesse_object_table_t;

// These are the replacement APIs for the previous lookup package
// They generalize the table (no requirement for a single global table)
// and the internal implementation changes to improve parallel behavior.
finesse_object_t *FinesseObjectLookupByIno(finesse_object_table_t *Table, fuse_ino_t InodeNumber);
finesse_object_t *FinesseObjectLookupByUuid(finesse_object_table_t *Table, uuid_t *Uuid);
void FinesseObjectRelease(finesse_object_table_t *Table, finesse_object_t *Object);
finesse_object_t *FinesseObjectCreate(finesse_object_table_t *Table, fuse_ino_t InodeNumber, uuid_t *Uuid);
uint64_t FinesseObjectGetTableSize(finesse_object_table_t *Table);
void FinesseInitializeTable(finesse_object_table_t *Table);
void FinesseDestroyTable(finesse_object_table_t *Table);
finesse_object_table_t *FinesseCreateTable(uint64_t EstimatedSize);

finesse_object_t *finesse_object_lookup_by_ino(fuse_ino_t inode);
finesse_object_t *finesse_object_lookup_by_uuid(uuid_t *uuid);
void finesse_object_release(finesse_object_t *object);
finesse_object_t *finesse_object_create(fuse_ino_t inode, uuid_t *uuid);
uint64_t finesse_object_get_table_size(void);

// If passed a NULL uuid (uuid_is_null() returns non-zero) then it return FUSE_ROOT_ID
// If the UUID passed is in the lookup table, it returns the inode number
// If the UUID passed is not in the table, it returns (fuse_ino_t) 0
// Note that no reference is created by this call.
fuse_ino_t finesse_lookup_ino(uuid_t *uuid);

extern int finesse_send_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count, int free_req);
extern void finesse_notify_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count);

#endif // __FINESSE_FUSE_H__
