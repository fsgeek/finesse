//
// (C) Copyright 2020 Tony Mason (fsgeek@cs.ubc.ca)
// All Rights Reserved
//
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include "bitbucket.h"

//
// Standard data structure issue: how do we know when we're done using something?
//
typedef struct _bitbucket_object_header {
    uint64_t                      Magic;  // object magic number.
    bitbucket_object_attributes_t ObjectAttributes;
    uint64_t                      ReferenceCount;
    uint32_t                      ReferenceReasons[BITBUCKET_MAX_REFERENCE_REASONS];
    size_t                        DataLength;
    char                          Unused[56];  // used to pad so Data starts on a 64 byte boundary
    uint64_t                      Data[1];
} bitbucket_object_header_t;

// const char foo[(64 * 9) - offsetof(bitbucket_object_header_t, Data)];
_Static_assert(0 == (offsetof(bitbucket_object_header_t, Data) % 64), "Bad alignment");

#define BITBUCKET_OBJECT_HEADER_MAGIC (0xc24e22f696a3861d)
#define CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(bboh) \
    verify_magic("bitbucket_object_header_t", __FILE__, __func__, __LINE__, BITBUCKET_OBJECT_HEADER_MAGIC, (bboh)->Magic)

// Note: this is global scope; if this proves to be an issue, we can create a group of locks.
static pthread_rwlock_t DefaultObjectLock = PTHREAD_RWLOCK_INITIALIZER;
static uint64_t         ObjectCount       = 0;

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
    (void)Length;
    return;
}

static void default_deallocate(void *Object, size_t Length)
{
    // no deinit
    assert(NULL != Object);
    (void)Length;
    return;
}

static void default_lock(void *Object, int Exclusive)
{
    int                        status;
    bitbucket_object_header_t *obj = container_of(Object, bitbucket_object_header_t, Data);

    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(obj);

    if (Exclusive) {
        status = pthread_rwlock_wrlock(&DefaultObjectLock);
        assert(0 == status);
    }
    else {
        status = pthread_rwlock_rdlock(&DefaultObjectLock);
        assert(0 == status);
    }
}

static int default_trylock(void *Object, int Exclusive)
{
    int                        status = 0;
    bitbucket_object_header_t *obj    = container_of(Object, bitbucket_object_header_t, Data);

    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(obj);

    if (Exclusive) {
        status = pthread_rwlock_trywrlock(&DefaultObjectLock);
    }
    else {
        status = pthread_rwlock_tryrdlock(&DefaultObjectLock);
    }

    return status;
}

