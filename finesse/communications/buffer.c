
#include <assert.h>
#include <malloc.h>
#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "fcinternal.h"

/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

//
// The intent of this package is to provide a (very) simple buffer allocator.  To set it up, you specify:
//  (1) the number of buffers you want to have available (N)
//  (2) the size of said buffers (S)
//
// To use this package then you either:
//  (i) Hand it an existing blob of somethign to map; or
//  (ii) Nothing, in which case it creates it for you.
//
// Thus, the APIs for this module are:
//  * Create the arena (with size/number)
//  * Create the arena (with a name)
//  (Both these APIs hand back a "handle")
//
//  * Allocate Buffer
//  * Free Buffer
//  (both take a handle, the free also takes the buffer or an index)
//
//  * Get Name (of the arena)
//  * Get Size (of the slabs in the arena)
//  * Get number of buffers in the arena (probably not necessary)
//
// Note that if there are no free buffers, allocations will fail, not block
//

#if !defined(make_mask64)
#define make_mask64(index) (((u_int64_t)1) << index)
#endif

static const char Signature[16] = {'F', 'i', 'n', 'c', 'o', 'm', ' ', 'A', 'r', 'e', 'n', 'a', '\0'};

typedef struct _fincomm_arena_info_t {
    char     Signature[16];
    char     Name[1024];
    size_t   Size;              // size of each region
    size_t   Count;             // count of each region
    size_t   Offset;            // start of the arena from start of file
    uint64_t AllocationBitmap;  // The map of what has been allocated
    unsigned LastAllocated;     // This is just a hint on where to start looking
} fincomm_arena_info_t;

struct _fincomm_arena_handle {
    char                  Signature[sizeof(Signature)];
    fincomm_arena_info_t *Arena;
    int                   FileDescriptor;
};

#define ARENA_CONTROL_AREA_SIZE (4096)

// declarations to be moved to a header file
//
struct _fincomm_arena_handle;
typedef struct _fincomm_arena_handle *fincomm_arena_handle_t;

fincomm_arena_handle_t FincommCreateArena(char *Name, size_t BufferSize, size_t Count);
void *                 FincommAllocateBuffer(fincomm_arena_handle_t ArenaHandle);
void                   FincommFreeBuffer(fincomm_arena_handle_t ArenaHandle, void *Buffer);
void  FincommGetArenaInfo(fincomm_arena_handle_t Handle, char *Name, size_t NameSize, size_t *BufferSize, size_t *Count);
off_t FincommGetBufferOffset(fincomm_arena_handle_t Handle, void *Buffer);

fincomm_arena_handle_t FincommCreateArena(char *Name, size_t BufferSize, size_t Count)
{
    size_t                ArenaSize;
    fincomm_arena_info_t *arena_info   = NULL;
    fincomm_arena_handle *arena_handle = malloc(sizeof(fincomm_arena_handle));
    int                   code;
    struct stat           statbuf;

    assert(NULL != arena_handle);
    assert(NULL != Name);

    if (0 == BufferSize) {
        BufferSize = ((uint64_t)64) * 1024 * 1024;  // 64MB
    }

    if (0 == Count) {
        Count = 64;
    }

    memcpy(arena_handle->Signature, Signature, sizeof(Signature));

    ArenaSize = ARENA_CONTROL_AREA_SIZE + (BufferSize * Count);  // one page header + all the data you could want

    assert(ArenaSize > ARENA_CONTROL_AREA_SIZE);  // sanity
    assert(Count <= 64);                          // because I'm lazy and only going to support a single 64 bit vector (again)
    assert(strlen(Name) < sizeof(arena_info->Name));

    // Create anonymous region
    arena_handle->FileDescriptor = memfd_create(Name, 0);
    assert(arena_handle->FileDescriptor >= 0);

    code = fstat(arena_handle->FileDescriptor, &statbuf);
    assert(code >= 0);

    if (statbuf.st_size > 0) {
        // Must be existing
        arena_info = mmap(NULL, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, arena_handle->FileDescriptor, 0);
        assert(MAP_FAILED != arena_info);
    }
    else {
        // New file, need to set the size
        code       = ftruncate(arena_handle->FileDescriptor, ArenaSize);
        arena_info = mmap(NULL, ArenaSize, PROT_READ | PROT_WRITE, MAP_SHARED, arena_handle->FileDescriptor, 0);
        assert(MAP_FAILED != arena_info);
        memcpy(arena_info->Signature, Signature, sizeof(Signature));
        strcpy(arena_info->Name, Name);
        arena_info->Size             = BufferSize;
        arena_info->Count            = Count;
        arena_info->Offset           = ARENA_CONTROL_AREA_SIZE;
        arena_info->AllocationBitmap = 0;
        arena_info->LastAllocated    = Count - 1;
    }

    //
    // At this point, we have the arena, so let's validate it is what we expected
    //
    assert(0 == strcmp(Signature, arena_info->Signature));  // eventually, we probably only need to use this field
    assert(0 == strcmp(Name, arena_info->Name));
    assert(arena_info->Size == BufferSize);
    assert(arena_info->Count == Count);
    assert(arena_info->Offset == ARENA_CONTROL_AREA_SIZE);

    arena_handle->Arena = arena_info;

    // We have the arena set up.  Return it.

    return arena_handle;
}

