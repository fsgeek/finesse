/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"

typedef struct _FinesseServerPathResolutionParameters {
    size_t       Size;                  // Number of bytes in this structure; allows growing it
    unsigned int FollowSymlinks : 1;    // If set, this indicates we should follow symlinks.  False, we stop with the symlink.
    unsigned int CheckSecurity : 1;     // This should perform a security check at each
    unsigned int GetFinalParent : 1;    // This should return the final parent as the target inode (even if child doesn't exist)
    fuse_ino_t   Parent;                // In parameter; 0 = PathName is absolute and must contain mountpoint name
    const char * PathName;              // In parameters
    const char * Cursor;                // Location in the PathName being parsed.
    const char * FinalName;             // out parameter MBZ on entry
    struct statx StatxBuffer;           // out parameter; object status information (including inode number)
    size_t       PathNameBufferLength;  // Maximum buffer length (internal use)
} FinesseServerPathResolutionParameters_t;

#define FINESSE_SERVER_PATH_RESOLUTION_FOLLOW_SYMLINKS (0x1)
#define FINESSE_SERVER_PATH_RESOLUTION_CHECK_SECURITY (0x2)
#define FINESSE_SERVER_PATH_RESOLUTION_GET_FINAL_PARENT (0x4)
#define FINESSE_SERVER_PATH_RESOLUTION_VALID_FLAGS                                                    \
    (FINESSE_SERVER_PATH_RESOLUTION_FOLLOW_SYMLINKS | FINESSE_SERVER_PATH_RESOLUTION_CHECK_SECURITY | \
     FINESSE_SERVER_PATH_RESOLUTION_GET_FINAL_PARENT)

void FinesseFreeServerPathResolutionParameters(FinesseServerPathResolutionParameters_t *Parameters)
{
    while (NULL != Parameters) {
        if (Parameters->PathName) {
            free((void *)(uintptr_t)Parameters->PathName);
            Parameters->PathName = NULL;
        }

        // Note that final name is just a pointer _into_ PathName, so it does not get freed
        // If that changes, fix this routine.
        free(Parameters);
        Parameters = NULL;
    }
}

//
// This allocates and initializes a server path resolution structure.  It should be freed with
// FinesseFreeServerPathResolutionParameters.
//
// ParentInode can be 0, which means that the PathName is an absolute path name.  It must start with the volume mount point!
// ParentInode can be any other valid inode, which means the PathName is relative to that parent Inode.
//
// PathName must not be NULL.
// PathName can be empty, in which case this will return the ParentInode; if there is no ParentInode, it will return EINVAL.
//
// Flags indicate any special handling for this path resolution:
//    FOLLOW_SYMLINKS - this is used to indicate that we should follow any symlinks as part of path name resolution
//    CHECK_SECURITY - this is used to indicate that we should do component-wise security checks (by calling access).
//    GET_FINAL_PARENT - this is used to indicate that we want the parent inode.  Thus if the path is "/foo/bar/xyz.txt", the
//                       parent returned will be the inode of "/foo/bar".
//
// Returns:
//    NULL - something failed (usually allocation)
//    pointer to the
//
FinesseServerPathResolutionParameters_t *FinesseAllocateServerPathResolutionParameters(fuse_ino_t ParentInode, const char *PathName,
                                                                                       int Flags)
{
    FinesseServerPathResolutionParameters_t *parameters = NULL;
    char *                                   buffer     = NULL;
    int                                      status     = 0;

    assert(NULL != PathName);

    parameters = malloc(sizeof(FinesseServerPathResolutionParameters_t));
    while (NULL != parameters) {
        memset(parameters, 0, sizeof(*parameters));
        parameters->Size = sizeof(*parameters);

        if (Flags & FINESSE_SERVER_PATH_RESOLUTION_FOLLOW_SYMLINKS) {
            parameters->FollowSymlinks = 1;
        }

        if (Flags & FINESSE_SERVER_PATH_RESOLUTION_CHECK_SECURITY) {
            parameters->CheckSecurity = 1;
        }

        if (Flags & FINESSE_SERVER_PATH_RESOLUTION_GET_FINAL_PARENT) {
            parameters->GetFinalParent = 1;
        }

        parameters->PathNameBufferLength = strlen(PathName) + 1;
        buffer                           = malloc(parameters->PathNameBufferLength);
        assert(NULL != buffer);
        strcpy(buffer, PathName);
        parameters->PathName = buffer;

        parameters->Parent = ParentInode;

        memset(&parameters->StatxBuffer, 0, sizeof(parameters->StatxBuffer));
        // Done
        break;
    }

    if (0 != status) {
        // Something went wrong...
        FinesseFreeServerPathResolutionParameters(parameters);
        parameters = NULL;
    }

    return parameters;
}

