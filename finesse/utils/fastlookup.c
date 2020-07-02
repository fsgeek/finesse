/*
 * Copyright (c) 2020 Tony Mason
 * All rights reserved.
 */

#define _GNU_SOURCE

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
#include "crc32c.h"
#include "murmurhash3.h"

static inline void verify_magic(const char *StructureName, const char *File, const char *Function, unsigned Line, uint64_t Magic, uint64_t CheckMagic)
{
    if (Magic != CheckMagic) {
        fprintf(stderr, "%s (%s:%d) Magic number mismatch (%lx != %lx) for %s\n", Function, File, Line, Magic, CheckMagic, StructureName);
        assert(Magic == CheckMagic);
    }
}

typedef struct _lookup_entry_table_bucket lookup_entry_table_bucket_t;
typedef struct _lookup_entry_table lookup_entry_table_t;

struct _lookup_entry_table_bucket {
    uint64_t                        Magic;
    uint64_t                        EntryCount;
    enum {
                                    LookupEntryTypeLinkedLists = 73,
                                    LookupEntryTypeLookupTable = 91,
    }                               LookupEntryType;
    union {
        struct {
            list_entry_t            InodeTableEntry;
            list_entry_t            UuidTableEntry;
        }                           LinkedLists;
        struct {
            lookup_entry_table_t   *Table;
            uint32_t                HashSeed;
        }                           LookupTable;
    }                               LookupEntryInstance;
    uint64_t                      (*HashFunction)(lookup_entry_table_bucket_t *Bucket);
    uint64_t                        LastHashValue;
    pthread_rwlock_t                Lock;
    // char                            UnusedSpace[8]; // pad to 64 bytes
};

#define FAST_LOOKUP_TABLE_BUCKET_MAGIC (0x7800c6664e1c877c)
#define CHECK_FAST_LOOKUP_TABLE_BUCKET_MAGIC(fltb) verify_magic("lookup_entry_table_bucket_t", __FILE__, __func__, __LINE__, FAST_LOOKUP_TABLE_BUCKET_MAGIC, (fltb)->Magic)

_Static_assert(0 == sizeof(lookup_entry_table_bucket_t) % 64, "lookup_entry_table_bucket_t length is not a multiple of 64 (cache line)");


typedef struct _lookup_entry_table {
    uint64_t                     Magic;
    uint64_t                     EntryCount;
    uint8_t                      BucketCountShift; // # of bits to shift for table size and (1<<BucketCountShift) is the # of buckets
    pthread_rwlock_t             ActiveEntryListLock;
    lookup_entry_table_bucket_t *Buckets;
    uint32_t                     HashSeed;
    char                         Pad[30]; // fill out structure to ensure cache line alignment, assuming the cache line is 64 bytes.
} lookup_entry_table_t;

#define FAST_LOOKUP_TABLE_MAGIC (0xe54c949fe52c2af5)
#define CHECK_FAST_LOOKUP_TABLE_MAGIC(flt) verify_magic("lookup_entry_table_t", __FILE__, __func__, __LINE__, FAST_LOOKUP_TABLE_MAGIC, (flt)->Magic)

// static char foo[sizeof(lookup_entry_table_t)];

_Static_assert(0 == sizeof(lookup_entry_table_t) % 64, "lookup_entry_table_t length is not a multiple of 64 bytes (cache line)");

typedef struct _lookup_entry {
    uint64_t                        Magic;
    uint64_t                        ReferenceCount;
    list_entry_t                    InodeListEntry;
    list_entry_t                    UuidListEntry;
    finesse_object_t                Object;
}  lookup_entry_t;

#define FAST_LOOKUP_ENTRY_MAGIC (0x4a0a84a016989cdf)
#define CHECK_FAST_LOOKUP_ENTRY_MAGIC(fle) verify_magic("lookup_entry_t", __FILE__, __func__, __LINE__, FAST_LOOKUP_TABLE_MAGIC, (fle)->Magic)


