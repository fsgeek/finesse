//
// (C) Copyright 2020 Tony Mason (fsgeek@cs.ubc.ca)
// All Rights Reserved
//

#if !defined(_BITBUCKET_DATA_H_)
#define _BITBUCKET_DATA_H_ (1)

#if !defined(FUSE_USE_VERSION)
#define FUSE_USE_VERSION 39
#endif // FUSE_USE_VERSION

#include <fuse_lowlevel.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include "trie.h"
#include "finesse-list.h"

// Arbitrary limits of what we will support.
#define MAX_FILE_NAME_SIZE (1024)
#define MAX_PATH_NAME_SIZE (65536)

// Common data structures used within the bitbucket file system.
static inline void verify_magic(const char *StructureName, const char *File, const char *Function, unsigned Line, uint64_t Magic, uint64_t CheckMagic)
{
    if (Magic != CheckMagic) {
        fprintf(stderr, "%s (%s:%d) Magic number mismatch (%lx != %lx) for %s\n", Function, File, Line, Magic, CheckMagic, StructureName);
        assert(Magic == CheckMagic);
    }
}

typedef struct _bitbucket_inode bitbucket_inode_t;


typedef struct _bitbucket_file {
    uint64_t            Magic; // magic number
    pthread_rwlock_t    Lock;
    uuid_t              FileId;
    char                FileIdName[40];
    struct stat         Attributes;
} bitbucket_file_t;

#define BITBUCKET_FILE_MAGIC (0x901bb9acacca7b19)

#define CHECK_BITBUCKET_FILE_MAGIC(bbf) verify_magic("bitbucket_file_t", __FILE__, __func__, __LINE__, BITBUCKET_FILE_MAGIC, (bbf)->Magic)

typedef struct _bitbucket_dir {
    uint64_t              Magic; // magic number
    uuid_t                DirId;
    char                  DirIdName[40];
    bitbucket_inode_t    *Parent;
    list_entry_t          Entries;
    struct Trie          *Children;
    list_entry_t          EAs;
    struct Trie          *ExtendedAttributes;
    uint64_t              Epoch; // increment each time the directory changes shape (add/remove entries)
} bitbucket_dir_t;

#define BITBUCKET_DIR_MAGIC (0x895fe26d657f24bd)
#define CHECK_BITBUCKET_DIR_MAGIC(bbd) verify_magic("bitbucket_dir_t", __FILE__, __func__, __LINE__, BITBUCKET_DIR_MAGIC, (bbd)->Magic)

typedef struct _bitbucket_inode_table bitbucket_inode_table_t;
#define BITBUCKET_INODE_TABLE_BUCKETS (1024)


typedef struct _bitbucket_userdata {
    uint64_t            Magic;
    uint8_t             Debug;
    uint8_t             Unused[7]; // For storing additional options!
    bitbucket_inode_t  *RootDirectory;
    void               *InodeTable;
    double              AttrTimeout; // Arbitrary for now.
} bitbucket_user_data_t;

#define BITBUCKET_USER_DATA_MAGIC 0xb2035aef09927b87  
#define CHECK_BITBUCKET_USER_DATA_MAGIC(bbud) verify_magic("bitbucket_userdata_t", __FILE__, __func__, __LINE__, BITBUCKET_USER_DATA_MAGIC, (bbud)->Magic)

int BitbucketInsertInodeInTable(void *Table, bitbucket_inode_t *Inode);
void BitbucketRemoveInodeFromTable(bitbucket_inode_t *Inode);
bitbucket_inode_t *BitbucketLookupInodeInTable(void *Table, ino_t Inode);
void *BitbucketCreateInodeTable(uint16_t BucketCount);
void BitbucketDestroyInodeTable(void *Table);

#define BITBUCKET_FILE_TYPE (0x10)
#define BITBUCKET_DIR_TYPE  (0x11)
#define BITBUCKET_SYMLINK_TYPE (0x12)
#define BITBUCKET_DEVNODE_TYPE (0x13)
#define BITBUCKET_UNKNOWN_TYPE (0xFF)