//
// The kernel (and FUSE) will do path name parsing.  While some file systems can handle multi-part path names,
// we can't assume that all will do so (bitbucket does not, passthrough_ll does).  Thus, we need to have
// something equivalent to namei style functionality.
//
// Given a specific FUSE session, this routine takes a parent inode number and a name (possibly a path name)
// If the parent inode number is 0, this routine will expect the passed in name to be an absolute name relative
// to the system root. If the parent inode number is anything else, this routine will expect the passed in name
// to be a name relative to the parent inode number.
//
// If successful, this routine returns 0 and sets TargetInode to contain the name of the final inode.
//
// Note: flags is expected to be used to ask for:
//   - Symbolic link following behavior
//   - Security checking behavior
//   - parent/child behavior ("need the path to the final parent")
//
// Returns:
//   EINVAL - the combination of parameters specified is not valid
//   ENOENT - some component of the path does not exist
//   ELOOP - too many symlinks
//   EACCES - permission denied
//   others to be determined.
//
int FinesseServerResolvePathName(struct fuse_session *se, FinesseServerPathResolutionParameters_t *Parameters)
{
    char *   workpath       = NULL;
    char *   finalsep       = NULL;
    char *   workcurrent    = NULL;
    char *   workend        = NULL;
    size_t   workpathlength = 0;
    ino_t    ino            = 0;
    ino_t    parentino      = 0;
    int      status         = EINVAL;
    unsigned idx            = 0;
    size_t   mp_length      = 0;

    assert(NULL != se);
    assert(NULL != Parameters);
    assert(NULL != Parameters->PathName);

    while (1) {
        parentino = Parameters->Parent;

        if (0 == parentino) {
            parentino = FUSE_ROOT_ID;
        }

        if (0 == Parameters->PathNameBufferLength) {
            // "" - interpret this as opening the parent
            // Note that this is consistent with what FUSE does
            ino = parentino;
            break;
        }

        // we need a working buffer so we can carve things up
        workpathlength = Parameters->PathNameBufferLength;
        workpath       = (char *)malloc(workpathlength);
        assert(NULL != workpath);

        // Strip off the mount point path if it is present
        mp_length = strlen(se->mountpoint);
        if ((Parameters->PathNameBufferLength < mp_length) || (0 != memcmp(Parameters->PathName, se->mountpoint, mp_length))) {
            mp_length = 0;  // don't strip it off
        }

        strcpy(workpath, &Parameters->PathName[mp_length]);

        finalsep = rindex(workpath, '/');
        if (NULL == finalsep) {
            // "foo" - this is not valid (needs a "/" at the beginning)
            ino = 0;
            break;
        }

        if (finalsep == workpath) {
            // "/foo" - just look it up relative to the parent; skips the leading slash
            status = FinesseServerInternalNameLookup(se, parentino, workpath + 1, &Parameters->StatxBuffer);

            if (0 != status) {
                fprintf(stderr, "%s:%d --> FinesseServerInternalNameLookup failed\n", __func__, __LINE__);
                assert((ENOENT == status) || (-ENOENT == status));
                Parameters->Cursor = workpath + 1;
                Parameters->Parent = ino;  // This is the parent we found.
                break;
            }
            ino = Parameters->StatxBuffer.stx_ino;
            assert(0 != ino);
            break;
        }

        workend = rindex(workpath, '/');

        // cases we've already eliminated
        assert(NULL != workend);
        assert(workend != workpath);
        workend++;  // skip over the separator

        ino = parentino;
        assert('/' == workpath[0]);
        for (workcurrent = &workpath[1]; workcurrent != workend;) {
            char *segend = NULL;
            assert(NULL != workcurrent);
            assert('/' != *workcurrent);
            segend = index(workcurrent, '/');
            assert(NULL != segend);  // otherwise we've met the termination condition already!
            *segend = '\0';

            // Security
            if (Parameters->CheckSecurity) {
                assert(0);  // TODO
                // Basically, this should be a call into the file system to see if the given
                // caller has access to this file. Probably need a wrapper, much like
                // the lookup wrapper.
            }

            // Look up the entry
            status = FinesseServerInternalNameLookup(se, ino, workcurrent, &Parameters->StatxBuffer);
            if (0 != status) {
                fprintf(stderr, "%s:%d --> FinesseServerInternalNameLookup failed\n", __func__, __LINE__);
                assert((ENOENT == status) || (-ENOENT == status));  // don't handle other errors at this point

                idx                = (unsigned)((uintptr_t)(workcurrent - workpath));
                Parameters->Cursor = &Parameters->PathName[idx];  // point to location we processed up to
                Parameters->Parent = ino;                         // This is the parent we found.
                break;
            }

            if (S_ISLNK(Parameters->StatxBuffer.stx_mode)) {
                //
                // This is a symlink.  To handle this properly, we need to read the link contents
                // and then reconstruct the name.  For Finesse this gets _more_ complicated because
                // links can traverse out of the file system
                //
                assert(0);  // TODO
            }
            ino = Parameters->StatxBuffer.stx_ino;
            assert(0 != ino);
            // TODO: this is where I'd do a check for a symlink, in which case I have to rebuild the name and try again
            // TODO: this is where I'd add a security check
            workcurrent = segend + 1;  // move to the next entry
        }

        // At this point I have a single segment left
        idx                   = (unsigned)((uintptr_t)(workcurrent - workpath));
        Parameters->Cursor    = &Parameters->PathName[idx];
        Parameters->FinalName = Parameters->Cursor + 1;
        Parameters->Parent    = ino;

        status = FinesseServerInternalNameLookup(se, ino, workend, &Parameters->StatxBuffer);
        if ((0 != status)) {
            fprintf(stderr, "%s:%d --> FinesseServerInternalNameLookup failed\n", __func__, __LINE__);
            assert((ENOENT == status) || (-ENOENT == status));

            if (Parameters->GetFinalParent) {
                // The caller didn't _need_ the last path
                status = 0;
            }
            break;
        }

        // We've found it all. Final inode is in the parameters structure.
        break;
    }

    if (NULL != workpath) {
        free(workpath);
        workpath = NULL;
    }

    return status;
}

int FinesseGetResolvedStatx(FinesseServerPathResolutionParameters_t *Parameters, struct statx *StatxData)
{
    assert(NULL != Parameters);
    assert(NULL != StatxData);

    if (0 == Parameters->StatxBuffer.stx_ino) {
        memset(StatxData, 0, sizeof(struct statx));
        return EINVAL;
    }

    memcpy(StatxData, &Parameters->StatxBuffer, sizeof(struct statx));

    return 0;
}

int FinesseGetResolvedInode(FinesseServerPathResolutionParameters_t *Parameters, ino_t *InodeNumber)
{
    int          status = 0;
    struct statx statxbuf;

    assert(NULL != Parameters);
    assert(NULL != InodeNumber);

    status = FinesseGetResolvedStatx(Parameters, &statxbuf);
    if (0 == status) {
        *InodeNumber = statxbuf.stx_ino;
    }
    else {
        *InodeNumber = 0;
    }
    return status;
}
