//
// (C) Copyright 2020 Tony Mason (fsgeek@cs.ubc.ca)
// All Rights Reserved
//

#include "bitbucket.h"
#include "bitbucketdata.h"
#include "finesse-list.h"
#include "trie.h"
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void DirectoryInitialize(void *Inode, size_t Length)
{
    bitbucket_inode_t *DirInode = (bitbucket_inode_t *)Inode;

    assert(BITBUCKET_INODE_MAGIC == DirInode->Magic);
    assert(BITBUCKET_UNKNOWN_TYPE == DirInode->InodeType);
    assert(Length == DirInode->InodeLength);

    DirInode->InodeType = BITBUCKET_DIR_TYPE; // Mark this as being a directory
    DirInode->Instance.Directory.Magic = BITBUCKET_DIR_MAGIC;
    initialize_list_entry(&DirInode->Instance.Directory.Entries);
    DirInode->Instance.Directory.Children = NULL; // allocated when needed
    DirInode->Attributes.st_mode |= S_IFDIR; // mark as a directory
    DirInode->Attributes.st_nlink = 1; // .

}

static void DirectoryDeallocate(void *Inode, size_t Length)
{
    bitbucket_inode_t *bbi;
    
    
    assert(NULL != Inode);
    bbi = (bitbucket_inode_t *)Inode;
    assert(NULL != bbi);
    assert(bbi->InodeLength == Length);
    CHECK_BITBUCKET_INODE_MAGIC(bbi);
    CHECK_BITBUCKET_DIR_MAGIC(&bbi->Instance.Directory); // layers of sanity checking

    assert(empty_list(&bbi->Instance.Directory.Entries)); // directory should be empty

    bbi->Instance.Directory.Magic = ~BITBUCKET_DIR_MAGIC; // make it easy to recognize use after free

    // Our state is torn down at this point. 

}

static void VerifyDirectoryEntries(bitbucket_inode_t *Directory) 
{
    list_entry_t *le = NULL;
    bitbucket_dir_entry_t *dirEntry = NULL;

    CHECK_BITBUCKET_INODE_MAGIC(Directory);
    assert(BITBUCKET_DIR_TYPE == Directory->InodeType);
    CHECK_BITBUCKET_DIR_MAGIC(&Directory->Instance.Directory);


    list_for_each(&Directory->Instance.Directory.Entries, le) {
        dirEntry = container_of(le, bitbucket_dir_entry_t, ListEntry);

        assert(NULL != dirEntry);
        CHECK_BITBUCKET_DIR_ENTRY_MAGIC(dirEntry);        
    }

}

static bitbucket_object_attributes_t DirectoryObjectAttributes = {
    .Magic = BITBUCKET_OBJECT_ATTRIBUTES_MAGIC,
    .Initialize = DirectoryInitialize,
    .Deallocate = DirectoryDeallocate,
    .Lock = NULL,
    .Trylock = NULL,
    .Unlock = NULL,
};

