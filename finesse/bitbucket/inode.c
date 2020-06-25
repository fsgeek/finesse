//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketdata.h"
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <fuse_log.h>

int bitbucket_debug_refcount = 1;

typedef struct _bitbucket_inode_table_entry {
    uint64_t             Magic;
    list_entry_t         ListEntry;
    bitbucket_inode_t   *Inode;
} bitbucket_inode_table_entry_t;

#define BITBUCKET_INODE_TABLE_ENTRY_MAGIC (0x6afb8bda9a2b8489)
#define CHECK_BITBUCKET_INODE_TABLE_ENTRY_MAGIC(ite) verify_magic("bitbucket_inode_table_entry_t", __FILE__, __func__, __LINE__, BITBUCKET_INODE_TABLE_ENTRY_MAGIC, (ite)->Magic)

struct _bitbucket_inode_table {
    uint64_t            Magic;
    uint16_t            BucketCount;
    uint64_t            InodeCount;
    struct {
        list_entry_t        ListEntry;
        pthread_rwlock_t    Lock;
        uint64_t            Count;
    } Buckets[BITBUCKET_INODE_TABLE_BUCKETS];
};

#define BITBUCKET_INODE_TABLE_MAGIC (0xc132b27785769815)
#define CHECK_BITBUCKET_INODE_TABLE_MAGIC(it) verify_magic("bitbucket_inode_table_t", __FILE__, __func__, __LINE__, BITBUCKET_INODE_TABLE_MAGIC, (it)->Magic)


static ino_t get_new_inode_number(void) 
{
    static int initialized = 0;
    static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;
    static ino_t last_inode_number = 0;
    static ino_t inode_bias = 0;
    ino_t ino = 0;

    if (!initialized) {
        pthread_mutex_lock(&init_lock);
        while (!initialized) {
            // randomly generate the starting inode number
            last_inode_number = (ino_t)random();
            while (0 == inode_bias) {
                inode_bias = (ino_t)random();
            }
            initialized = 1; // initialization is done
        }
        pthread_mutex_unlock(&init_lock);
    }

    while (0 == ino) {
        ino =  __atomic_fetch_add(&last_inode_number, inode_bias, __ATOMIC_RELAXED); // arbitrary bias
    }

    return ino;
}

static uint16_t hash_inode(ino_t Inode) 
{
    uint64_t h = (uint64_t)Inode;
    uint16_t ih;

    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53L;
    h ^= h >> 33;

    // All I really want is a 10 bit value here

    ih = h & 0xFFF;
    ih ^= (h >> 10) & 0x3FF;
    ih ^= (h >> 10) & 0x3FF;
    ih ^= (h >> 10) & 0x3FF;
    ih ^= (h >> 10) & 0x3FF;
    ih ^= (h >> 10) & 0x3FF;

    return (ih & 0x3FF);
}

static void LockInodeBucket(bitbucket_inode_table_t *Table, uint16_t BucketId, int Change)
{
    int status = 0;

    assert(NULL != Table);
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(Table);
    assert(BucketId < Table->BucketCount);

    if (Change) {
        status = pthread_rwlock_wrlock(&Table->Buckets[BucketId].Lock);
        assert(0 == status);
    }
    else {
        pthread_rwlock_rdlock(&Table->Buckets[BucketId].Lock);
        assert(0 == status);
    }
}

static int TryLockInodeBucket(bitbucket_inode_table_t *Table, uint16_t BucketId, int Change)
{
    int status = 0;

    assert(NULL != Table);
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(Table);
    assert(BucketId < Table->BucketCount);

    if (Change) {
        status = pthread_rwlock_trywrlock(&Table->Buckets[BucketId].Lock);
    }
    else {
        status = pthread_rwlock_tryrdlock(&Table->Buckets[BucketId].Lock);
    }

    return status;
}

