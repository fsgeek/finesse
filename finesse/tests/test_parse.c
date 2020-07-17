/*
 * Copyright (c) 2017-2020, Tony Mason. All rights reserved.
 */

#include <errno.h>
#include <munit.h>
#include <strings.h>
#include "test_utils.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif  //

typedef struct _FinesseServerPathResolutionParameters {
    uint8_t     Size;                // Number of bytes in this structure; allows growing it
    int         FollowSymlinks : 1;  // If set, this indicates we should follow symlinks.  False, we stop with the symlink.
    int         CheckSecurity : 1;   // This should perform a security check at each
    int         GetFinalParent : 1;  // This should return the final parent as the target inode (even if child doesn't exist)
    fuse_ino_t  Parent;              // In parameter; 0 = PathName is absolute and must contain mountpoint name
    const char *PathName;            // In parameters
    const char *FinalName;           // out parameter MBZ on entry
    fuse_ino_t  TargetInode;         // out parameter MBZ on entry
} FinesseServerPathResolutionParameters_t;

static int next_ino = 101;

// dummy function - it always succeeds
static ino_t lookup(ino_t parent, const char *name)
{
    ino_t ino;

    munit_assert(NULL != name);
    munit_assert(0 != parent);
    if (0 == strlen(name)) {
        ino = parent;
    }
    else {
        ino = next_ino++;
    }
    munit_assert(NULL == index(name, '/'));

    return ino;
}

static ino_t parse_path(ino_t parent, const char *path)
{
    char * workpath       = NULL;
    char * finalsep       = NULL;
    char * workcurrent    = NULL;
    char * workend        = NULL;
    size_t pathlength     = 0;
    size_t workpathlength = 0;
    ino_t  ino            = 0;

    munit_assert(NULL != path);

    while (1) {
        if (0 == parent) {
            parent = FUSE_ROOT_ID;
        }

        pathlength = strlen(path);
        if (0 == pathlength) {
            // "" - interpret this as opening the parent
            ino = parent;
            break;
        }

        finalsep = rindex(path, '/');
        if (NULL == finalsep) {
            // "foo" - this is not valid (needs a "/" at the beginning)
            ino = 0;
            break;
        }

        if (finalsep == path) {
            // "/foo" - just look it up relative to the parent
            ino = lookup(parent, &path[1]);
            break;
        }

        // otherwise, we need a working buffer so we can carve things up
        workpathlength = pathlength + 1;
        workpath       = (char *)malloc(workpathlength);
        munit_assert(NULL != workpath);

        memcpy(workpath, path, pathlength);
        workpath[pathlength] = '\0';
        workend              = rindex(workpath, '/');

        // cases we've already eliminated
        munit_assert(NULL != workend);
        munit_assert(workend != workpath);
        workend++;  // skip over the separator

        ino = parent;
        munit_assert('/' == workpath[0]);
        for (workcurrent = &workpath[1]; workcurrent != workend;) {
            char *segend = NULL;
            munit_assert(NULL != workcurrent);
            munit_assert('/' != *workcurrent);
            segend = index(workcurrent, '/');
            munit_assert(NULL != segend);  // otherwise we've met the termination condition already!
            *segend = '\0';
            ino     = lookup(ino, workcurrent);
            assert(0 != ino);
            // TODO: this is where I'd do a check for a symlink, in which case I have to rebuild the name and try again
            // TODO: this is where I'd add a security check
            workcurrent = segend + 1;  // move to the next entry
        }

        // At this point I have a single segment left
        // TODO: this is where I'd handle the "parent lookup of final segment" option
        ino = lookup(ino, workend);
        break;
    }

    if (NULL != workpath) {
        free(workpath);
        workpath = NULL;
    }

    return ino;
}

static const char *strings_to_parse[] = {
    "", "/foo", "invalid", "/foo/bar/", "/foo:bar/foo/bar",
};

static int parse(FinesseServerPathResolutionParameters_t *Parameters)
{
    ino_t ino = 0;

    munit_assert(NULL != Parameters);
    munit_assert(sizeof(FinesseServerPathResolutionParameters_t) == Parameters->Size);
    munit_assert(0 == Parameters->FollowSymlinks);  // not coded
    munit_assert(0 == Parameters->CheckSecurity);   // not coded
    munit_assert(0 == Parameters->GetFinalParent);  // not coded yet
    munit_assert(0 == Parameters->Parent);          // not coded yet
    munit_assert(NULL != Parameters->PathName);     // Must be passed
    munit_assert(NULL == Parameters->FinalName);    // Must not be passed
    munit_assert(0 == Parameters->TargetInode);
    ino = parse_path(Parameters->Parent, Parameters->PathName);

    munit_assert(0 != ino);

    return MUNIT_OK;
}

static MunitResult test_parse(const MunitParameter params[], void *arg)
{
    FinesseServerPathResolutionParameters_t path_params;
    int                                     status;

    (void)params;
    (void)arg;

    path_params.Size           = sizeof(path_params);
    path_params.FollowSymlinks = 0;
    path_params.CheckSecurity  = 0;
    path_params.GetFinalParent = 0;
    path_params.Parent         = 0;

    for (unsigned index = 0; index < sizeof(strings_to_parse) / sizeof(const char *); index++) {
        path_params.PathName    = strings_to_parse[index];
        path_params.FinalName   = NULL;
        path_params.TargetInode = 0;

        status = parse(&path_params);
        assert(0 == status);  // good place to start
    }

    return MUNIT_OK;
}

static MunitTest parse_tests[] = {
    TEST((char *)(uintptr_t) "/null", test_null, NULL),
    TEST((char *)(uintptr_t) "/parse", test_parse, NULL),
    TEST(NULL, NULL, NULL),
};

const MunitSuite parse_suite = {
    .prefix     = (char *)(uintptr_t) "/parse",
    .tests      = parse_tests,
    .suites     = NULL,
    .iterations = 1,
    .options    = MUNIT_SUITE_OPTION_NONE,
};

static MunitSuite testutils_suites[10];

MunitSuite *SetupMunitSuites()
{
    unsigned index = 0;

    memset(testutils_suites, 0, sizeof(testutils_suites));
    testutils_suites[index++] = parse_suite;
    return testutils_suites;
}