// Insert a new Inode into an existing directory
// Note: this function does not deal with name collisions very well at the moment (it should assert)
// A root directory entry has identical Inodes and a NULL pointer to the name
int BitbucketInsertDirectoryEntry(bitbucket_inode_t *DirInode, bitbucket_inode_t *Inode, const char *Name)
{
    bitbucket_dir_entry_t *newentry = NULL;
    size_t entry_length = offsetof(bitbucket_dir_entry_t, Name);
    size_t name_length = 0;
    void *tobj = NULL;
    int status;

    assert(NULL != DirInode);
    assert(NULL != Inode);
    assert(NULL != Name);

    assert(BITBUCKET_DIR_TYPE == DirInode->InodeType);
    CHECK_BITBUCKET_DIR_MAGIC(&DirInode->Instance.Directory);

    if (NULL != Name) {
        name_length = strlen(Name) + 1;
        assert(name_length > 1);
    }

    assert(name_length < MAX_FILE_NAME_SIZE);
    entry_length += name_length;
    newentry = (bitbucket_dir_entry_t *)malloc(entry_length);
    assert(NULL != newentry);

    newentry->Magic = BITBUCKET_DIR_ENTRY_MAGIC;
    newentry->Inode = Inode;
    BitbucketReferenceInode(Inode, INODE_DIRENT_REFERENCE);
    initialize_list_entry(&newentry->ListEntry);
    if (NULL != Name) {
        strncpy(newentry->Name, Name, name_length);
        assert(strlen(newentry->Name) == strlen(Name)); // must be the same
    }

    if (DirInode != Inode) {
        BitbucketLockTwoInodes(DirInode, Inode, 1);
    }
    else {
        BitbucketLockInode(DirInode, 1);
    }
    while (NULL != DirInode) {
        newentry->Offset = DirInode->Epoch;
        if (NULL == DirInode->Instance.Directory.Children) {
            DirInode->Instance.Directory.Children = TrieCreateNode();
        }
        assert(NULL != DirInode->Instance.Directory.Children);

        tobj = TrieSearch(DirInode->Instance.Directory.Children, Name);

        if (NULL != tobj) {
            // caller can deal with this case
            status = EEXIST;
            break;
        }

        TrieInsert(DirInode->Instance.Directory.Children, Name, newentry);

        insert_list_head(&DirInode->Instance.Directory.Entries, &newentry->ListEntry);

        if (Inode != DirInode) {
            Inode->Attributes.st_nlink++;
        }

        // TODO: make this conditional on some debugging parameter(s)
        // VerifyDirectoryEntries(DirInode);

        status = 0;
        break;
    }
    if (DirInode != Inode) {
        BitbucketUnlockInode(DirInode);
        BitbucketUnlockInode(Inode);
    }
    else {
        BitbucketUnlockInode(DirInode);
    }

    return status;
}

void BitbucketLookupObjectInDirectory(bitbucket_inode_t *Inode, const char *Name, bitbucket_inode_t **Object) 
{
    bitbucket_dir_t *Directory;
    bitbucket_dir_entry_t *dirent = NULL;

    assert(NULL != Inode);
    assert(NULL != Object);

    assert(BITBUCKET_DIR_TYPE == Inode->InodeType);
    Directory = &Inode->Instance.Directory;

    CHECK_BITBUCKET_DIR_MAGIC(Directory);


    if (NULL != Directory->Children) {

        BitbucketLockInode(Inode, 0);
        dirent = (bitbucket_dir_entry_t *) TrieSearch(Directory->Children, Name);
        if (NULL != dirent) {
            *Object = dirent->Inode;
            BitbucketReferenceInode(dirent->Inode, INODE_LOOKUP_REFERENCE); // ensures that it doesn't go away.
        }

        VerifyDirectoryEntries(Inode); 

        BitbucketUnlockInode(Inode);
    }

    if (NULL == dirent) {
        // Entry not found
        *Object = NULL;
    }

    return;
}

// Bitbucket 

//
// Create a new directory with the given name, insert it into the parent directory, and
// return a pointer to that directory.
//
// To create a root directory call BitbucketCreateRootDirectory
//
// Upon return, this inode will have a lookup reference (for the caller's copy) plus
// references for the directory entries ('.' and '..') and the Inode table reference.
// (In theory it could have more if someone were to find it in the Inode table before
//  this call returns).
//
bitbucket_inode_t *BitbucketCreateDirectory(bitbucket_inode_t *Parent, const char *DirName)
{
    bitbucket_inode_t *newdir = NULL;
    int status = 0;
    uint64_t refcount;

    // Both parameters are required
    assert(NULL != Parent);
    assert(NULL != DirName);

    CHECK_BITBUCKET_INODE_MAGIC(Parent);
    assert(BITBUCKET_DIR_TYPE == Parent->InodeType); // don't support anything else with "contents" (for now)
    CHECK_BITBUCKET_DIR_MAGIC(&Parent->Instance.Directory);
    assert(NULL != Parent->Table); // Parent must be in a table

    newdir = BitbucketCreateInode(Parent->Table, &DirectoryObjectAttributes, 0);
    assert(NULL != newdir);
    CHECK_BITBUCKET_INODE_MAGIC(newdir);
    assert(BITBUCKET_DIR_TYPE == newdir->InodeType); // don't support anything else with "contents"
    CHECK_BITBUCKET_DIR_MAGIC(&newdir->Instance.Directory);

    assert(Parent != newdir); // not permitted

    // We maintain a pointer from the child to the parent.
    // Question: could we avoid this entirely and just keep the directory entry?
    // I'm of course thinking about a less traditional non-hierarchical model here... 
    newdir->Instance.Directory.Parent = Parent;
    BitbucketReferenceInode(Parent, INODE_PARENT_REFERENCE); 

    // All new directories point to themselves.
    status = BitbucketInsertDirectoryEntry(newdir, newdir, ".");
    assert(0 == status);

    // Child points to its parent (no name required)
    status = BitbucketInsertDirectoryEntry(newdir, Parent, "..");
    assert(0 == status);

    // Parent points to the child (if there's a name)
    if (NULL != DirName) {
        status = BitbucketInsertDirectoryEntry(Parent, newdir, DirName);
        assert(0 == status);
    }

    // Four references: one for lookup, one for '.' and one from directory entry
    refcount = BitbucketGetInodeReferenceCount(newdir);
    assert(3 == refcount);

    VerifyDirectoryEntries(newdir); 


    return newdir;

}