uint64_t (*HashFunction)(void *Data, size_t DataLength);


static uint64_t truncate_hash(uint64_t UntruncatedHash, uint8_t Shift)
{
    uint64_t mask = (1 << Shift) - 1; // 2^N - 1
    uint64_t uh = UntruncatedHash;
    uint64_t th = UntruncatedHash & mask;

    assert(Shift < 32);

    for (unsigned index = 0; index < 64 / Shift ; index += Shift) 
    {
        uh = uh >> Shift;
        th ^= uh & mask;
    }

    return (th & mask);
}

void TestFastLookup(void);
void TestFastLookup(void)
{
    assert(0); // not really implemented
    truncate_hash(0, 0);
}

static uint64_t LookupBucketHashFunction(lookup_entry_table_bucket_t *Bucket)
{
    uint64_t mh[2];
    uint64_t hash;

    assert(NULL != Bucket);
    CHECK_FAST_LOOKUP_TABLE_BUCKET_MAGIC(Bucket);

    MurmurHash3_x64_128(Bucket, offsetof(lookup_entry_table_bucket_t, LastHashValue), 0x7800c666, &mh);
    hash = mh[0] ^ mh[1]; // mix them
    if (0 == hash) {
        hash = ~0; // so we can use 0 as "not valid"
    }

    return hash;

}

//
// Create a new lookup table with BucketCount buckets
// and using HashSeed; if the latter is 0, a default value
// is used.
//
static void *CreateLookupTable(uint16_t BucketCount, uint32_t HashSeed) {
    // int status = 0;
    lookup_entry_table_t *table = NULL;
    size_t size = 0;
    size_t offset = sizeof(lookup_entry_table_t);

    offset += 63; // round up
    offset &= ~63; // truncate to nearest 64 byte boundary
    size = offset + (BucketCount * sizeof(lookup_entry_table_bucket_t));

    table = (lookup_entry_table_t *)malloc(size);
    assert(NULL != table);

    table->Magic = FAST_LOOKUP_TABLE_MAGIC;
    table->EntryCount = 0;
    switch(BucketCount) {
        default:
            assert(0); // this is not a supported bucket count;
            break;
        case 1024:
            table->BucketCountShift = 10;
            break;
        case 2048:
            table->BucketCountShift = 11;
            break;
        case 4096:
            table->BucketCountShift = 12;
            break;
        case 8192:
            table->BucketCountShift = 13;
            break;
        case 16384:
            table->BucketCountShift = 14;
            break;
        case 32768:
            table->BucketCountShift = 15;
    }
    pthread_rwlock_init(&table->ActiveEntryListLock, NULL);
    if (0 == HashSeed) {
        table->HashSeed = 0x7800c666;
    }
    else {
        table->HashSeed = HashSeed;
    }

    table->Buckets = (lookup_entry_table_bucket_t *)(((uintptr_t)table) + offset);
    for (unsigned index = 0; index < BucketCount; index++) {
        table->Buckets[index].Magic = FAST_LOOKUP_TABLE_BUCKET_MAGIC;
        table->Buckets[index].EntryCount = 0;
        table->Buckets[index].LookupEntryType = LookupEntryTypeLinkedLists; // this is always where we start
        initialize_list(&table->Buckets[index].LookupEntryInstance.LinkedLists.InodeTableEntry);
        initialize_list(&table->Buckets[index].LookupEntryInstance.LinkedLists.UuidTableEntry);
        table->Buckets[index].HashFunction = LookupBucketHashFunction;
        table->Buckets[index].LastHashValue = LookupBucketHashFunction(&table->Buckets[index]);
        pthread_rwlock_init(&table->Buckets[index].Lock, NULL);
    }

    return table;

}