void FincommReleaseArena(fincomm_arena_handle_t ArenaHandle)
{
    int    code;
    size_t arenaSize;

    assert(NULL != ArenaHandle);
    assert(0 == memcmp(ArenaHandle->Signature, Signature, sizeof(Signature)));
    assert(NULL != ArenaHandle->Arena);
    assert(ArenaHandle->FileDescriptor >= 0);

    arenaSize = ARENA_CONTROL_AREA_SIZE +
                (ArenaHandle->Arena->Size * ArenaHandle->Arena->Count);  // one page header + all the data you could want

    code = munmap(ArenaHandle->Arena, arenaSize);
    assert(0 == code);

    ArenaHandle->Arena = NULL;
    close(ArenaHandle->FileDescriptor);
    ArenaHandle->FileDescriptor = -1;
    memset(ArenaHandle->Signature, 0, sizeof(ArenaHandle->Signature));
    free(ArenaHandle);
}

void *FincommAllocateBuffer(fincomm_arena_handle_t ArenaHandle)
{
    void *   buffer = NULL;
    unsigned index;
    uint64_t old_bitmap;
    uint64_t new_bitmap;

    assert(NULL != ArenaHandle);
    assert(0 == memcmp(ArenaHandle->Signature, Signature, sizeof(Signature)));
    assert(NULL != ArenaHandle->Arena);
    assert(ArenaHandle->FileDescriptor >= 0);

    //
    // This block: starts with the value of the last allocated (a hint).
    // It then tries the next bit.   So long as there is at least ONE
    // free bit (not all bits are set) it keeps trying.
    //
    // This code has inherent race conditions: "LastAllocated" isn't
    // really a stable value (but it's a decent enough hint), and we
    // could easily find that the world changed between us capturing
    // the current (cached) copy of the bitmap and when we actually
    // try to swap the two values out.  This isn't a high enough
    // contention path for this to happen very often ("don't care")
    // but this approach avoids naive use of locks instead.
    //
    // Like any chunk of "tricky" code it could be wrong; if so, it is
    // probably wrong in all the places that I tried to do this.
    //
    index = ArenaHandle->Arena->LastAllocated;  // start from where we left off at some point
    while ((uint64_t)~0 != ArenaHandle->Arena->AllocationBitmap) {
        // As long as there's some bit that is zero, we have space
        new_bitmap = old_bitmap = ArenaHandle->Arena->AllocationBitmap;
        index                   = (index + 1) % ArenaHandle->Arena->Count;
        new_bitmap |= make_mask64(index);

        if (old_bitmap == new_bitmap) {
            // The bit we picked to try is already in use, so let's try
            // a different bit
            continue;
        }

        if (__sync_bool_compare_and_swap(&ArenaHandle->Arena->AllocationBitmap, old_bitmap, new_bitmap)) {
            // We were able to claim this particular entry!
            ArenaHandle->Arena->LastAllocated = index;  // we allocated this entry, so save it away
            buffer = (void *)(((uintptr_t)ArenaHandle->Arena) + ArenaHandle->Arena->Offset + (index * ArenaHandle->Arena->Size));
            break;
        }
    }

    return buffer;
}