static bitbucket_dir_entry_t *RemoveDirEntryFromDirectory(bitbucket_inode_t *Inode, const char *Name)
{
    bitbucket_dir_entry_t *dirent = NULL;
    assert(BITBUCKET_DIR_TYPE == Inode->InodeType);
    
    BitbucketLockInode(Inode, 1);

    while (NULL == dirent) {
        dirent = (bitbucket_dir_entry_t *) TrieSearch(Inode->Instance.Directory.Children, Name);
        if (NULL == dirent) {
            break;
        }

        BitbucketLockInode(dirent->Inode, 1);
        // Remove the entries
        remove_list_entry(&dirent->ListEntry);
        TrieDeletion(&Inode->Instance.Directory.Children, Name);
        assert(dirent->Inode->Attributes.st_nlink > 0);
        dirent->Inode->Attributes.st_nlink--;
        BitbucketUnlockInode(dirent->Inode);
    }

    VerifyDirectoryEntries(Inode);

    BitbucketUnlockInode(Inode);

    return dirent;
}


int BitbucketDeleteDirectoryEntry(bitbucket_inode_t *Directory, const char *Name)
{
    bitbucket_inode_t *inode = NULL;
    bitbucket_dir_entry_t *dirent = NULL;
    int status = ENOSYS;
    bitbucket_dir_entry_t *de = NULL;

    assert(NULL != Directory);
    assert(NULL != Name);
    assert(strlen(Name) > 0);
    CHECK_BITBUCKET_INODE_MAGIC(Directory);
    assert(BITBUCKET_DIR_TYPE == Directory->InodeType);
    CHECK_BITBUCKET_DIR_MAGIC(&Directory->Instance.Directory);
    VerifyDirectoryEntries(Directory);

    // Now let's go find the entry

    while (ENOSYS == status) {

        BitbucketLookupObjectInDirectory(Directory, Name, &inode);
        if (NULL == inode) {
            status = ENOENT;
            break;
        }

        dirent = RemoveDirEntryFromDirectory(Directory, Name);
        if (NULL == dirent) {
            assert(dirent->Inode == inode); // otherwise, something is wrong!
            status = ENOENT;
            break;
        }

        if (BITBUCKET_DIR_TYPE == dirent->Inode->InodeType) {

            // For directories, we break the linkage here
            // So parent no longer points to child (directory), we
            // fix up the child to no longer point to the parent.
            de = RemoveDirEntryFromDirectory(dirent->Inode, "..");

            // Note that when we try to delete the child from the parent, we will find '..'
            // But when we try to delete '.' from the child, we won't find '..'

            if (NULL != de) {
            
                assert(de->Inode == Directory); // how could it not point to the parent?
                assert(dirent->Inode->Instance.Directory.Parent == Directory); // if not, how did we find it?

                BitbucketDereferenceInode(de->Inode, INODE_DIRENT_REFERENCE);
                de->Inode = NULL;

                BitbucketDereferenceInode(dirent->Inode->Instance.Directory.Parent, INODE_PARENT_REFERENCE);
                dirent->Inode->Instance.Directory.Parent = NULL;

                memset(de, 0, offsetof(bitbucket_dir_entry_t, Name));
                free(de);
                de = NULL;
            }

        }

        status = 0;
        break;
    }

    if (NULL != inode) {
        BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
        inode = NULL;
    }

    if (NULL != dirent) {
        BitbucketDereferenceInode(dirent->Inode, INODE_DIRENT_REFERENCE);
        dirent->Inode = NULL;
        memset(dirent, 0, offsetof(bitbucket_dir_entry_t, Name));
        free(dirent);
        dirent = NULL;
    }

    VerifyDirectoryEntries(Directory);


    assert(NULL == dirent);
    assert(NULL == inode);
    assert(NULL == de);

    return status;
}