//
// This allows coordination with the specialization component
//   * Initialize - this is called with the specialized portion of the object for initialization
//   * Deallocate - this is called when the reference count drops to zero.  Note that it is called with the lock held.
//                  The lock is assumed to be released (and likely destroyed) upon return.
//   * Lock - this is called whenever the reference count is changing; shared or exclusive.
//   * Unlock - this is called whenever the reference count change is complete
// The lock/unlock operation(s) are optional.  If they are not provided, the object package will
// use an internal default lock
//
#define BITBUCKET_MAX_REFERENCE_REASONS (8)
#define BITBUCKET_MAX_REFERENCE_REASON_NAME_LENGTH (32)
typedef struct _bitbucket_object_attributes {
    uint64_t            Magic;
    uint8_t             ReasonCount;
    char                ReferenceReasonsNames[BITBUCKET_MAX_REFERENCE_REASONS][BITBUCKET_MAX_REFERENCE_REASON_NAME_LENGTH+1];
    void              (*Initialize)(void *Object, size_t Length); //
    void              (*Deallocate)(void *Object, size_t Length); // Call this when the reference count drops to zero
    void              (*Lock)(void *Object, int Exclusive);
    void              (*Unlock)(void *Object);
} bitbucket_object_attributes_t;

#define BITBUCKET_OBJECT_ATTRIBUTES_MAGIC (0x1b126261e6db52cd)
#define CHECK_BITBUCKET_OBJECT_ATTRIBUTES_MAGIC(bboa) verify_magic("bitbucket_object_attributes_t", __FILE__, __func__, __LINE__, BITBUCKET_OBJECT_ATTRIBUTES_MAGIC, (bboa)->Magic)


typedef struct _bitbucket_symlink {
    uint64_t    Magic;
    char        LinkContents[1];
} bitbucket_symlink_t;

#define BITBUCKET_SYMLINK_MAGIC (0x6fba60a633f4e259)
#define CHECK_BITBUCKET_SYMLINK_MAGIC(bbsl) verify_magic("bitbucket_symlink_t", __FILE__, __func__, __LINE__, BITBUCKET_SYMLINK_MAGIC, (bbsl)->Magic)

typedef struct _bitbucket_devnode {
    uint64_t    Magic;
    dev_t       Device;
    char        Data[1];
} bitbucket_devnode_t;

#define BITBUCKET_DEVNODE_MAGIC (0x08cb9f938b09ddfd)
#define CHECK_BITBUCKET_DEVNODE_MAGIC(bbdn) verify_magic("bitbucket_devnode_t", __FILE__, __func__, __LINE__, BITBUCKET_DEVNODE_MAGIC, (bbdn)->Magic)

typedef struct _bitbucket_inode {
    uint64_t                        Magic;
    size_t                          InodeLength;
    uint8_t                         InodeType;
    pthread_rwlock_t                InodeLock;
    bitbucket_inode_table_t         *Table; // if not null, inode is inserted in an inode table
    uuid_t                          Uuid;
    char                            UuidString[40];
    struct stat                     Attributes;
    struct timeval                  AccessTime; // last time anyone accessed this file
    struct timeval                  ModifiedTime; // last time anyone changed the _contents_ of this file
    struct timeval                  CreationTime; // when this file was (first) created
    struct timeval                  ChangeTime; // last time _attributes_ of this file changed (including other timestamps)
    union {
        bitbucket_dir_t             Directory;
        bitbucket_file_t            File;
        bitbucket_symlink_t         SymbolicLink;
        bitbucket_devnode_t         DeviceNode;
    } Instance;
} bitbucket_inode_t;

#define BITBUCKET_INODE_MAGIC (0x3eb0674fe159eab4)
#define CHECK_BITBUCKET_INODE_MAGIC(bbi) verify_magic("bitbucket_inode_t", __FILE__, __func__, __LINE__, BITBUCKET_INODE_MAGIC, (bbi)->Magic)

#define INODE_TABLE_REFERENCE    (0)
#define INODE_LOOKUP_REFERENCE   (1)
#define INODE_PARENT_REFERENCE   (2)
#define INODE_DIRENT_REFERENCE   (3)

typedef struct _bitbucket_dir_entry {
    uint64_t            Magic;
    list_entry_t        ListEntry;
    bitbucket_inode_t  *Inode;
    uint64_t            Offset;
    char                Name[1];
} bitbucket_dir_entry_t;