//
// This tearms down a lookup table
//
// Note that there shouldn't be any remaining activity on this
// lookup table!
//
static void DestroyLookupTable(lookup_entry_table_t *Table)
{
    uint64_t bucketCount = 0;
    lookup_entry_table_bucket_t *bucket = NULL;
    list_entry_t *le = NULL;
    lookup_entry_t *entry = NULL;

    assert(NULL != Table);
    CHECK_FAST_LOOKUP_TABLE_MAGIC(Table);

    // To cleanup all the entries, it suffices for us to free
    // the inode entries and ignore the uuid lists, as each
    // entry is inserted into each one.
    bucketCount = (1 << Table->BucketCountShift);
    for (unsigned index = 0; index < bucketCount; index++) {
        bucket = &Table->Buckets[index];
        while (!empty_list(&bucket->LookupEntryInstance.LinkedLists.InodeTableEntry)) {
            le = remove_list_head(&bucket->LookupEntryInstance.LinkedLists.InodeTableEntry);
            entry = container_of(le, lookup_entry_t, InodeListEntry);
            free(entry);
        }
    }

    // Now free the table - recall the buckets were part of its
    // allocation, so those are gone as well.
    free(Table);
}

static uint64_t LockBucket(lookup_entry_table_bucket_t *Bucket, int Exclusive)
{
    uint64_t currentHashValue = 0;

    assert(NULL != Bucket);
    CHECK_FAST_LOOKUP_TABLE_BUCKET_MAGIC(Bucket);

    if (0 == Exclusive) {
        pthread_rwlock_rdlock(&Bucket->Lock);
    }
    else {
        pthread_rwlock_wrlock(&Bucket->Lock);
    }

    if (Bucket->HashFunction) {
        currentHashValue = Bucket->HashFunction(Bucket);
        // Since we preserve this just before unlock,
        // And should only change it with the lock held,
        // This should be true.  Goal: detect unlock changes.
        assert(Bucket->LastHashValue == currentHashValue);

        return currentHashValue;
    }

    if (NULL != Bucket->HashFunction) {
        return Bucket->HashFunction(Bucket);
    }

    return 0;
}

static void UnlockBucket(lookup_entry_table_bucket_t *Bucket)
{
    int exclusive;
    int status;
    uint64_t currentHashValue = 0;

    assert(NULL != Bucket);
    CHECK_FAST_LOOKUP_TABLE_BUCKET_MAGIC(Bucket);

    if (Bucket->HashFunction) {
        currentHashValue = Bucket->HashFunction(Bucket);
    }

    status = pthread_rwlock_trywrlock(&Bucket->Lock);
    assert(0 != status);

    status = pthread_rwlock_tryrdlock(&Bucket->Lock);
    if (-1 == status) {
        status = errno;
    }

    switch(status) {
        case 0:
            // the lock was acquired so it must be shared
            pthread_rwlock_unlock(&Bucket->Lock); // this reverses our successful trylock
            break;
        case EDEADLK: // this thread owns the lock exclusive
        case EBUSY: // SOME thread owns the lock exclusive (hopefully it's this thread!)
            exclusive = 1;
            break;
        case EINVAL:
            assert(0); // this lock isn't initialized
            break;
        case EAGAIN:
            // the lock is acquired shared by someone (e.g., this thread)
            break;
    }

    if (exclusive) {
        // Only the exclusive locker should change this!
        // Let's preserve it now.
        Bucket->LastHashValue = currentHashValue;
    }
    else {
        assert(Bucket->LastHashValue == currentHashValue);
    }

    pthread_rwlock_unlock(&Bucket->Lock);

}

static uint16_t GetBucketIndex(lookup_entry_table_t *Table, void *Key, size_t KeyLength)
{
    uint64_t mh[2];
    uint64_t hash;
    uint16_t index;

    assert(NULL != Table);
    assert(NULL != Key);
    assert(KeyLength > 0);
    CHECK_FAST_LOOKUP_TABLE_MAGIC(Table);

    MurmurHash3_x64_128(Key, KeyLength, Table->HashSeed, &mh);

    hash = mh[0] ^ mh[1]; // mix them
    index = (uint16_t) truncate_hash(hash, Table->BucketCountShift);

    assert(index < (1 << Table->BucketCountShift));

    return index;

}