//
// Invoke this when you want to initiate teardown of a directory.
// This will do two things: 
//   (1) it will remove the '.' entry for the directory
//   (2) it will remove the directory from the Inode table.
//
// This call will fail if the directory has children.
// This call will fail if the directory has a parent reference (meaning it is still reachable)
// This call **may** delete the Directory.  If there are outstanding references, the directory
// will continue to exist; the caller doesn't know.
//
// Note: the caller should own a reference when calling this function.  The caller should
// release that reference after this call returns (which could trigger deletion).
//
int BitbucketDeleteDirectory(bitbucket_inode_t *Inode)
{
    int status = ENOTEMPTY;

    assert(NULL != Inode);
    CHECK_BITBUCKET_INODE_MAGIC(Inode);
    assert(BITBUCKET_DIR_TYPE == Inode->InodeType);
    CHECK_BITBUCKET_DIR_MAGIC(&Inode->Instance.Directory);

    while (NULL != Inode) {
        if (NULL != Inode->Instance.Directory.Parent) {
            // This has a parent reference.  We do not delete it
            status = EINVAL;
            break;
        }

        // If this directory has just one entry ("." presumably) then
        // we consider it to be empty.
        if (!empty_list(&Inode->Instance.Directory.Entries) 
            &&
            (Inode->Instance.Directory.Entries.next->next != &Inode->Instance.Directory.Entries)) {
            status = ENOTEMPTY;
            break;
        }

        status = BitbucketDeleteDirectoryEntry(Inode, ".");  // remove '.' entry

        // the only other references should be opens on the directory itself
        // When they "go away" the directory should go away as well.
        // Admittedly, it's not a very interesting directory at this point.
        break;
    }

    if (0 == status) {
        // at this point we should have at least one references:
        // (1) lookup reference (from the caller)
        assert(1 <= BitbucketGetInodeReferenceCount(Inode));

        // Let the caller drop their own reference.
    }

    
    return status;
}

#if 0
//
// This routine handles deletion of the child directory for the given parent
// with proper locks held.
//
// The caller should hold no locks.  The directory deletion will fail
// if the child directory has any children.
//
// Note: this routine is not coded; I copied it and started thinking about
// what is needed to fix this.  For now, I'm not fixing it...
//
int BitbucketDeleteChildDirectory(bitbucket_inode_t *Inode, const char *Name)
{
    assert(NULL != Directory);
    assert(NULL != Name);
    assert(strlen(Name) > 0);
    CHECK_BITBUCKET_INODE_MAGIC(Directory);
    assert(BITBUCKET_DIR_TYPE == Directory->InodeType);
    CHECK_BITBUCKET_DIR_MAGIC(&Directory->Instance.Directory);
    VerifyDirectoryEntries(Directory);

    // Now let's go find the entry

    while (ENOSYS == status) {

        BitbucketLookupObjectInDirectory(Directory, Name, &inode);
        if (NULL == inode) {
            status = ENOENT;
            break;
        }

        dirent = RemoveDirEntryFromDirectory(Directory, Name);
        if (NULL == dirent) {
            assert(dirent->Inode == inode); // otherwise, something is wrong!
            status = ENOENT;
            break;
        }

        if (BITBUCKET_DIR_TYPE == dirent->Inode->InodeType) {

            // For directories, we break the linkage here
            // So parent no longer points to child (directory), we
            // fix up the child to no longer point to the parent.
            de = RemoveDirEntryFromDirectory(dirent->Inode, "..");

            // Note that when we try to delete the child from the parent, we will find '..'
            // But when we try to delete '.' from the child, we won't find '..'

            if (NULL != de) {
            
                assert(de->Inode == Directory); // how could it not point to the parent?
                assert(dirent->Inode->Instance.Directory.Parent == Directory); // if not, how did we find it?

                BitbucketDereferenceInode(de->Inode, INODE_DIRENT_REFERENCE);
                de->Inode = NULL;

                BitbucketDereferenceInode(dirent->Inode->Instance.Directory.Parent, INODE_PARENT_REFERENCE);
                dirent->Inode->Instance.Directory.Parent = NULL;

                memset(de, 0, offsetof(bitbucket_dir_entry_t, Name));
                free(de);
                de = NULL;
            }

        }

        status = 0;
        break;
    }

    if (NULL != inode) {
        BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
        inode = NULL;
    }

    if (NULL != dirent) {
        BitbucketDereferenceInode(dirent->Inode, INODE_DIRENT_REFERENCE);
        dirent->Inode = NULL;
        memset(dirent, 0, offsetof(bitbucket_dir_entry_t, Name));
        free(dirent);
        dirent = NULL;
    }

    VerifyDirectoryEntries(Directory);


    assert(NULL == dirent);
    assert(NULL == inode);
    assert(NULL == de);

    return status;
}
#endif // 0


