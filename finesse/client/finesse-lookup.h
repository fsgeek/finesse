/*
 * Copyright (c) 2017 Tony Mason
 * All rights reserved.
 */

#if !defined(__FINESSE_LOOKUP_H__)
#define __FINESSE_LOOKUP_H__ (1)

#if !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS (64)
#endif // _FILE_OFFSET_BITS

#include <fuse_lowlevel.h>
#include <uuid/uuid.h>
#include <stdint.h>

//
// Niccolum must be able to look up by inode number and UUID (the two sources of handles)
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

#endif // __FINESSE_LOOKUP_H__

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