void FincommFreeBuffer(fincomm_arena_handle_t ArenaHandle, void *Buffer)
{
    uint64_t index  = ~0;  // clearly bogus value
    off_t    offset = ~0;
    uint64_t old_bitmap;
    uint64_t new_bitmap;

    assert(NULL != ArenaHandle);
    assert(0 == memcmp(ArenaHandle->Signature, Signature, sizeof(Signature)));
    assert(NULL != ArenaHandle->Arena);
    assert(ArenaHandle->FileDescriptor >= 0);

    offset = FincommGetBufferOffset(ArenaHandle, Buffer);
    assert(((uintptr_t)(ArenaHandle->Arena) + ARENA_CONTROL_AREA_SIZE + offset) == (uintptr_t)Buffer);

    index = (FincommGetBufferOffset(ArenaHandle, Buffer) / ArenaHandle->Arena->Size);

    assert(((uintptr_t)(ArenaHandle->Arena) + ARENA_CONTROL_AREA_SIZE + (index * ArenaHandle->Arena->Size)) == (uintptr_t)Buffer);
    assert(0 != (make_mask64(index) & ArenaHandle->Arena->AllocationBitmap));  // Don't free something that's not allocated

    do {
        old_bitmap = new_bitmap = ArenaHandle->Arena->AllocationBitmap;
        new_bitmap              = ~make_mask64(index) & ArenaHandle->Arena->AllocationBitmap;  // compute bit clearing
        assert(old_bitmap != new_bitmap);                                                      // protect against this - it's a bug.
    } while (0 == __sync_bool_compare_and_swap(&ArenaHandle->Arena->AllocationBitmap, old_bitmap, new_bitmap));

    // At this point we've "freed" that buffer.
}

void FincommGetArenaInfo(fincomm_arena_handle_t ArenaHandle, char *Name, size_t NameSize, size_t *BufferSize, size_t *Count)
{
    assert(NULL != ArenaHandle);
    assert(0 == memcmp(ArenaHandle->Signature, Signature, sizeof(Signature)));
    assert(NULL != ArenaHandle->Arena);
    assert(ArenaHandle->FileDescriptor >= 0);

    if (NameSize >= strlen(ArenaHandle->Arena->Name)) {
        NameSize = strlen(ArenaHandle->Arena->Name) + 1;
    }
    memcpy(Name, ArenaHandle->Arena->Name, NameSize);
    *BufferSize = ArenaHandle->Arena->Size;
    *Count      = ArenaHandle->Arena->Count;
}

off_t FincommGetBufferOffset(fincomm_arena_handle_t ArenaHandle, void *Buffer)
{
    uintptr_t BufferAddress = (uintptr_t)Buffer;
    uintptr_t StartOfArena;
    uintptr_t EndOfArena;

    assert(NULL != ArenaHandle);
    assert(0 == memcmp(ArenaHandle->Signature, Signature, sizeof(Signature)));
    assert(NULL != ArenaHandle->Arena);
    assert(ArenaHandle->FileDescriptor >= 0);

    StartOfArena = ((uintptr_t)ArenaHandle->Arena) + ARENA_CONTROL_AREA_SIZE;
    EndOfArena   = StartOfArena + (ArenaHandle->Arena->Size * (ArenaHandle->Arena->Count - 1));

    assert((BufferAddress >= StartOfArena) && (BufferAddress <= EndOfArena));

    return BufferAddress - StartOfArena;
}