//
// This function is used to initialize an enumeration context structure.  It
// must be called prior to using the enumeration context.
//
// Note: Every enumeration context must be destroyed after use
//
void BitbucketInitalizeDirectoryEnumerationContext(bitbucket_dir_enum_context_t *EnumerationContext, bitbucket_inode_t *Directory)
{
    int status;

    assert(NULL != EnumerationContext);
    assert(NULL != Directory);
    CHECK_BITBUCKET_INODE_MAGIC(Directory);
    assert(BITBUCKET_DIR_TYPE == Directory->InodeType);

    VerifyDirectoryEntries(Directory);

    EnumerationContext->Magic = BITBUCKET_DIR_ENUM_CONTEXT_MAGIC;
    EnumerationContext->Directory = Directory;
    BitbucketReferenceInode(Directory, INODE_ENUM_REFERENCE);
    EnumerationContext->NextEntry = NULL;
    EnumerationContext->Offset = 0;
    EnumerationContext->Epoch = EnumerationContext->Directory->Epoch;
    EnumerationContext->LastError = 0;

    BitbucketLockInode(Directory, 0);
    status = BitbucketSeekDirectory(EnumerationContext, 0);
    BitbucketUnlockInode(Directory);
    assert(0 == status); // why are we enumerating an empty directory?
}

//
// This routine is called to clean up an enumeration context after it is not longer needed
//
void BitbucketCleanupDirectoryEnumerationContext(bitbucket_dir_enum_context_t *EnumerationContext)
{
    assert(NULL != EnumerationContext);
    CHECK_BITBUCKET_DIR_ENUM_CONTEXT_MAGIC(EnumerationContext);

    if (NULL != EnumerationContext->Directory) {
        BitbucketDereferenceInode(EnumerationContext->Directory, INODE_ENUM_REFERENCE);
        EnumerationContext->Directory = NULL;
    }

}