static void UnlockInodeBucket(bitbucket_inode_table_t *Table, uint16_t BucketId)
{
    int status;
    assert(NULL != Table);
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(Table);
    assert(BucketId < BITBUCKET_INODE_TABLE_BUCKETS);

    status = pthread_rwlock_unlock(&Table->Buckets[BucketId].Lock);
    assert(0 == status);
}
static void LockInodeForLookup(bitbucket_inode_table_t *Table, ino_t Inode)
{
    uint16_t bucketId = hash_inode(Inode);

    assert(NULL != Table);
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(Table);

    LockInodeBucket(Table, bucketId, 0);
}

static int TryLockInode(bitbucket_inode_table_t *Table, ino_t Inode, int Exclusive)
{
    uint16_t bucketId = hash_inode(Inode);

    assert(NULL != Table);
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(Table);

    return TryLockInodeBucket(Table, bucketId, Exclusive);
}

static void LockInodeForChange(bitbucket_inode_table_t *Table, ino_t Inode)
{
    uint16_t bucketId = hash_inode(Inode);

    assert(NULL != Table);
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(Table);

    LockInodeBucket(Table, bucketId, 1);
}


static void UnlockInode(bitbucket_inode_table_t *Table, ino_t Inode)
{
    uint16_t bucketId = hash_inode(Inode);

    assert(NULL != Table);
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(Table);

    UnlockInodeBucket(Table, bucketId);
}

static bitbucket_inode_table_entry_t *LookupInodeInTableLocked(bitbucket_inode_table_t *Table, const ino_t Inode)
{
    assert(0 != Inode);
    assert(NULL != Table);
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(Table);
    uint16_t bucketId = ~0;
    list_entry_t *entry;
    bitbucket_inode_table_entry_t *ite = NULL;

    bucketId = hash_inode(Inode);

    list_for_each(&Table->Buckets[bucketId].ListEntry, entry) {
        bitbucket_inode_table_entry_t *dite = list_item(entry, bitbucket_inode_table_entry_t, ListEntry);

        CHECK_BITBUCKET_INODE_TABLE_ENTRY_MAGIC(dite);
        if (Inode == dite->Inode->Attributes.st_ino) {
            ite = dite;
            break;
        }
    }

    return ite;
}

int BitbucketInsertInodeInTable(void *Table, bitbucket_inode_t *Inode)
{
    bitbucket_inode_table_entry_t *ite = NULL;
    int result = 0;
    uint16_t bucketId = ~0;
    bitbucket_inode_table_t *table = (bitbucket_inode_table_t *)Table;

    assert(NULL != Table);
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(table);       
    assert(0 != Inode);
    assert(BITBUCKET_UNKNOWN_TYPE != Inode->InodeType); // shouldn't insert an unknown Inode
    CHECK_BITBUCKET_INODE_MAGIC(Inode);
    assert(NULL == Inode->Table);
    ite = (bitbucket_inode_table_entry_t *)malloc(sizeof(bitbucket_inode_table_entry_t));
    ite->Magic = BITBUCKET_INODE_TABLE_ENTRY_MAGIC;
    ite->Inode = Inode;
    assert(FUSE_ROOT_ID != Inode->Attributes.st_ino); // we don't use this value
    assert(0 != Inode->Attributes.st_ino);
    assert(FUSE_ROOT_ID != Inode->Attributes.st_ino); // for now, we don't allow this - keep it in the userdata
    bucketId = hash_inode(Inode->Attributes.st_ino);

    LockInodeBucket(table, bucketId, 1);
    while (NULL != ite) {
        bitbucket_inode_table_entry_t *dite = LookupInodeInTableLocked(table, Inode->Attributes.st_ino);
        if (NULL != dite) {
            // collision
            result = -EEXIST;
            break;
        }
        insert_list_head(&table->Buckets[bucketId].ListEntry, &ite->ListEntry);
        table->Buckets[bucketId].Count++;
        CHECK_BITBUCKET_INODE_TABLE_ENTRY_MAGIC(ite);
        ite = NULL;
        break;
    }
    UnlockInodeBucket(table, bucketId);

    Inode->Table = table;

    if (NULL != ite) {
        free(ite);
        ite = NULL;
    }

    __atomic_fetch_add(&table->InodeCount, 1, __ATOMIC_RELAXED);

    return result;
}


