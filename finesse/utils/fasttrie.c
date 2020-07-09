/*
 * Copyright (c) 2020 Tony Mason
 * All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <crc.h>
#include <errno.h>
#include <murmurhash3.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A bit of randomness...
//
//  18 1e c6 eb f4 fe 13 d5
//
//

#define TRIE_NODE_COUNT (256)

static inline void verify_magic(const char *StructureName, const char *File, const char *Function, unsigned Line, uint64_t Magic,
                                uint64_t CheckMagic)
{
    if (Magic != CheckMagic) {
        fprintf(stderr, "%s (%s:%d) Magic number mismatch (%lx != %lx) for %s\n", Function, File, Line, Magic, CheckMagic,
                StructureName);
        assert(Magic == CheckMagic);
    }
}

typedef struct _fast_trie_node {
    uint64_t                Magic;  // sanity check value
    void *                  Prefix;
    size_t                  CurrentPrefixLength;  // Active prefix length
    size_t                  MaximumPrefixLength;  // space available for a prefix
    void *                  Object;               // If this location is occupied (e.g., there is something with this prefix)
    uint64_t                NodeCount;
    uint64_t                LastHashValue;
    struct _fast_trie_node *Nodes[TRIE_NODE_COUNT];  // Pointers to longer prefixes; prefixes are sorted
} fast_trie_node_t;

#define FAST_TRIE_NODE_MAGIC (0x0883596059ad298b)
#define CHECK_FAST_TRIE_NODE_MAGIC(ftn) \
    verify_magic("fast_trie_node_t", __FILE__, __func__, __LINE__, FAST_TRIE_NODE_MAGIC, (ftn)->Magic)

typedef struct _fast_trie fast_trie_t;
typedef uint64_t (*TrieDataStructureHash)(fast_trie_t *Trie);

struct _fast_trie {
    uint64_t              Magic;
    pthread_rwlock_t      Lock;
    TrieDataStructureHash HashFunction;
    uint64_t              LastHashValue;
    uint64_t              EntryCount;
    fast_trie_node_t *    Node;
};

#define FAST_TRIE_TABLE_MAGIC (0xd8201a3421222f70)
#define CHECK_FAST_TRIE_TABLE_MAGIC(ftt) \
    verify_magic("fast_trie_t", __FILE__, __func__, __LINE__, FAST_TRIE_TABLE_MAGIC, (ftt)->Magic)

void         FastTrieInsert(fast_trie_t *Trie, void *Key, size_t KeyLength, void *Object);
void *       FastTrieLookup(fast_trie_t *Trie, void *Key, size_t KeyLength);
void *       FastTrieRemove(fast_trie_t *Trie, void *Key, size_t KeyLength);
uint64_t     FastTrieGetEntryCount(fast_trie_t *Trie);
fast_trie_t *FastTrieCreate(void);

//
// This routine acquires the Trie lock and then computes the current
// checksum over the Trie data.
//
static inline uint64_t LockTrie(fast_trie_t *Trie, int Exclusive)
{
    if (0 == Exclusive) {
        pthread_rwlock_rdlock(&Trie->Lock);
    }
    else {
        pthread_rwlock_wrlock(&Trie->Lock);
    }
    if (NULL != Trie->HashFunction) {
        return Trie->HashFunction(Trie);
    }
    return 0;
}

static inline void UnlockTrie(fast_trie_t *Trie, uint64_t CurrentHashValue)
{
    int exclusive = 1;
    int status;

    status = pthread_rwlock_trywrlock(&Trie->Lock);
    assert(0 != status);

    status = pthread_rwlock_tryrdlock(&Trie->Lock);
    if (-1 == status) {
        status = errno;
    }

    switch (status) {
        case 0:
            // the lock was acquired so it must be shared
            pthread_rwlock_unlock(&Trie->Lock);  // this reverses our successful trylock
            break;
        case EDEADLK:  // this thread owns the lock exclusive
        case EBUSY:    // SOME thread owns the lock exclusive (hopefully it's this thread!)
            exclusive = 1;
            break;
        case EINVAL:
            assert(0);  // this lock isn't initialized
            break;
        case EAGAIN:
            // the lock is acquired shared by someone (e.g., this thread)
            break;
        default:
            assert(0);  // not sure what this means
    }

    if (exclusive) {
        Trie->LastHashValue = CurrentHashValue;
    }
    else {
        assert(Trie->LastHashValue == CurrentHashValue);
    }

    pthread_rwlock_unlock(&Trie->Lock);
}

static uint64_t HashTrie(fast_trie_t *Trie)
{
    uint64_t mh[2];
    uint64_t hash;

    MurmurHash3_x64_128(&Trie->EntryCount, sizeof(fast_trie_t) - offsetof(fast_trie_t, EntryCount), 0xd8201a34, &mh);
    hash = mh[0] ^ mh[1];  // mix them
    if (0 == hash) {
        hash = ~0;  // so we can use 0 as "not valid"
    }

    return hash;
}

static uint64_t HashTrieNode(fast_trie_node_t *TrieNode)
{
    uint64_t mh[2];
    uint64_t hash = 0;

    // Don't use the "last hash value" field in the hash computation
    MurmurHash3_x64_128(TrieNode, offsetof(fast_trie_node_t, LastHashValue), 0x21222f70, mh);
    hash = mh[0] ^ mh[1];  // mix them

    MurmurHash3_x64_128(TrieNode->Nodes, sizeof(fast_trie_node_t) - offsetof(fast_trie_node_t, Nodes), 0x21222f70, mh);
    hash ^= mh[0];
    hash ^= mh[1];

    if (0 == hash) {
        hash = ~0;  // so we can use 0 as "not valid"
    }
    return hash;
}

fast_trie_t *FastTrieCreate(void)
{
    fast_trie_t *Trie = (fast_trie_t *)malloc(sizeof(fast_trie_t));

    if (NULL != Trie) {
        Trie->Magic = FAST_TRIE_TABLE_MAGIC;
        pthread_rwlock_init(&Trie->Lock, NULL);
        Trie->LastHashValue = 0;
        Trie->HashFunction  = HashTrie;
        Trie->EntryCount    = 0;
        Trie->Node          = NULL;
        Trie->LastHashValue = HashTrie(Trie);
    }

    return Trie;
}

static fast_trie_node_t *CreateTrieNode(size_t PrefixLength)
{
    size_t            size = sizeof(fast_trie_node_t) + PrefixLength;
    fast_trie_node_t *tn;

    assert(PrefixLength > 0);

    size += 63;  // round up to next 64 bytes
    size &= ~63;
    assert(sizeof(fast_trie_node_t) + PrefixLength <= size);

    tn = malloc(size);
    if (NULL != tn) {
        tn->Magic               = FAST_TRIE_NODE_MAGIC;
        tn->Prefix              = (void *)((uintptr_t)tn + sizeof(fast_trie_node_t));
        tn->CurrentPrefixLength = 0;
        tn->MaximumPrefixLength = PrefixLength;
        tn->Object              = NULL;
        tn->NodeCount           = 0;
        memset(tn->Nodes, 0, sizeof(tn->Nodes));
        tn->LastHashValue = HashTrieNode(tn);
    }

    return tn;
}

//
// This function looks at entries from start to End to see if any can be merged to make space; if so,
// one new entry is created and inserted into the table, and the previous entries are pushed into the
// new entry.  Thus, space is made in the TrieNode.
//
// This routine returns the number of entries that were freed (e.g., 0 means "did not free any")
static int TrieMakeSpaceForEntries(fast_trie_node_t *TrieNode, uint8_t Start, uint8_t End, size_t Bias)
{
    uint8_t           index;
    uint8_t           first, last;
    uint8_t           remaining = 0;
    size_t            len;
    int               cmp;
    int               merged      = 0;
    fast_trie_node_t *newTrieNode = NULL;

    assert(Bias > 0);  // if a bias of 0 works, it's a bug because it means two adjacent nodes have a prefix overlap

    for (index = Start; index + 1 < End; index++) {
        first = index;
        last  = index + 1;

        len = TrieNode->Nodes[first]->CurrentPrefixLength;
        if (len < TrieNode->Nodes[last]->CurrentPrefixLength) {
            len = TrieNode->Nodes[last]->CurrentPrefixLength;
        }
        if (len <= Bias) {
            // Don't consider these two: I want better matches
            continue;
        }
        len -= Bias;  // Our goal is to find the longest substring match

        cmp = memcmp(TrieNode->Nodes[first]->Prefix, TrieNode->Nodes[last]->Prefix, len);

        if (0 != cmp) {  // These two were not mergeable
            continue;
        }

        // These two can be merged.  We need to see if any other adjacent entries can be
        // merged as well (e.g., they have a common prefix);
        for (uint8_t index2 = first; index2 > 0;) {
            index2--;  // look at the prior entry
            if (len > TrieNode->Nodes[index2]->CurrentPrefixLength) {
                // not interesting; if they were common, they wouldn't be
                // adjacent like this.
                break;
            }

            if (0 != memcmp(TrieNode->Nodes[index2]->Prefix, TrieNode->Nodes[index2 - 1]->Prefix, len)) {
                // This one did not match, so none of those below it will match either.
                break;
            }

            // This matches, so we should move this entry as well.  Let's move our starting spot down.
            first = index2;
        }

        // Now let's look UP in the list
        for (uint8_t index2 = last; index2 < TrieNode->NodeCount;) {
            index2++;

            if (len > TrieNode->Nodes[index2]->CurrentPrefixLength) {
                // not interesting; if they were common, they wouldn't be
                // adjacent like this.
                break;
            }

            if (0 != memcmp(TrieNode->Nodes[index2]->Prefix, TrieNode->Nodes[index2 - 1]->Prefix, len)) {
                // This one did not match, so none of those above it will match either.
                break;
            }

            last = index2;
        }

        merged = last - first;

        // These elements
        newTrieNode = CreateTrieNode(len);
        assert(NULL != newTrieNode);

        // Use the common prefix in the new node
        memcpy(newTrieNode->Prefix, TrieNode->Nodes[first]->Prefix, len);

        for (uint8_t i = 0; i <= last - first; i++) {
            // Move nodes from their current location into the new location
            newTrieNode->Nodes[i] = TrieNode->Nodes[first + i];

            // Adjust the length of the prefix (since the common part is now in the parent)
            assert(newTrieNode->Nodes[i]->CurrentPrefixLength > len);  // otherwise, logic bug
            newTrieNode->Nodes[i]->CurrentPrefixLength -= len;
        }

        // Now let's reclaim this space
        assert(first < last);
        TrieNode->Nodes[first] = newTrieNode;
        first++;  // We don't want to overwrite the entry we just used

        // We need to move everything following last to the end of the list into
        // the first index (which we just moved forward).

        // [250, first, 252, 253, last, 255]
        // [250, <new>, first = 252, 253, last, 255]
        // [250, <new>, 255]

        assert(last < TrieNode->NodeCount);
        remaining = TrieNode->NodeCount - (last + 1);
        if (remaining > 0) {
            memmove(&TrieNode->Nodes[first], &TrieNode->Nodes[last + 1], sizeof(fast_trie_node_t *) * remaining);
        }

        // Adjust the number of nodes
        TrieNode->NodeCount -= merged;

        // Clear the unused entries
        memset(&TrieNode->Nodes[TrieNode->NodeCount], 0, sizeof(fast_trie_node_t *) * (TRIE_NODE_COUNT - TrieNode->NodeCount));
    }

    return merged;
}

static void TrieMakeSpace(fast_trie_node_t *TrieNode)
{
    size_t  len;
    size_t  bias   = 0;
    int     merged = 0;
    uint8_t rnd    = random() % TRIE_NODE_COUNT;

    assert(NULL != TrieNode);
    CHECK_FAST_TRIE_NODE_MAGIC(TrieNode);
    assert(TRIE_NODE_COUNT == TrieNode->NodeCount + 1);  // if not, we shouldn't be calling this function.

    len = 0;
    for (unsigned index = 0; index < TrieNode->NodeCount; index++) {
        if (len < TrieNode->Nodes[index]->CurrentPrefixLength) {
            len = TrieNode->Nodes[index]->CurrentPrefixLength;
        }
    }

    assert(len > 1);  // otherwise there's a logic bug

    // we randomly pick a location from which to start looking.

    for (bias = 1; !merged; bias++) {
        assert(bias < len);  // otherwise there's a logic bug

        merged = TrieMakeSpaceForEntries(TrieNode, rnd, TrieNode->NodeCount - 1, bias);

        if (merged > 0) {
            // we made space
            break;
        }

        merged = TrieMakeSpaceForEntries(TrieNode, 0, rnd - 1, bias);

        if (merged > 0) {
            // we made space;
            break;
        }
    }
}

// local function that inserts this Key into the Trie starting from the given Node
static void TrieInsert(fast_trie_node_t *TrieNode, void *Key, size_t KeyLength, void *Object)
{
    uint8_t   start, end, current;  // we only have 256 entries
    size_t    len = KeyLength;
    int       cmp = 0;
    uintptr_t key;

    assert(NULL != TrieNode);
    assert(NULL != Key);
    assert(KeyLength > 0);
    assert(NULL != Object);
    CHECK_FAST_TRIE_NODE_MAGIC(TrieNode);

    // Cases:
    //  (1) This key matches this node and there is no more left to compare (Collision)
    //  (2) This key matches this node and there is more left to compare (node is a prefix)
    //
    // For case (2), I need to search the (ordered) list of prefixes until I find:
    //     (a) A match for more of this Key
    //     (b) The correct place in which I am going to insert the new prefix
    //
    // (b) is the challenge, because I might have run out of nodes, in which case
    //     I have to split my nodes.  The size I picked (256) was done because
    //     the *worst case* is consuming a single byte and fanning out on that.

    while (1) {
        // Step 1: binary search for the correct position in the Trie
        start   = 0;
        end     = TrieNode->NodeCount - 1;
        current = end - start / 2;

        for (current = end - start / 2; start < current && current < end; current = (end - start) / 2) {
            if (KeyLength > TrieNode->Nodes[current]->CurrentPrefixLength) {
                len = TrieNode->Nodes[current]->CurrentPrefixLength;  // can't compare more than this amount.
            }
            else {
                len = KeyLength;
            }

            cmp = memcmp(TrieNode->Nodes[current]->Prefix, Key, len);

            if (0 == cmp) {
                break;
            }

            if (cmp < 0) {
                // Prefix is less than key, so we need to search higher in the list
                start = current;
                continue;
            }

            // Prefix is greater than key, so we need to search lower in the list
            end = current;
        }

        if (0 == cmp) {
            // We either have a sub-prefix or a collision
            assert(TrieNode->CurrentPrefixLength != KeyLength);  // collision
            key = (uintptr_t)Key + len;                          // skip over common prefix part
            TrieInsert(TrieNode->Nodes[current], (void *)key, KeyLength - len, Object);
            // We're actually done at this point - when this returns, the entry
            // is inserted
            break;
        }

        // We didn't find a prefix match, so we need to insert this entry into the table
        // if there is space.

        if (TRIE_NODE_COUNT == TrieNode->NodeCount + 1) {
            // We don't have space, so it means we have to shuffle this table
            TrieMakeSpace(TrieNode);
            assert(TRIE_NODE_COUNT > TrieNode->NodeCount + 1);  // now there's space

            // Since we've changed the table shape, we need to
            // start over.
            continue;
        }

        // We have space, so we need to insert the node into it's correct home
        memmove(&TrieNode->Nodes[current + 2], &TrieNode->Nodes[current + 1], TrieNode->NodeCount - current);
        TrieNode->Nodes[current] = CreateTrieNode(KeyLength);
        memcpy(TrieNode->Nodes[current]->Prefix, Key, KeyLength);
        TrieNode->Nodes[current]->CurrentPrefixLength = KeyLength;
        TrieNode->Nodes[current]->Object              = Object;
        TrieNode->NodeCount++;
        break;
    }
}

//
// Routine to insert an entry into the trie.
//
// Trie - This is a pointer to a pointer where the Trie is stored; if NULL, a new
//         Trie is created and the initial key added.
// Key -  This is a pointer to the key being inserted.  Note that it is a binary key
// KeyLength - This is the length of Key (in bytes)
// Object - This is the abstract pointer stored by the Trie package.
//
void FastTrieInsert(fast_trie_t *Trie, void *Key, size_t KeyLength, void *Object)
{
    assert(NULL != Trie);
    CHECK_FAST_TRIE_TABLE_MAGIC(Trie);
    assert(NULL != Key);
    assert(0 != KeyLength);
    assert(NULL != Object);

    LockTrie(Trie, 1);
    while (1) {
        if (Trie->Node == NULL) {
            Trie->Node = CreateTrieNode(KeyLength);
            assert(NULL != Trie->Node);
            assert(KeyLength <= Trie->Node->MaximumPrefixLength);  // if not, we have an issue
            memcpy(Trie->Node->Prefix, Key, KeyLength);
            Trie->Node->CurrentPrefixLength = KeyLength;
            Trie->Node->Object              = Object;
            break;
        }

        TrieInsert(Trie->Node, Key, KeyLength, Object);
        break;
    }
    UnlockTrie(Trie, HashTrie(Trie));
}

void *FastTrieLookup(fast_trie_t *Trie, void *Key, size_t KeyLength)
{
    (void)Trie;
    (void)Key;
    (void)KeyLength;

    return NULL;
}

static void *FastTrieNodeRemove(fast_trie_node_t *Trie, void *Key, size_t KeyLength)
{
    (void)Trie;
    (void)Key;
    (void)KeyLength;
    assert(0);

    return NULL;
}

void *FastTrieRemove(fast_trie_t *Trie, void *Key, size_t KeyLength)
{
    void *object = NULL;

    assert(NULL != Trie);
    assert(NULL != Key);
    assert(KeyLength > 0);

    LockTrie(Trie, 1);
    object = FastTrieNodeRemove(Trie->Node, Key, KeyLength);
    UnlockTrie(Trie, HashTrie(Trie));
    return object;
}

uint64_t FastTrieGetEntryCount(fast_trie_t *Trie)
{
    uint64_t count;

    LockTrie(Trie, 0);
    count = Trie->EntryCount;
    UnlockTrie(Trie, Trie->LastHashValue);

    return count;
}