// This is an iterative enumeration call/request 
// Note: this MUST be called with the directory locked (at least for read).
// If NULL is returned check the LastError field in the context: ENOENT means the enumeration is done, ERESTARTSYS means the
// directory contents changed during enumeration.  The enumeration cannot be continued in either case.
const bitbucket_dir_entry_t *BitbucketEnumerateDirectory(bitbucket_dir_enum_context_t *EnumerationContext)
{
    bitbucket_dir_entry_t *returnEntry = NULL;
    int status;


    assert(NULL != EnumerationContext);
    CHECK_BITBUCKET_DIR_ENUM_CONTEXT_MAGIC(EnumerationContext);
    assert(NULL != EnumerationContext->Directory);
    EnsureInodeLockedAgainstChanges(EnumerationContext->Directory);

    while (NULL == returnEntry) {

        if (0 != EnumerationContext->LastError) {
            // We can't continue this enumeration; the caller needs to "clean things up".
            returnEntry = NULL;
            break;
        }

        if (NULL == EnumerationContext->NextEntry) {
            // There are no more entries to return
            EnumerationContext->LastError = ENOENT;
            EnumerationContext->Offset = 0; // invalid
            break;
        }

        if (EnumerationContext->Directory->Epoch != EnumerationContext->Epoch) {
            // The directory shape has changed, so we need to re-verify.
            //
            // Note: we could be less forgiving and force the caller to deal with this.  There's
            // no right answer.
            //
            status = BitbucketSeekDirectory(EnumerationContext, EnumerationContext->Offset);
            if (0 != status) {
                // Error state - we're done
                break;
            }
        }

        returnEntry = EnumerationContext->NextEntry;

        // Set up for the next call
        if (returnEntry->ListEntry.next == &EnumerationContext->Directory->Instance.Directory.Entries) {
            // No more entries to return; we aren't in error state.  Next time through we will
            // notice no more entries and return an error.
            EnumerationContext->NextEntry = NULL;
            EnumerationContext->Offset = ~0;
            break;
        }

        EnumerationContext->NextEntry = container_of(returnEntry->ListEntry.next, bitbucket_dir_entry_t, ListEntry);
        CHECK_BITBUCKET_DIR_ENTRY_MAGIC(EnumerationContext->NextEntry);

        // The offset for the next directory entry.
        EnumerationContext->Offset = EnumerationContext->NextEntry->Offset;

        break;

    }

    return returnEntry;
}

//
// This will set the enumeration context to match the information in the Offset.  If the offset is not valid, this will return
// an error (not zero) otherwise it returns zero.  Note that the offset is an opaque value - do not rely upon the current
// implementation not changing.
// Note: the caller is responsible for locking the directory (for read) prior to invoking this call.
int BitbucketSeekDirectory(bitbucket_dir_enum_context_t *EnumerationContext, uint64_t Offset)
{
    list_entry_t *le = NULL;
    bitbucket_dir_entry_t *dirEntry = NULL;

    assert(NULL != EnumerationContext);
    CHECK_BITBUCKET_DIR_ENUM_CONTEXT_MAGIC(EnumerationContext);
    assert(BITBUCKET_DIR_TYPE == EnumerationContext->Directory->InodeType);
    CHECK_BITBUCKET_DIR_MAGIC(&EnumerationContext->Directory->Instance.Directory);
    EnsureInodeLockedAgainstChanges(EnumerationContext->Directory);

    while (NULL == le) {
        // Any time we do a seek, we update to the current Epoch
        EnumerationContext->Epoch = EnumerationContext->Directory->Epoch;

        if (0 == Offset) {
            // this is a "rewind to the beginning" request
            if (empty_list(&EnumerationContext->Directory->Instance.Directory.Entries)) {
                EnumerationContext->NextEntry = NULL;
                EnumerationContext->LastError = ENOENT;
                EnumerationContext->Offset = 0;
                break;
            }

            le = list_head(&EnumerationContext->Directory->Instance.Directory.Entries);
            EnumerationContext->NextEntry = container_of(le, bitbucket_dir_entry_t, ListEntry);
            CHECK_BITBUCKET_DIR_ENTRY_MAGIC(EnumerationContext->NextEntry);
            EnumerationContext->Offset = EnumerationContext->NextEntry->Offset;
            EnumerationContext->LastError = 0;
            break;
        }

        if (EnumerationContext->Offset == Offset) {
            // This is a no-op, so we stop here.
            break;
        }

        // Set up for the default return (which is an error condition)
        EnumerationContext->NextEntry = NULL;
        EnumerationContext->Offset = 0;
        EnumerationContext->LastError = ENOENT;

        list_for_each(&EnumerationContext->Directory->Instance.Directory.Entries, le) {
            dirEntry = container_of(le, bitbucket_dir_entry_t, ListEntry);
            
            if (dirEntry->Offset == Offset) {
                break;
            }
            dirEntry = NULL; // we rely on this being NULL in the "ran off the end" case handled below
        }

        if (NULL == dirEntry) {
            // we ran off the end of the list
            break;
        }

        // We have an entry
        EnumerationContext->LastError = 0; // success
        EnumerationContext->Offset = dirEntry->Offset;
        EnumerationContext->NextEntry = dirEntry;
        break;

    }

    return EnumerationContext->LastError;

}