#if 0

// Removing this because deletion from the table should always be done
// by deleting the objects themselves - this ensures correct
// reference counting.  No direct deletion should occur (except perhaps
// in table teardown itself...)

// Given an inode number it removes it from the table, if
// it exists.  Return value is the object pointer of the
// deleted entry (NULL otherwise).
//
// Note: this call will release the inode table reference.
void BitbucketRemoveInodeFromTable(bitbucket_inode_t *Inode)
{
    bitbucket_inode_table_entry_t *ite = NULL;
    bitbucket_inode_table_t *table; // = (bitbucket_inode_table_t *)Table;
    ino_t ino = 0;
    uint16_t bucketId = hash_inode(Inode->Attributes.st_ino);

    assert(NULL != Inode);
    CHECK_BITBUCKET_INODE_MAGIC(Inode);

    table = Inode->Table;
    assert(NULL != table); // can't remove it if it isn't in the table
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(table);

    ino = Inode->Attributes.st_ino;
    LockInodeForChange(table, ino);
    ite = LookupInodeInTableLocked(table, ino);
    if (NULL != ite) {
        remove_list_entry(&ite->ListEntry);
        table->Buckets[bucketId].Count--;
    }
    UnlockInode(table, ino);

    if (NULL != ite) {
        if (NULL != ite->Inode) {
            BitbucketDereferenceInode(ite->Inode, INODE_TABLE_REFERENCE); // drop our reference
            ite->Inode = NULL;
        }
        free(ite);
    }
}
#endif // 0

//
// Return the number of Inodes in the table at the time of the
// call (note this value is likely changing).
//
uint64_t BitbucketGetInodeTableCount(void *Table)
{
    bitbucket_inode_table_t *table = (bitbucket_inode_table_t *)Table;

    assert(NULL != Table);
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(table);
    
    return __atomic_load_n(&table->InodeCount, __ATOMIC_RELAXED);
}

// Find an inode from the inode number in the specified table
// If found, a reference counted pointer to the inode is returned
// If not found, NULL is returned.
bitbucket_inode_t *BitbucketLookupInodeInTable(void *Table, ino_t Inode)
{
    bitbucket_inode_table_entry_t *ite = NULL;
    bitbucket_inode_table_t *table = (bitbucket_inode_table_t *)Table;
    bitbucket_inode_t *inode = NULL;

    assert(NULL != Table);
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(table);
    assert(0 != Inode);

    LockInodeForLookup(table, Inode);
    ite = LookupInodeInTableLocked(table, Inode);
    if (NULL != ite) {
        inode = ite->Inode;
        BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
    }
    UnlockInode(table, Inode);

    return inode;
}

void *BitbucketCreateInodeTable(uint16_t BucketCount)
{
    int status = 0;
    bitbucket_inode_table_t *inode_table = NULL;

    assert(BITBUCKET_INODE_TABLE_BUCKETS == BucketCount); // haven't coded for anything else at this point...

    while (NULL == inode_table) {
        inode_table = malloc(sizeof(bitbucket_inode_table_t));
        assert(NULL != inode_table);
    }

    inode_table->Magic = BITBUCKET_INODE_TABLE_MAGIC;
    inode_table->BucketCount = BucketCount;
    for (unsigned index = 0; index < BucketCount; index++) {
        inode_table->Buckets[index].Count = 0;
        status = pthread_rwlock_init(&inode_table->Buckets[index].Lock, NULL); // default attributes (process private)
        assert(0 == status);
        initialize_list_entry(&inode_table->Buckets[index].ListEntry); // TODO: make this a double linked list head
    }

    return inode_table;
}

