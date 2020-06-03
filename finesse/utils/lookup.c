/*
 * Copyright (c) 2017 Tony Mason
 * All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fuse_lowlevel.h>
#include "finesse-list.h"
#include <finesse-fuse.h>
#include "../api/api-internal.h"

#if !defined(offset_of)
#define offset_of(type, field) (unsigned long)&(((type *)0)->field)
#endif // offset_of

#if !defined(container_of)
#define container_of(ptr, type, member) ((type *)(((char *)ptr) - offset_of(type, member)))
#endif // container_of

//
// The current implementation is a simple linked list.  If this isn't sufficiently efficient,
// the interface is general and it can be moved to a more efficient structure.
//
// A reader/writer lock is used to protect.  The read lock is used to protect against insertion/deletion
// and the write lock is used to allow insertion/deletion.
//
// Each object has a reference count; when the reference count goes to zero, it can be removed from
// the list IFF the list lock is held exclusive.
//

static void lock_table_for_lookup(void);
static void lock_table_for_change(void);
static void unlock_table(void);

static list_entry_t table_list = {.next = &table_list, .prev = &table_list};
static uint64_t object_count;

static inline void add_object(void)
{
    __atomic_fetch_add(&object_count, 1, __ATOMIC_RELAXED);
}

static inline void remove_object(void)
{
    __atomic_fetch_sub(&object_count, 1, __ATOMIC_RELAXED);
}

typedef struct _finesse_internal_object
{
    list_entry_t list_entry;
    uint32_t refcount;
    finesse_object_t object;
} finesse_internal_object_t;

static finesse_internal_object_t *lookup_by_ino_locked(fuse_ino_t inode)
{
    finesse_internal_object_t *internal_object;
    list_entry_t *entry;
    int found = 0;

    list_for_each(&table_list, entry)
    {
        internal_object = container_of(entry, finesse_internal_object_t, list_entry);
        if (internal_object->object.inode == inode)
        {
            found = 1;
            __atomic_fetch_add(&internal_object->refcount, 1, __ATOMIC_RELAXED);
            break;
        }
    }

    return found ? internal_object : NULL;
}

static finesse_internal_object_t *lookup_by_uuid_locked(uuid_t *uuid)
{
    finesse_internal_object_t *internal_object;
    list_entry_t *entry;
    int found = 0;

    list_for_each(&table_list, entry)
    {
        internal_object = container_of(entry, finesse_internal_object_t, list_entry);
        if (0 == memcmp(&internal_object->object.uuid, uuid, sizeof(uuid_t)))
        {
            found = 1;
            __atomic_fetch_add(&internal_object->refcount, 1, __ATOMIC_RELAXED);
            break;
        }
    }

    return found ? internal_object : NULL;
}

static void release(finesse_internal_object_t *object)
{
    //
    // this is the only place I try to be tricky
    // To delete, the refcount must do the 1->0 transition
    // but since most transitions will be to > 0 I don't want to take the lock
    // exclusive.  So I hold the lock shared (preventing deletion) and decrement
    // and see if this might be a deletion call.  If it _might_ be then I have to
    // bump the refcount and try again with the exclusive lock held.  If it
    //  *still* drops to zero, I can safely remove and delete the object as nobody
    // else can find it.
    //
    // Lookup: hold lock shared, atomic increment
    // Release: hold lock shared, atomic decrement IFF decrement causes deletion, atomic increment
    //          THEN hold lock exclusive, atomic decrement IFF decrement causes deletion, remove and free.
    //
    //
    assert(object->list_entry.next != &object->list_entry);
    lock_table_for_lookup();
    if (1 == __atomic_fetch_sub(&object->refcount, 1, __ATOMIC_RELAXED))
    {
        // this is an actual delete, so we need the lock exclusive
        __atomic_fetch_add(&object->refcount, 1, __ATOMIC_RELAXED);
        unlock_table();
        lock_table_for_change();
        if (1 == __atomic_fetch_sub(&object->refcount, 1, __ATOMIC_RELAXED))
        {
            // now it's safe to remove it
            remove_list_entry(&object->list_entry);
            free(object);
            remove_object();
        }
    }
    unlock_table();
}

static finesse_internal_object_t *object_create(fuse_ino_t inode, uuid_t *uuid)
{
    finesse_internal_object_t *internal_object = (finesse_internal_object_t *)malloc(sizeof(finesse_internal_object_t));
    finesse_internal_object_t *dummy = NULL;

    while (NULL != internal_object)
    {
        initialize_list_entry(&internal_object->list_entry);
        memcpy(internal_object->object.uuid, uuid, sizeof(uuid_t));
        internal_object->object.inode = inode;
        internal_object->refcount = 2;

        lock_table_for_change();
        dummy = lookup_by_ino_locked(inode);
        // assert(NULL == dummy); // shouldn't be one
        if (NULL == dummy)
        {
            insert_list_head(&table_list, &internal_object->list_entry);
            unlock_table();
        }
        else
        {
            unlock_table();
            free(internal_object);
            internal_object = dummy;
            dummy = NULL;
        }
        break;
    }
    assert(NULL == dummy);
    add_object();
    return internal_object;
}

finesse_object_t *finesse_object_create(fuse_ino_t inode, uuid_t *uuid)
{
    uuid_t dummy_uuid;

    if (NULL == uuid)
    {
        uuid_generate(dummy_uuid);
        uuid = &dummy_uuid;
    }

    finesse_internal_object_t *internal_object = object_create(inode, uuid);

    return internal_object ? &internal_object->object : NULL;
}

finesse_object_t *finesse_object_lookup_by_ino(fuse_ino_t inode)
{
    finesse_internal_object_t *internal_object;

    lock_table_for_lookup();
    internal_object = lookup_by_ino_locked(inode);
    unlock_table();
    return internal_object ? &internal_object->object : NULL;
}

finesse_object_t *finesse_object_lookup_by_uuid(uuid_t *uuid)
{
    finesse_internal_object_t *internal_object;

    lock_table_for_lookup();
    internal_object = lookup_by_uuid_locked(uuid);
    unlock_table();
    return internal_object ? &internal_object->object : NULL;
}

fuse_ino_t finesse_lookup_ino(uuid_t *uuid)
{
    fuse_ino_t inode = FUSE_ROOT_ID;
    finesse_internal_object_t *internal_object;
    list_entry_t *entry;

    if (uuid_is_null(*uuid)) {
        return inode;
    }

    inode = (fuse_ino_t) 0;

    lock_table_for_lookup();

    list_for_each(&table_list, entry)
    {
        internal_object = container_of(entry, finesse_internal_object_t, list_entry);
        if (0 == memcmp(&internal_object->object.uuid, uuid, sizeof(uuid_t)))
        {
            inode = internal_object->object.inode;
            break;
        }
    }
    unlock_table();

    return inode;
}


uint64_t finesse_object_get_table_size()
{
    return __atomic_load_n(&object_count, __ATOMIC_RELAXED);
}

void finesse_object_release(finesse_object_t *object)
{
    finesse_internal_object_t *internal_object = container_of(object, finesse_internal_object_t, object);

    release(internal_object);
}

static pthread_rwlock_t table_lock = PTHREAD_RWLOCK_INITIALIZER;

static void lock_table_for_lookup(void)
{
    pthread_rwlock_rdlock(&table_lock);
}

static void lock_table_for_change(void)
{
    pthread_rwlock_wrlock(&table_lock);
}

static void unlock_table(void)
{
    pthread_rwlock_unlock(&table_lock);
}