#define BITBUCKET_DIR_ENTRY_MAGIC (0xdb88e347869a9599)
#define CHECK_BITBUCKET_DIR_ENTRY_MAGIC(de) verify_magic("bitbucket_dir_entry_t", __FILE__, __func__, __LINE__, BITBUCKET_DIR_MAGIC, (de)->Magic)


typedef struct _bitbucket_dir_enum_context {
    uint64_t               Magic;
    bitbucket_inode_t     *Directory;
    bitbucket_dir_entry_t *NextEntry;
    uint64_t               Offset;
    uint64_t               Epoch;
    int                    LastError;
} bitbucket_dir_enum_context_t;

#define BITBUCKET_DIR_ENUM_CONTEXT_MAGIC (0xb058a92686f1e02d)
#define CHECK_BITBUCKET_DIR_ENUM_CONTEXT_MAGIC(dec) verify_magic("bitbucket_dir_enum_context_t", __FILE__, __func__, __LINE__, BITBUCKET_DIR_ENUM_CONTEXT_MAGIC, (dec)->Magic)

void BitbucketInitalizeDirectoryEnumerationContext(bitbucket_dir_enum_context_t *EnumerationContext, bitbucket_inode_t *Directory);
const bitbucket_dir_entry_t *BitbucketEnumerateDirectory(bitbucket_dir_enum_context_t *EnumerationContext);
int BitbucketSeekDirectory(bitbucket_dir_enum_context_t *EnumerationContext, uint64_t Offset);
void BitbucketLockDirectory(bitbucket_inode_t *Directory, int Exclusive);
void BitbucketUnlockDirectory(bitbucket_inode_t *Directory);


// Create a reference counted object;  On return the region returned will be at least
// ObjectSize bytes long and may be used as the caller sees fit.
//
// Note: the callbacks registered are invoked as needed: 
//   * initialize (set up this object)
//   * deallocate (prepare this object for deletion) - called with the lock held
//   * lock (synchronize access to this object)
//   * unlock (release this object)
//
// Default values are provided if NULL is passed
//
// Upon return, this object has a reference count of 1.
void *BitbucketObjectCreate(bitbucket_object_attributes_t *ObjectAttributes, size_t ObjectSize, uint8_t InitialReason);

// Increment the reference count on this object
void BitbucketObjectReference(void *Object, uint8_t Reason);

// Decrement the reference count on this object; if it drops to zero,
// call the deallocate callback and then delete the object.
// Note: this is done in a thread-safe fashion, provided that the
// object owner is locking it prior to lookup and reference counting it.
void BitbucketObjectDereference(void *Object, uint8_t Reason);

// Return the number of objects outstanding in the system
uint64_t BitbucketObjectCount(void);

// Return the reference count of the given object
uint64_t BitbucketGetObjectReferenceCount(void *Object);


bitbucket_inode_t *BitbucketCreateInode(bitbucket_inode_table_t *Table, bitbucket_object_attributes_t *ObjectAttributes, size_t DataLength);


// Given an inode, insert it into the directory with the specified name.
int BitbucketInsertDirectoryEntry(bitbucket_inode_t *DirInode, bitbucket_inode_t *Inode, const char *Name);

// Given a directory, search for the name; if found, return a (referenced) pointer to the inode
void BitbucketLookupObjectInDirectory(bitbucket_inode_t *Inode, const char *Name, bitbucket_inode_t **Object);

// Given an inode, remove it from the specified entry.
// Return ENOENT if the entry isn't found.
int BitbucketDeleteDirectoryEntry(bitbucket_inode_t *Directory, const char *Name);


void BitbucketReferenceInode(bitbucket_inode_t *Inode, uint8_t Reason);
void BitbucketDereferenceInode(bitbucket_inode_t *Inode, uint8_t Reason);
uint64_t BitbucketGetInodeReferenceCount(bitbucket_inode_t *Inode);
void BitbucketGetObjectReasonReferenceCounts(void *Object, uint32_t *Counts, uint8_t CountEntries);
const char *BitbucketGetObjectReasonName(void *Object, uint8_t Reason);


// More random numbers
// 
// fe 15 bc e7 d8 48 e1 c8 
// d5 a7 31 20 77 dc 4c 89
// a9 e1 65 46 9a 58 d6 0c  
//
// Source: https://www.random.org/cgi-bin/randbyte?nbytes=16&format=h

#endif // _BITBUCKET_DATA_H_