void BitbucketDestroyInodeTable(void *Table)
{
    bitbucket_inode_table_t *inode_table = (bitbucket_inode_table_t *)Table;
    int status;

    assert(NULL != inode_table);
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(inode_table);

    for (unsigned index = 0; index < inode_table->BucketCount; index++) {
        status = pthread_rwlock_destroy(&inode_table->Buckets[index].Lock);
        assert(0 == status);
        assert(0 == inode_table->Buckets[index].Count); // no logic to clean up these entries at present
        assert(empty_list(&inode_table->Buckets[index].ListEntry)); // again, no logic for cleanup here
    }

    free(inode_table);
}



typedef struct _bitbucket_private_inode {
    uint64_t                        Magic; // another magic number
    size_t                          Length; // size of this object
    bitbucket_object_attributes_t   RegisteredAttributes; // not ours, the component creating this inode
    bitbucket_inode_table_t        *Table; // the table containing this inode
    char                            Unused0[40]; // align next field to 64 bytes
    bitbucket_inode_t               PublicInode; // this is what we return to the caller - must be last field
} bitbucket_private_inode_t;

// const char foo[(64 * 6) - offsetof(bitbucket_private_inode_t, PublicInode)];
_Static_assert((0 == offsetof(bitbucket_private_inode_t, PublicInode) % 64), "Alignment issue in private inode struct");

#define BITBUCKET_PRIVATE_INODE_MAGIC (0xe7820c4eb6a8f620)
#define CHECK_BITBUCKET_PRIVATE_INODE_MAGIC(bbpi) verify_magic("bitbucket_private_inode_t", __FILE__, __func__, __LINE__, BITBUCKET_PRIVATE_INODE_MAGIC, (bbpi)->Magic)

static void InodeInitialize(void *Object, size_t Length) 
{
    int status;

    bitbucket_private_inode_t *bbpi = (bitbucket_private_inode_t *)Object;
    bbpi->Magic = BITBUCKET_PRIVATE_INODE_MAGIC;
    memset(&bbpi->RegisteredAttributes, 0, sizeof(bitbucket_object_attributes_t));
    bbpi->Table = NULL;
    bbpi->Length = Length;
    memset(&bbpi->PublicInode, 0, sizeof(bitbucket_inode_t));
    bbpi->PublicInode.Magic = BITBUCKET_INODE_MAGIC;
    bbpi->PublicInode.InodeType = BITBUCKET_UNKNOWN_TYPE;
    bbpi->PublicInode.InodeLength = Length - offsetof(bitbucket_private_inode_t, PublicInode);
    status = pthread_rwlock_init(&bbpi->PublicInode.InodeLock, NULL);
    assert(0 == status);
    uuid_generate(bbpi->PublicInode.Uuid);
    uuid_unparse(bbpi->PublicInode.Uuid, bbpi->PublicInode.UuidString);
    bbpi->PublicInode.Epoch = rand(); // detect changes
    status = clock_gettime(CLOCK_TAI, &bbpi->PublicInode.CreationTime);
    assert(0 == status);
    bbpi->PublicInode.Attributes.st_atim = bbpi->PublicInode.CreationTime;
    bbpi->PublicInode.Attributes.st_mtim = bbpi->PublicInode.CreationTime;
    bbpi->PublicInode.Attributes.st_atim = bbpi->PublicInode.CreationTime;
    bbpi->PublicInode.Attributes.st_ctim = bbpi->PublicInode.CreationTime;
    bbpi->PublicInode.Attributes.st_mode = S_IRWXU | S_IRWXG | S_IRWXO; // is this the right default?
    bbpi->PublicInode.Attributes.st_gid = getgid();
    bbpi->PublicInode.Attributes.st_uid = getuid();
    bbpi->PublicInode.Attributes.st_size = 4096; // TODO: maybe do some sort of funky calculation here?
    bbpi->PublicInode.Attributes.st_blksize = S_BLKSIZE; // TODO: again, does this matter?
    bbpi->PublicInode.Attributes.st_blocks = bbpi->PublicInode.Attributes.st_size / 512;
    bbpi->PublicInode.Attributes.st_ino = get_new_inode_number();
    bbpi->PublicInode.Attributes.st_nlink = 0; // this should be bumped when this is added

    // BitbucketInitializeExtendedAttributes
    BitbucketInitializeExtendedAttributes(&bbpi->PublicInode);
}