static inline uint16_t GetBucketIndexForInode(lookup_entry_table_t *Table, fuse_ino_t InodeNumber)
{
    assert(0 != InodeNumber);
    return GetBucketIndex(Table, &InodeNumber, sizeof(fuse_ino_t));
}

static inline uint16_t GetBucketIndexForUuid(lookup_entry_table_t *Table, uuid_t *Uuid)
{
    assert(!uuid_is_null(*Uuid));
    return GetBucketIndex(Table, Uuid, sizeof(uuid_t));
}


//
// A common routine for looking up an entry in a bucket.
// Only one of Inode or Uuid can be valid.
// Caller must have this bucket locked
//
// Entry is returned without a reference being added.
//
static lookup_entry_t *lookup_entry(lookup_entry_table_bucket_t *Bucket, fuse_ino_t InodeNumber, uuid_t *Uuid)
{
    list_entry_t *le = NULL;
    lookup_entry_t *entry = NULL;
    int check_inode = 0;
    list_entry_t *listHead;

    assert((0 == InodeNumber) || uuid_is_null(*Uuid)); // at least one must NOT be chosen
    assert((0 != InodeNumber) || !uuid_is_null(*Uuid)); // at least one MUST be chosen
    assert(NULL != Bucket);
    CHECK_FAST_LOOKUP_TABLE_BUCKET_MAGIC(Bucket);   
    assert(LookupEntryTypeLinkedLists == Bucket->LookupEntryType); // we haven't coded anything else

    if (0 != InodeNumber) {
        check_inode = 1;
        listHead = &Bucket->LookupEntryInstance.LinkedLists.InodeTableEntry;
    }
    else {
        check_inode = 0;
        listHead = &Bucket->LookupEntryInstance.LinkedLists.UuidTableEntry;
    }

    list_for_each(listHead, le) {
        if (check_inode) {
            entry = container_of(le, lookup_entry_t, InodeListEntry);
            if (entry->Object.inode == InodeNumber) {
                break;
            }
        }
        else {
            entry = container_of(le, lookup_entry_t, UuidListEntry);
            if  (0 == memcmp(&entry->Object.uuid, Uuid, sizeof(uuid_t))) {
                break;
            }
        }
        entry = NULL;
    }

    return entry;

}

finesse_object_t *FinesseObjectLookupByIno(finesse_object_table_t *Table, fuse_ino_t InodeNumber)
{
    lookup_entry_table_t *table = (lookup_entry_table_t *)Table;
    lookup_entry_table_bucket_t *bucket = NULL;
    uint16_t index;
    lookup_entry_t *entry = NULL;
    uuid_t null_uuid;

    uuid_clear(null_uuid);

    assert(NULL != table);
    CHECK_FAST_LOOKUP_TABLE_MAGIC(table);

    index = GetBucketIndexForInode(table, InodeNumber);
    bucket = &table->Buckets[index];
    CHECK_FAST_LOOKUP_TABLE_BUCKET_MAGIC(bucket);   
    assert(LookupEntryTypeLinkedLists == bucket->LookupEntryType); // we haven't coded anything else

    LockBucket(bucket, 0);
    entry = lookup_entry(bucket, InodeNumber, &null_uuid);
    if (NULL != entry) {
        __atomic_fetch_add(&entry->ReferenceCount, 1, __ATOMIC_RELAXED); // bump ref count
    }
    UnlockBucket(&table->Buckets[index]);

    if (NULL != entry) {
        return &entry->Object;
    }
    return NULL;
}