static void default_unlock(void *Object)
{
    bitbucket_object_header_t *obj = container_of(Object, bitbucket_object_header_t, Data);
    int                        status;

    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(obj);

    status = pthread_rwlock_unlock(&DefaultObjectLock);
    assert(0 == status);
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

#if 0
static int TrylockObject(bitbucket_object_header_t *Object, int Exclusive)
{
    int status = 0;

    assert(NULL != Object);
    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(Object);

    if (NULL == Object->ObjectAttributes.Lock) {
        status = default_trylock(Object, Exclusive);
    }
    else {
        status = Object->ObjectAttributes.Trylock(Object->Data, Exclusive);
    }
    return status;
}
#endif  // 0

static void UnlockObject(bitbucket_object_header_t *Object)
{
    assert(NULL != Object);
    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(Object);

    if ((NULL == Object) || (NULL == Object->ObjectAttributes.Unlock)) {
        default_unlock(Object);
    }
    else {
        Object->ObjectAttributes.Unlock(Object->Data);
    }
}

static bitbucket_object_attributes_t default_object_attributes = {
    .Magic       = BITBUCKET_OBJECT_ATTRIBUTES_MAGIC,
    .ReasonCount = BITBUCKET_MAX_REFERENCE_REASONS,
    .ReferenceReasonsNames =
        {
            "ObjectReason0",
            "ObjectReason1",
            "ObjectReason2",
            "ObjectReason3",
            "ObjectReason4",
            "ObjectReason5",
            "ObjectReason6",
            "ObjectReason7",
        },
    .Initialize = default_initialize,
    .Deallocate = default_deallocate,
    .Lock       = default_lock,
    .Trylock    = default_trylock,
    .Unlock     = default_unlock,
};

//
// Create a new object, with the specified attributes.
// Reserve additional space (ObjectSize)
// Indicate reason for initial reference.
//
void *BitbucketObjectCreate(bitbucket_object_attributes_t *ObjectAttributes, size_t ObjectSize, uint8_t InitialReason)
{
    size_t                     length = sizeof(bitbucket_object_header_t) + ObjectSize;
    bitbucket_object_header_t *newobj = NULL;
    void *                     data;

    if (NULL == ObjectAttributes) {
        ObjectAttributes = &default_object_attributes;
    }

    CHECK_BITBUCKET_OBJECT_ATTRIBUTES_MAGIC(ObjectAttributes);

    length = (length + 0x3F) & ~0x3F;  // round up to the next 64 byte boundary
    newobj = (bitbucket_object_header_t *)malloc(length);

    assert(ObjectAttributes->ReasonCount <= BITBUCKET_MAX_REFERENCE_REASONS);
    for (unsigned index = 0; index < ObjectAttributes->ReasonCount; index++) {
        assert(strlen(ObjectAttributes->ReferenceReasonsNames[index]) <= BITBUCKET_MAX_REFERENCE_REASON_NAME_LENGTH);
    }
    assert(InitialReason < ObjectAttributes->ReasonCount);
    memset(newobj, 0, length);
    newobj->Magic                           = BITBUCKET_OBJECT_HEADER_MAGIC;
    newobj->ObjectAttributes                = *ObjectAttributes;
    newobj->ReferenceCount                  = 1;
    newobj->ReferenceReasons[InitialReason] = 1;
    newobj->DataLength                      = ObjectSize;

    newobj->ObjectAttributes.Initialize(newobj->Data, ObjectSize);

    IncrementObjectCount();
    // return space aligned to the nearest cache line (assuming the cache line is on 64 byte boundaries)
    data = newobj->Data;
    assert(container_of(data, bitbucket_object_header_t, Data) == newobj);
    return (void *)newobj->Data;
}

void BitbucketObjectReference(void *Object, uint8_t Reason)
{
    bitbucket_object_header_t *bbobj = container_of(Object, bitbucket_object_header_t, Data);
    uint64_t                   refcount;
    uint64_t                   reasonRefCount;

    assert(Reason < bbobj->ObjectAttributes.ReasonCount);

    // If the caller really doesn't own a reference, this will (at some point) blow up.
    LockObject(bbobj, 0);
    refcount       = __atomic_fetch_add(&bbobj->ReferenceCount, 1, __ATOMIC_RELAXED);
    reasonRefCount = __atomic_fetch_add(&bbobj->ReferenceReasons[Reason], 1, __ATOMIC_RELAXED);
    UnlockObject(bbobj);

    assert(0 != refcount);               // if we ever see the 0->1 transition, we've found a bug as this isn't supported
    assert(reasonRefCount <= refcount);  // weak assert - should add them all up
}

void BitbucketObjectDereference(void *Object, uint8_t Reason, uint64_t Bias)
{
    bitbucket_object_header_t *bbobj = container_of(Object, bitbucket_object_header_t, Data);
    uint64_t                   refcount;
    uint32_t                   reasonRefCount;

    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(bbobj);
    assert(Reason < bbobj->ObjectAttributes.ReasonCount);
    assert(Bias <= bbobj->ReferenceCount);
    assert(Bias <= bbobj->ReferenceReasons[Reason]);

    LockObject(bbobj, 0);
    refcount = __atomic_fetch_sub(&bbobj->ReferenceCount, Bias, __ATOMIC_RELAXED);
    if (Bias == refcount) {
        // Unsafe to delete since we don't hold the exclusive lock
        __atomic_fetch_add(&bbobj->ReferenceCount, Bias, __ATOMIC_RELAXED);
    }
    else {
        reasonRefCount = __atomic_fetch_sub(&bbobj->ReferenceReasons[Reason], Bias, __ATOMIC_RELAXED);
        assert(reasonRefCount >= Bias);  // if not, this is an underflow.
    }
    UnlockObject(bbobj);

    if (Bias == refcount) {
        // Since this was an attempt to delete the object, we must acquire the exclusive lock
        // and try this again.
        LockObject(bbobj, 1);
        refcount = __atomic_fetch_sub(&bbobj->ReferenceCount, Bias, __ATOMIC_RELAXED);
        assert(refcount >= Bias);  // this means the ref count went negative; this is bad.
        reasonRefCount = __atomic_fetch_sub(&bbobj->ReferenceReasons[Reason], Bias, __ATOMIC_RELAXED);
        assert(reasonRefCount >= Bias);  // underflow
        if (Bias == refcount) {
            // Now it is safe for us to delete this
            bbobj->ObjectAttributes.Deallocate(Object, bbobj->DataLength);  // This should remove all external usage of this object
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

    return __atomic_load_n(&bbobj->ReferenceCount, __ATOMIC_RELAXED);
}

uint64_t BitbucketGetObjectReasonReferenceCount(void *Object, uint8_t Reason)
{
    bitbucket_object_header_t *bbobj;

    assert(NULL != Object);
    bbobj = container_of(Object, bitbucket_object_header_t, Data);
    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(bbobj);
    assert(Reason < BITBUCKET_MAX_REFERENCE_REASONS);

    return __atomic_load_n(&bbobj->ReferenceReasons[Reason], __ATOMIC_RELAXED);
}

// Obtain the reason counts. Results are only for debug use, since this is done as
// an unlocked operation.
void BitbucketGetObjectReasonReferenceCounts(void *Object, uint32_t *Counts, uint8_t CountEntries)
{
    bitbucket_object_header_t *bbobj;

    assert(NULL != Object);
    assert(NULL != Counts);
    bbobj = container_of(Object, bitbucket_object_header_t, Data);
    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(bbobj);

    assert(CountEntries <= BITBUCKET_MAX_REFERENCE_REASONS);
    // Note that this is not guaranteed accurate!
    memcpy(Counts, bbobj->ReferenceReasons, sizeof(uint32_t) * bbobj->ReferenceCount);
}

const char *BitbucketGetObjectReasonName(void *Object, uint8_t Reason)
{
    bitbucket_object_header_t *bbobj;

    assert(NULL != Object);
    bbobj = container_of(Object, bitbucket_object_header_t, Data);
    CHECK_BITBUCKET_OBJECT_HEADER_MAGIC(bbobj);

    assert(Reason < bbobj->ObjectAttributes.ReasonCount);
    return bbobj->ObjectAttributes.ReferenceReasonsNames[Reason];
}