static void InodeTableLock(void *Object, int Exclusive)
{
    bitbucket_private_inode_t *bbpi = NULL;
    bitbucket_inode_table_t *table = NULL;

    assert(NULL != Object);
    bbpi = (bitbucket_private_inode_t *)Object;
    CHECK_BITBUCKET_PRIVATE_INODE_MAGIC(bbpi);
    assert(NULL != bbpi->Table);
    table = bbpi->Table;
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(table);

    // Since this might be removing the entry from the table, we
    // need to exclusive lock it here.  We don't need the
    // actual inode lock.
    if (Exclusive) {
        LockInodeForChange(table, bbpi->PublicInode.Attributes.st_ino);
    }
    else {
        LockInodeForLookup(table, bbpi->PublicInode.Attributes.st_ino);
    }
}

static int InodeTableTrylock(void *Object, int Exclusive)
{
    bitbucket_private_inode_t *bbpi = NULL;
    bitbucket_inode_table_t *table = NULL;

    assert(NULL != Object);
    bbpi = (bitbucket_private_inode_t *)Object;
    CHECK_BITBUCKET_PRIVATE_INODE_MAGIC(bbpi);
    assert(NULL != bbpi->Table);
    table = bbpi->Table;
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(table);

    // Since this might be removing the entry from the table, we
    // need to exclusive lock it here.  We don't need the
    // actual inode lock.
    return TryLockInode(bbpi->Table, bbpi->PublicInode.Attributes.st_ino, Exclusive);

}


static void InodeTableUnlock(void *Object)
{
    bitbucket_private_inode_t *bbpi = NULL;
    bitbucket_inode_table_t *table = NULL;

    assert(NULL != Object);
    bbpi = (bitbucket_private_inode_t *)Object;
    CHECK_BITBUCKET_PRIVATE_INODE_MAGIC(bbpi);
    assert(NULL != bbpi->Table);
    table = bbpi->Table;
    CHECK_BITBUCKET_INODE_TABLE_MAGIC(table);

    // Since this might be removing the entry from the table, we
    // need to exclusive lock it here.  We don't need the
    // actual inode lock.
    UnlockInode(bbpi->Table, bbpi->PublicInode.Attributes.st_ino);

}


static void InodeDeallocate(void *Object, size_t Length)
{
    int status = 0;
    bitbucket_inode_table_t *table = NULL;
    bitbucket_private_inode_t *bbpi = (bitbucket_private_inode_t *)Object;
    CHECK_BITBUCKET_PRIVATE_INODE_MAGIC(bbpi);
    assert(bbpi->Length == Length);
    assert(NULL != bbpi->Table);
    table = bbpi->Table;
    bitbucket_inode_table_entry_t *ite = NULL;
    uint16_t bucketId = hash_inode(bbpi->PublicInode.Attributes.st_ino);
    uint64_t oldcount = 0;

    // We must be holding the inode table bucket lock EXCLUSIVE
    status = InodeTableTrylock(Object, 0);
    // if that call succeeded, we just acquired it shared - this is wrong.
    assert(0 != status);
    // We know that SOME caller owns the lock exclusive; let's hope it
    // is us.
    ite = LookupInodeInTableLocked(table, bbpi->PublicInode.Attributes.st_ino);
    assert(NULL != ite); // logic bug if we don't find it
    remove_list_entry(&ite->ListEntry);
    table->Buckets[bucketId].Count--;
    // We can drop the table lock now because it has been removed; we
    // were the final reference to this object, we have the table locked
    // so nobody else found it, and now we can finish tearing it down.
    InodeTableUnlock(Object);
    oldcount = __atomic_fetch_sub(&table->InodeCount, 1, __ATOMIC_RELAXED);
    assert(0 != oldcount); // underflow!

    // No longer need the inode table entry
    free(ite);
    ite = NULL;
 
    if ((NULL != bbpi->PublicInode.ExtendedAttributeTrie) ||
        !empty_list(&bbpi->PublicInode.ExtendedAttributes)) {
        BitbucketDestroyExtendedAttributes(&bbpi->PublicInode);
    }
    assert(NULL == bbpi->PublicInode.ExtendedAttributeTrie);
    assert(empty_list(&bbpi->PublicInode.ExtendedAttributes));
    


    if (NULL != bbpi->RegisteredAttributes.Deallocate) {
        // call down the chain BEFORE our own cleanup.
        assert(bbpi->PublicInode.InodeLength == Length - offsetof(bitbucket_private_inode_t, PublicInode)); // sanity
        bbpi->RegisteredAttributes.Deallocate(&bbpi->PublicInode, bbpi->PublicInode.InodeLength);
    }

    status = pthread_rwlock_destroy(&bbpi->PublicInode.InodeLock);
    assert(0 == status);

    // Just wipe the entire region out
    memset(Object, 0, Length);


}


