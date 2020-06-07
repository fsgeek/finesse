/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "test_bitbucket.h"
#include <uuid/uuid.h>
#include "../bitbucketdata.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif //

typedef struct _test_object {
    uint64_t                Magic;
    size_t                  Length;
    uint64_t                InitializeCalled;
    uint64_t                DeallocateCalled;
    uint64_t                LockCalled;
    uint64_t                ExclusiveCalled;
    uint64_t                UnlockCalled;
    struct _test_object    *Save;
} test_object_t;

static void test_co_init(void *Object, size_t Length) 
{
    test_object_t *toc = (test_object_t *)Object;
    assert(sizeof(test_object_t) <= Length); // bigger is allowed
    toc->Magic = 42;
    toc->Length = Length;
    toc->InitializeCalled = 1;
    toc->DeallocateCalled = 0;
    toc->LockCalled = 0;
    toc->ExclusiveCalled = 0;
    toc->UnlockCalled = 0;
    toc->Save = NULL;
}

static void test_co_dealloc(void *Object, size_t Length)
{
    test_object_t *toc = (test_object_t *)Object;

    assert(NULL != toc);
    assert(42 == toc->Magic);
    assert(toc->Length == Length);

    toc->DeallocateCalled++;
    if (NULL != toc->Save) {
        *toc->Save = *toc; // save a copy for verification
    }
}

static void test_co_lock(void *Object, int Exclusive)
{
    test_object_t *toc = (test_object_t *)Object;

    (void) Exclusive; // TODO: in the future we could use that info
    toc->LockCalled++;
    if (Exclusive) {
        toc->ExclusiveCalled++;
    }
}

static void test_co_unlock(void *Object)
{
    test_object_t *toc = (test_object_t *)Object;

    toc->UnlockCalled++;
}


static MunitResult
test_create_object(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    size_t length = 128;
    void *object = NULL;
    test_object_t to = {
        .Magic = 42,
        .InitializeCalled = 0,
        .DeallocateCalled = 0,
        .LockCalled = 0,
        .UnlockCalled = 0,
    };
    test_object_t *toc = NULL;
    bitbucket_object_attributes_t oa = {
        .Magic = BITBUCKET_OBJECT_ATTRIBUTES_MAGIC,
        .ReasonCount = BITBUCKET_MAX_REFERENCE_REASONS,
        .ReferenceReasonsNames = {
        "TestReason0",
        "TestReason1",
        "TestReason2",
        "TestReason3",
        "TestReason4",
        "TestReason5",
        "TestReason6",
        "TestReason7",
        },
        .Initialize = test_co_init,
        .Deallocate = test_co_dealloc,
        .Lock = test_co_lock,
        .Unlock = test_co_unlock,
    };

    object = BitbucketObjectCreate(&oa, length, 0);
    munit_assert(NULL != object);

    toc = (test_object_t *)object;
    toc->Save = &to;
    BitbucketObjectReference(toc, 1);
    BitbucketObjectDereference(toc, 1);
    BitbucketObjectDereference(object, 0);
    // No easy way to verify it has been deleted, is there...

    munit_assert(42 == to.Magic);
    munit_assert(1 == to.InitializeCalled);
    munit_assert(1 == to.DeallocateCalled);
    munit_assert(4 == to.LockCalled);
    munit_assert(1 == to.ExclusiveCalled);
    munit_assert(3 == to.UnlockCalled); // last call isn't captured

    return MUNIT_OK;
 

    return MUNIT_OK;
}

static MunitResult
test_create_object_with_defaults(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    size_t length = 128;
    void *object = NULL;

    object = BitbucketObjectCreate(NULL, length, 0);
    munit_assert(NULL != object);

    BitbucketObjectDereference(object, 0);
    // No easy way to verify it has been deleted, is there...

    return MUNIT_OK;
}


static const MunitTest object_tests[] = {
        TEST("/null", test_null, NULL),
        TEST("/create", test_create_object, NULL),
        TEST("/defaultcreate", test_create_object_with_defaults, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite object_suite = {
    .prefix = (char *)(uintptr_t)"/object",
    .tests = (MunitTest *)(uintptr_t)object_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};


