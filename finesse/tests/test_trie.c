/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "test_bitbucket.h"
#include "trie.h"
#include <uuid/uuid.h>

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif //

static MunitResult
test_create_trie(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    struct Trie *trie = NULL;
    int status;

    trie = TrieCreateNode();
    munit_assert(NULL != trie);

    status = TrieDeletion(&trie, "");
    munit_assert(0 == status);
    munit_assert(NULL == trie);

    return MUNIT_OK;
}

typedef struct _trie_test_object {
    uuid_t uuid;
    char uuid_string[40];
} trie_test_object_t;

static MunitResult
test_insert(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    struct Trie *trie = NULL;
    int status;
    const unsigned object_count = 2;
    const unsigned iterations = 1; // TODO: bump this up
    trie_test_object_t *trie_objects = NULL;

    trie_objects = (trie_test_object_t *)malloc(object_count * sizeof(trie_test_object_t));
    munit_assert(NULL != trie_objects);

    for (unsigned index = 0; index < object_count; index++) {
        uuid_generate(trie_objects[index].uuid);
        uuid_unparse(trie_objects[index].uuid, trie_objects[index].uuid_string);
    }

    trie = TrieCreateNode();
    munit_assert(NULL != trie);

    for (unsigned index = 0; index < object_count; index++) {
        TrieInsert(trie, trie_objects[index].uuid_string, &trie_objects[index]);
    }

    for (unsigned index = 0; index < iterations * object_count; index++) {
        long int rnd = random() % object_count;
        trie_test_object_t *testobj = NULL;

        testobj = TrieSearch(trie, trie_objects[rnd].uuid_string);
        munit_assert(testobj == &trie_objects[rnd]);
    }

    for (unsigned index = 0; index < object_count; index++) {
        trie_test_object_t *testobj = NULL;

        status = TrieDeletion(&trie, trie_objects[index].uuid_string);
        munit_assert(status >= 0);
        testobj = TrieSearch(trie, trie_objects[index].uuid_string);
        munit_assert(NULL == testobj); // shouldn't be able to find it any longer
        trie_objects[index].uuid_string[0] = '\0';
    }

    munit_assert(NULL == trie); // should be all deleted at this point, since it has gone empty

    free(trie_objects);
    trie_objects = NULL;

    return MUNIT_OK;
}

static MunitResult
test_dots(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    struct Trie *trie = NULL;
    void *result;
    int status;

    trie = TrieCreateNode();
    munit_assert(NULL != trie);

    TrieInsert(trie, ".", (void *)0x1);
    TrieInsert(trie, "..", (void *)0x2);

    result = TrieSearch(trie, ".");
    munit_assert(result == (void *)0x1);

    result = TrieSearch(trie, "..");
    munit_assert(result == (void *)0x2);

    status = TrieDeletion(&trie, "..");
    munit_assert(0 == status);
    
    status = TrieDeletion(&trie, ".");
    munit_assert(0 == status);

    munit_assert(NULL == trie);

    return MUNIT_OK;
}



static const MunitTest trie_tests[] = {
        TEST("/null", test_null, NULL),
        TEST("/create", test_create_trie, NULL),
        TEST("/insert", test_insert, NULL),
        TEST("/dots", test_dots, NULL),
    	TEST(NULL, NULL, NULL),
    };

const MunitSuite trie_suite = {
    .prefix = (char *)(uintptr_t)"/trie",
    .tests = (MunitTest *)(uintptr_t)trie_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};