static bitbucket_object_attributes_t InodePrivateObjectAttributes = 
{
    .Magic = BITBUCKET_OBJECT_ATTRIBUTES_MAGIC,
    .ReasonCount = BITBUCKET_MAX_REFERENCE_REASONS,
    .ReferenceReasonsNames = {
        "Lookup",
        "Dir:Parent",
        "Dir:Entry",
        "Dir:Enum",
        "Remove Dirent",
        "Fuse:Lookup",
        "Fuse:Open",
        "Fuse:Opendir",
        "Reason8",
        "Reason9",
        "Reason10",
        "Reason11",
    },
    .Initialize = InodeInitialize,
    .Deallocate = InodeDeallocate,
    .Lock = InodeTableLock,
    .Trylock = InodeTableTrylock,
    .Unlock = InodeTableUnlock,
};

// When creating an inode structure, an additional length (for variable size data) can be allocated.  Can be zero
// Object attributes are used for registering callbacks on various events (initialization, destruction, lock, unlock)
// These are optional (defaults are provided) but strongly recommended.
// These are reference counted objects.
//
// Upon return, this object will have an INODE_LOOKUP_REFERENCE
// Note: when this object is returned, it will have an INODE_LOOKUP_REFERENCE for the returned pointer.  It will also
// have an INODE_TABLE_REFERENCE for the table entry.
//
bitbucket_inode_t *BitbucketCreateInode(bitbucket_inode_table_t *Table, bitbucket_object_attributes_t *ObjectAttributes, size_t DataLength)
{
    size_t length = (sizeof(bitbucket_private_inode_t) + DataLength + 0x3F) & ~0x3F; // round up to next 64 byte value
    void *object = NULL;
    bitbucket_private_inode_t *bbpi = NULL;
    uint32_t refcount = 0;

    assert(length < 65536); // arbitrary, but seems somewhat sane at this point.
    assert(NULL != Table); // must have a table

    object = BitbucketObjectCreate(&InodePrivateObjectAttributes, length, INODE_LOOKUP_REFERENCE);
    assert(NULL != object);
    // Make sure the ref count is correct.
    refcount = BitbucketGetObjectReferenceCount(object);
    assert(1 == refcount);

    bbpi = (bitbucket_private_inode_t *)object;
    CHECK_BITBUCKET_PRIVATE_INODE_MAGIC(bbpi);

    if (NULL != ObjectAttributes) {
        CHECK_BITBUCKET_OBJECT_ATTRIBUTES_MAGIC(ObjectAttributes);
        bbpi->RegisteredAttributes = *ObjectAttributes;

        if (NULL != bbpi->RegisteredAttributes.Initialize) {
            bbpi->RegisteredAttributes.Initialize(&bbpi->PublicInode, length - offsetof(bitbucket_private_inode_t, PublicInode));
        }
    }

    // Insert this inode into the table
    BitbucketInsertInodeInTable(Table, &bbpi->PublicInode);
    bbpi->Table = Table;

    // Make sure the ref count is correct: the table reference is NOT refcounted
    refcount = BitbucketGetObjectReferenceCount(object);
    assert(1 == refcount);

    return &bbpi->PublicInode;
}

