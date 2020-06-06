//
// (C) Copyright 2020 Tony Mason (fsgeek@cs.ubc.ca)
// All Rights Reserved
//

#include "bitbucket.h"
#include <stdlib.h>
#include <string.h>

//
// Standard data structure issue: how do we know when we're done using something?
//
typedef struct _bitbucket_object_header {
    uint64_t                        Magic; // object magic number.
    bitbucket_object_attributes_t   ObjectAttributes;
    uint64_t                        ReferenceCount;
    size_t                          DataLength;
    // char                            Unused[0]; // used to pad so Data starts on a 64 byte boundary
    uint64_t                        Data[0];
} bitbucket_object_header_t;

_Static_assert(0 == (offsetof(bitbucket_object_header_t, Data) % 64), "Bad alignment");

#define BITBUCKET_OBJECT_HEADER_MAGIC (0xc24e22f696a3861d)
#define CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(bboh) verify_magic("bitbucket_object_header_t", __FILE__, __PRETTY_FUNCTION__, __LINE__, BITBUCKET_OBJECT_HEADER_MAGIC, (bboh)->Magic)

// Note: this is global scope; if this proves to be an issue, we can create a group of locks.
static pthread_rwlock_t DefaultObjectLock = PTHREAD_RWLOCK_INITIALIZER;
static uint64_t ObjectCount = 0;

static inline uint64_t IncrementObjectCount(void)
{
    return __atomic_fetch_add(&ObjectCount, 1, __ATOMIC_RELAXED);
}

static inline uint64_t DecrementObjectCount(void)
{
    return __atomic_fetch_sub(&ObjectCount, 1, __ATOMIC_RELAXED);
}


static void default_initialize(void *Object, size_t Length)
{
    // no initalization
    assert(NULL != Object);
    (void) Length;
    return;
}

static void default_deallocate(void *Object, size_t Length)
{
    // no deinit
    assert(NULL != Object);
    (void) Length;
    return;
}

static void default_lock(void *Object, int Exclusive)
{
    bitbucket_object_header_t *obj = container_of(Object, bitbucket_object_header_t, Data);

    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(obj);

    if (Exclusive) {
        pthread_rwlock_wrlock(&DefaultObjectLock);
    }
    else {
        pthread_rwlock_rdlock(&DefaultObjectLock);
    }
}

static void default_unlock(void *Object)
{
    bitbucket_object_header_t *obj = container_of(Object, bitbucket_object_header_t, Data);

    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(obj);

    pthread_rwlock_unlock(&DefaultObjectLock);
}

static void LockObject(bitbucket_object_header_t *Object, int Exclusive)
{
    assert(NULL != Object);
    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(Object);

    if (NULL == Object->ObjectAttributes.Lock) {
        default_lock(Object, Exclusive);
    }
    else {
        Object->ObjectAttributes.Lock(Object->Data, Exclusive);
    }
}

static void UnlockObject(bitbucket_object_header_t *Object)
{
    assert(NULL != Object);
    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(Object);

    if ((NULL == Object) || (NULL == Object->ObjectAttributes.Lock)) {
        default_unlock(Object);
    }
    else {
        Object->ObjectAttributes.Unlock(Object->Data);
    }

}

static bitbucket_object_attributes_t default_object_attributes = {
    .Magic = BITBUCKET_OBJECT_ATTRIBUTES_MAGIC,
    .Initialize = default_initialize,
    .Deallocate = default_deallocate,
    .Lock = default_lock,
    .Unlock = default_unlock,
};


void *BitbucketObjectCreate(bitbucket_object_attributes_t *ObjectAttributes, size_t ObjectSize)
{
    size_t length = sizeof(bitbucket_object_header_t) + ObjectSize;
    bitbucket_object_header_t *newobj = NULL;
    void *data;

    if (NULL == ObjectAttributes) {
        ObjectAttributes = &default_object_attributes;
    }

    CHECK_BITBUCKET_OBJECT_ATTRIBUTES_MAGIC(ObjectAttributes);

    length = (length + 0x3F) & ~0x3F; // round up to the next 64 byte boundary
    newobj = (bitbucket_object_header_t *)malloc(length);

    memset(newobj, 0, length);
    newobj->Magic = BITBUCKET_OBJECT_HEADER_MAGIC;
    newobj->ObjectAttributes = *ObjectAttributes;
    newobj->ReferenceCount = 1;
    newobj->DataLength = ObjectSize;

    newobj->ObjectAttributes.Initialize(newobj->Data, ObjectSize);

    IncrementObjectCount();
    // return space aligned to the nearest cache line (assuming the cache line is on 64 byte boundaries)
    data = newobj->Data;
    assert(container_of(data, bitbucket_object_header_t, Data) == newobj);
    return (void *)newobj->Data;
}

void BitbucketObjectReference(void *Object)
{
    bitbucket_object_header_t *bbobj = container_of(Object, bitbucket_object_header_t, Data);
    uint64_t refcount;

    // If the caller really doesn't own a reference, this will (at some point) blow up.
    LockObject(bbobj, 0);
    refcount = __atomic_fetch_add(&bbobj->ReferenceCount, 1, __ATOMIC_RELAXED);
    UnlockObject(bbobj);

    assert(0 != refcount); // if we ever see the 0->1 transition, we've found a bug as this isn't supported
}

void BitbucketObjectDereference(void *Object) 
{
    bitbucket_object_header_t *bbobj = container_of(Object, bitbucket_object_header_t, Data);
    uint64_t refcount;
    int local_unlock = 0;

    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(bbobj);

    LockObject(bbobj, 0);
    refcount = __atomic_fetch_sub(&bbobj->ReferenceCount, 1, __ATOMIC_RELAXED);
    if (1 == refcount) {
        // Unsafe to delete since we don't hold the exclusive lock
        __atomic_fetch_add(&bbobj->ReferenceCount, 1, __ATOMIC_RELAXED);
    }
    UnlockObject(bbobj);
    
    if (1 == refcount) {
        // Since this was an attempt to delete the object, we must acquire the exclusive lock
        // and try this again.
        LockObject(bbobj, 1);
        refcount = __atomic_fetch_sub(&bbobj->ReferenceCount, 1, __ATOMIC_RELAXED);
        assert(0 != refcount); // this means the ref count went negative, which is a bad thing.
        if (1 == refcount) {
            // Now it is safe for us to delete this
            if (NULL == bbobj->ObjectAttributes.Lock) {
                local_unlock = 1; // we have to release OUR lock
            }
            bbobj->ObjectAttributes.Deallocate(Object, bbobj->DataLength); // This should remove all external usage of this object
            if (local_unlock) {
                pthread_rwlock_unlock(&DefaultObjectLock);
                local_unlock = 0;
            }
            free(bbobj);
            bbobj = NULL;
            DecrementObjectCount();
        }
    }

}

uint64_t BitbucketObjectCount(void)
{
    return __atomic_load_n(&ObjectCount, __ATOMIC_RELAXED);
}

uint64_t BitbucketGetObjectReferenceCount(void *Object)
{
    bitbucket_object_header_t *bbobj;
    
    assert(NULL != Object);
    bbobj = container_of(Object, bitbucket_object_header_t, Data);
    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(bbobj);

    return bbobj->ReferenceCount;
}