//
// This routine creates a root directory inode and inserts it
// into the specified inode table.
//
// Upon return this object has five references:
//   (1) for it's table reference
//   (1) for the reference returned to the caller
//   (1) for it's self reference (Child Dir -> Parent Dir)
//   (2) for it's directory entries ('.' -> Self, '..' -> Self)
//
// Note: while I call this a "root directory inode" it is not required
// to be unique and could coexist with other root directory inodes.
// Disambiguating them would be entirely the responsibility of some
// higher layer.
//
bitbucket_inode_t *BitbucketCreateRootDirectory(bitbucket_inode_table_t *Table)
{
	bitbucket_inode_t *inode = NULL;
    int status = 0;

	while (NULL == inode) {
        inode = BitbucketCreateInode(Table, &DirectoryObjectAttributes, 0);
        assert(NULL != inode);
        CHECK_BITBUCKET_INODE_MAGIC(inode);
        assert(BITBUCKET_DIR_TYPE == inode->InodeType);
        CHECK_BITBUCKET_DIR_MAGIC(&inode->Instance.Directory);

        // Single ref here: the lookup reference returned from create.
        // Technically, this assert isn't quite right, since once we've
        // inserted it, someone else could find it.  That's unlikely
        // but if it becomes a problem, remove it or weaken it.
        assert(1 == BitbucketGetInodeReferenceCount(inode));

        // A root directory is its own parent
        inode->Instance.Directory.Parent = inode;
        BitbucketReferenceInode(inode, INODE_PARENT_REFERENCE);
        assert(2 == BitbucketGetInodeReferenceCount(inode));

        // All directories point to themselves.
        status = BitbucketInsertDirectoryEntry(inode, inode, ".");
        assert(0 == status);
        assert(3 == BitbucketGetInodeReferenceCount(inode));

        // A root directory is its own parent (this time in the directory)
        status = BitbucketInsertDirectoryEntry(inode, inode, "..");
        assert(0 == status);
        assert(4 == BitbucketGetInodeReferenceCount(inode));

        // We have a complete inode now.
        break;
	}

	return inode;
}

//
// This routine:
//   - Removes the given root directory inode from the inode table
//   - Removes the "." and ".." references for this root directory.
//   - Removes the self-reference (from Parent)
//
// Thus, assuming the caller holds a lookup reference, it won't
// go away.  
// This routine creates a root directory inode and inserts it
// into the specified inode table.
//
// Upon return this object has five references:
//   (1) for it's table reference
//   (1) for the reference returned to the caller
//   (1) for it's self reference (Child Dir -> Parent Dir)
//   (2) for it's directory entries ('.' -> Self, '..' -> Self)
//
// Note: while I call this a "root directory inode" it is not required
// to be unique and could coexist with other root directory inodes.
// Disambiguating them would be entirely the responsibility of some
// higher layer.
//

void BitbucketDeleteRootDirectory(bitbucket_inode_t *RootDirectory)
{
    int status = 0;

    CHECK_BITBUCKET_INODE_MAGIC(RootDirectory);
    assert(BITBUCKET_DIR_TYPE == RootDirectory->InodeType);

    // Make sure it doesn't go away on us
    BitbucketReferenceInode(RootDirectory, INODE_LOOKUP_REFERENCE);

    // We need to get rid of the parent reference
    assert(NULL != RootDirectory->Instance.Directory.Parent);
    BitbucketDereferenceInode(RootDirectory->Instance.Directory.Parent, INODE_PARENT_REFERENCE);
    RootDirectory->Instance.Directory.Parent = NULL;
    status = BitbucketDeleteDirectoryEntry(RootDirectory, "..");
    assert(0 == status);

    // Now we can just call the standard deletion routine
    status = BitbucketDeleteDirectory(RootDirectory);
    assert(0 == status);
    // has to be at least one
    assert(1 <= BitbucketGetInodeReferenceCount(RootDirectory));

    // Release our reference
    BitbucketDereferenceInode(RootDirectory, INODE_LOOKUP_REFERENCE);
}