//
// Increment reference count on this inode
//
void BitbucketReferenceInode(bitbucket_inode_t *Inode, uint8_t Reason)
{
    bitbucket_private_inode_t *bbpi = container_of(Inode, bitbucket_private_inode_t, PublicInode);
    CHECK_BITBUCKET_PRIVATE_INODE_MAGIC(bbpi);

    if (bitbucket_debug_refcount) {
        uint64_t refcnt = BitbucketGetInodeReferenceCount(Inode);
        uint64_t reasoncount = BitbucketGetInodeReasonReferenceCount(Inode, Reason);

        fuse_log(FUSE_LOG_DEBUG, 
                "Finesse: Add reference to inode %ld reason %d (%s) (ref: %lu -> %lu, reason: %lu -> %lu)\n", 
                bbpi->PublicInode.Attributes.st_ino, 
                Reason,
                InodePrivateObjectAttributes.ReferenceReasonsNames[Reason],
                refcnt, refcnt + 1,
                reasoncount, reasoncount + 1);
    }

    BitbucketObjectReference(bbpi, Reason);
}

//
// Note: caller should destroy their reference after this call in most cases.
//
void BitbucketDereferenceInode(bitbucket_inode_t *Inode, uint8_t Reason, uint64_t Bias)
{
    bitbucket_private_inode_t *bbpi = container_of(Inode, bitbucket_private_inode_t, PublicInode);
    CHECK_BITBUCKET_PRIVATE_INODE_MAGIC(bbpi);

    if (bitbucket_debug_refcount) {
        uint64_t refcnt = BitbucketGetInodeReferenceCount(Inode);
        uint64_t reasoncount = BitbucketGetInodeReasonReferenceCount(Inode, Reason);

        fuse_log(FUSE_LOG_DEBUG, 
                "Finesse: Remove reference to inode %ld reason %d (%s) (ref: %lu -> %lu, reason: %lu -> %lu)\n", 
                bbpi->PublicInode.Attributes.st_ino, 
                Reason,
                InodePrivateObjectAttributes.ReferenceReasonsNames[Reason],
                refcnt, refcnt - 1,
                reasoncount, reasoncount - 1);
    }
    
    BitbucketObjectDereference(bbpi, Reason, Bias);
    bbpi = NULL;
}

// Return the reference count for the given Inode object
uint64_t BitbucketGetInodeReferenceCount(bitbucket_inode_t *Inode)
{
    bitbucket_private_inode_t *bbpi = container_of(Inode, bitbucket_private_inode_t, PublicInode);
    CHECK_BITBUCKET_PRIVATE_INODE_MAGIC(bbpi);

    return BitbucketGetObjectReferenceCount(bbpi);
}

uint64_t BitbucketGetInodeReasonReferenceCount(bitbucket_inode_t *Inode, uint8_t Reason)
{
    bitbucket_private_inode_t *bbpi = container_of(Inode, bitbucket_private_inode_t, PublicInode);
    CHECK_BITBUCKET_PRIVATE_INODE_MAGIC(bbpi);

    return BitbucketGetObjectReasonReferenceCount(bbpi, Reason);
}

int BitbucketTryLockInode(bitbucket_inode_t *Inode, int Exclusive)
{
    int status = 0;
    assert(NULL != Inode);
    CHECK_BITBUCKET_INODE_MAGIC(Inode);

    if (Exclusive) {
        status = pthread_rwlock_trywrlock(&Inode->InodeLock);
        Inode->Epoch++;
    }
    else {
        status = pthread_rwlock_tryrdlock(&Inode->InodeLock);
    }

    return status;
}