finesse_object_t *FinesseObjectLookupByUuid(finesse_object_table_t *Table, uuid_t *Uuid)
{
    lookup_entry_table_t *table = (lookup_entry_table_t *)Table;
    lookup_entry_table_bucket_t *bucket = NULL;
    uint16_t index;
    lookup_entry_t *entry = NULL;

    assert(NULL != table);
    CHECK_FAST_LOOKUP_TABLE_MAGIC(table);

    index = GetBucketIndexForUuid(table, Uuid);
    bucket = &table->Buckets[index];
    CHECK_FAST_LOOKUP_TABLE_BUCKET_MAGIC(bucket);   
    assert(LookupEntryTypeLinkedLists == bucket->LookupEntryType); // we haven't coded anything else

    LockBucket(bucket, 0);
    entry = lookup_entry(bucket, 0, Uuid);
    if (NULL != entry) {
        __atomic_fetch_add(&entry->ReferenceCount, 1, __ATOMIC_RELAXED); // bump ref count
    }
    UnlockBucket(&table->Buckets[index]);

    if (NULL != entry) {
        return &entry->Object;
    }
    return NULL;
}

static void release_entry(lookup_entry_table_t *Table, lookup_entry_t * Entry)
{
    uint16_t index, first, second;
    uint64_t refCount = 0;

    first = GetBucketIndexForInode(Table, Entry->Object.inode);
    second = GetBucketIndexForUuid(Table, &Entry->Object.uuid);
    if (first > second) {
        index = first;
        first = second;
        second = index;
    }

    LockBucket(&Table->Buckets[first], 1);
    if (first != second) {
        LockBucket(&Table->Buckets[second], 1);
    }

    refCount = __atomic_fetch_sub(&Entry->ReferenceCount, 1, __ATOMIC_RELAXED);
    if (1 == refCount) {
        remove_list_entry(&Entry->InodeListEntry);
        remove_list_entry(&Entry->UuidListEntry);
        __atomic_fetch_sub(&Table->Buckets[first].EntryCount, 1, __ATOMIC_RELAXED);
        __atomic_fetch_sub(&Table->Buckets[second].EntryCount, 1, __ATOMIC_RELAXED);
        memset(Entry, 0, sizeof(lookup_entry_t)); // this is really a debug aid...
        free(Entry);
    }

    if (first != second) {
        UnlockBucket(&Table->Buckets[second]);
    }
    UnlockBucket(&Table->Buckets[first]);

}

void FinesseObjectRelease(finesse_object_table_t *Table, finesse_object_t *Object)
{
    lookup_entry_t *entry = NULL;
    lookup_entry_table_t *table = (lookup_entry_table_t *)Table;
    uint64_t refCount = 0;
    lookup_entry_table_bucket_t *bucket = NULL;
    uint16_t index = ~0;

    assert(NULL != Object);
    assert(NULL != Table);
    CHECK_FAST_LOOKUP_TABLE_MAGIC(table);
   
    index = GetBucketIndexForUuid(table, &Object->uuid);
    bucket = &table->Buckets[index];
    CHECK_FAST_LOOKUP_TABLE_BUCKET_MAGIC(bucket);

    LockBucket(bucket, 0);
    entry = lookup_entry(bucket, 0, &Object->uuid);
    assert(NULL != entry); // logic error otherwise
    refCount = __atomic_fetch_sub(&entry->ReferenceCount, 1, __ATOMIC_RELAXED);
    if (1 == refCount) {
        // this is the removal but we don't hold the exclusive lock
        __atomic_fetch_add(&entry->ReferenceCount, 1, __ATOMIC_RELAXED);
    }
    UnlockBucket(bucket);

    if (1 == refCount) {
        release_entry(table, entry);
    }
}