//
// Lock two different inodes in a canonical order
//
// Locks two inodes in a pre-defined order to avoid
// trivial (obvious) deadlocks.
//
// Note that upon return, both inodes have been locked.
// They may be unlocked in any order; the caller should
// NOT lock any additional inodes until both locks have
// been released (otherwise it may deadlock).
//
// This call may block
//
// @param Inode1 - the first inode to lock
// @param Inode2 - the second inode to lock
// @Exclusive - 0 if this is shared (read) 1 if this is exclusive (write)
//
void BitbucketLockTwoInodes(bitbucket_inode_t *Inode1, bitbucket_inode_t *Inode2, int Exclusive)
{
    bitbucket_inode_t *first = NULL;
    bitbucket_inode_t *second = NULL;

    assert(NULL != Inode1);
    assert(NULL != Inode2);
    assert(Inode1 != Inode2); // can't be the same inode!

    //
    // Lock hierarchy:
    //   - Table lock (not an inode lock)
    //   - Dir lock
    //   - File lock
    //   - Symlink lock
    //   - Devnode
    _Static_assert(BITBUCKET_DIR_TYPE < BITBUCKET_FILE_TYPE, "locking relies upon type value");
    _Static_assert(BITBUCKET_FILE_TYPE < BITBUCKET_SYMLINK_TYPE, "locking relies upon type value");
    _Static_assert(BITBUCKET_SYMLINK_TYPE < BITBUCKET_DEVNODE_TYPE, "locking relies upon type value");
    _Static_assert(BITBUCKET_SYMLINK_TYPE < BITBUCKET_UNKNOWN_TYPE, "locking relies upon type value");
    assert((BITBUCKET_DIR_TYPE == Inode1->InodeType) ||
           (BITBUCKET_FILE_TYPE == Inode1->InodeType) ||
           (BITBUCKET_SYMLINK_TYPE == Inode1->InodeType) ||
           (BITBUCKET_DEVNODE_TYPE == Inode1->InodeType));

    if (Inode1->InodeType < Inode2->InodeType) {
        first = Inode1;
        second = Inode2;
    }
    else {
        if ((uintptr_t)Inode1 < (uintptr_t)Inode2) {
            first = Inode2;
            second = Inode1;
        }
        else {
            first = Inode1;
            second = Inode2;
        }
    }

    BitbucketLockInode(first, Exclusive);
    BitbucketLockInode(second, Exclusive);

}


void BitbucketLockInode(bitbucket_inode_t *Inode, int Exclusive)
{
    int status = 0;

    assert(NULL != Inode);
    CHECK_BITBUCKET_INODE_MAGIC(Inode);

    if (Exclusive) {
        status = pthread_rwlock_wrlock(&Inode->InodeLock);
        assert(0 == status);
        Inode->Epoch++;
    }
    else {
        status = pthread_rwlock_rdlock(&Inode->InodeLock);
        assert(0 == status);
    }
}

void BitbucketUnlockInode(bitbucket_inode_t *Inode)
{
    int status;
    int exclusive = 0;
    assert(NULL != Inode);
    CHECK_BITBUCKET_INODE_MAGIC(Inode);

    status = pthread_rwlock_tryrdlock(&Inode->InodeLock);

    switch(status) {
        case 0:
            // the lock was acquired so it must be shared
            pthread_rwlock_unlock(&Inode->InodeLock); // this reverses our successful trylock
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
        // We assume inode attributes changed.  Data changes
        // must be handled in paths where the data itself could change.
        status = clock_gettime(CLOCK_TAI, &Inode->Attributes.st_ctim);
        assert(0 == status);
    }
    // Now to accomplish the original goal!
    pthread_rwlock_unlock(&Inode->InodeLock);

}