static lookup_entry_t *insert_entry(lookup_entry_table_t *Table, fuse_ino_t InodeNumber, uuid_t *Uuid)
{
    uint16_t index, first, second;
    lookup_entry_table_bucket_t *inode_bucket = NULL;
    lookup_entry_table_bucket_t *uuid_bucket = NULL;
    lookup_entry_t *entry = NULL;

    assert(NULL != Table);
    assert(0 != InodeNumber);
    assert(!uuid_is_null(*Uuid));

    first = GetBucketIndexForInode(Table, InodeNumber);
    inode_bucket = &Table->Buckets[first];
    second = GetBucketIndexForUuid(Table, Uuid);
    uuid_bucket = &Table->Buckets[second];
    if (first > second) {
        index = first;
        first = second;
        second = index;
    }

    entry = malloc(sizeof(lookup_entry_t));
    assert(NULL != entry);
    entry->Magic = FAST_LOOKUP_ENTRY_MAGIC;
    entry->ReferenceCount = 1;
    initialize_list_entry(&entry->InodeListEntry);
    initialize_list_entry(&entry->UuidListEntry);
    entry->Object.inode = InodeNumber;
    uuid_copy(entry->Object.uuid, *Uuid);

    LockBucket(&Table->Buckets[first], 1);
    if (first != second) {
        LockBucket(&Table->Buckets[second], 1);
    }

    // Haven't coded anything else
    assert(LookupEntryTypeLinkedLists == inode_bucket->LookupEntryType);
    assert(LookupEntryTypeLinkedLists == uuid_bucket->LookupEntryType);

    insert_list_tail(&inode_bucket->LookupEntryInstance.LinkedLists.InodeTableEntry, &entry->InodeListEntry);
    insert_list_tail(&uuid_bucket->LookupEntryInstance.LinkedLists.UuidTableEntry, &entry->UuidListEntry);

    __atomic_fetch_add(&inode_bucket->EntryCount, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&uuid_bucket->EntryCount, 1, __ATOMIC_RELAXED);

    if (first != second) {
        UnlockBucket(&Table->Buckets[second]);
    }
    UnlockBucket(&Table->Buckets[first]);

    return entry;

}


finesse_object_t *FinesseObjectCreate(finesse_object_table_t *Table, fuse_ino_t InodeNumber, uuid_t *Uuid)
{
    lookup_entry_t *entry = NULL;

    entry = insert_entry((lookup_entry_table_t *)Table, InodeNumber, Uuid);
    assert(NULL != entry);

    return &entry->Object;
}

uint64_t FinesseObjectGetTableSize(finesse_object_table_t *Table)
{
    lookup_entry_table_t *table = (lookup_entry_table_t *)Table;

    return __atomic_load_n(&table->EntryCount, __ATOMIC_RELAXED);
}

void FinesseDestroyTable(finesse_object_table_t *Table)
{
    DestroyLookupTable((lookup_entry_table_t *) Table);
}

finesse_object_table_t *FinesseCreateTable(uint64_t EstimatedSize)
{
    uint64_t bucket_count = EstimatedSize / 1024;
    unsigned index;

    if (bucket_count < 1024) {
        bucket_count = 1024; // smallest size we're doing for now
    }

    if (bucket_count > 32768) {
        bucket_count = 32768;
    }

    // Let's guarantee this is a power-of-two value
    index = 10;
    while (1 != (bucket_count >> index)) {
        index++;
    }

    bucket_count = 1 << index;

    // Use default hash seed, pass in estimated table size
    return (finesse_object_table_t *)CreateLookupTable(bucket_count, 0);

}

#if 0
//
// This lookup table takes the estimated number of elements and returns
// an initialized lookup table.
// An ExpectedSize of 0 will choose the default size.
//
FinesseInitializeLookupTable(uint64_t ExpectedSize)
{

}

void FinesseInsertEntry(void *Table, lookup_entry_t *NewEntry)
{

}

//
// This looks up an entry in the given table based upon the given Inode number.
FinesseLookupEntryByInode(void *Table, ino_t InodeNumber)
{

}
#endif // 0