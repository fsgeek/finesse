
/*
 * Copyright (c) 2017-2020, Tony Mason. All rights reserved.
 */

#include "test_utils.h"
#include "../utils/crc32c.h"
#include "../utils/murmurhash3.h"
#include <finesse-fuse.h>
#include <stdlib.h>

#define __packed __attribute__((packed))
#define __notused __attribute__((unused))

#if 0
typedef void *finesse_object_t;
typedef void *finesse_object_table_t;
finesse_object_t *FinesseObjectLookupByIno(finesse_object_table_t *Table, fuse_ino_t InodeNumber);
finesse_object_t *FinesseObjectLookupByUuid(finesse_object_table_t *Table, uuid_t *Uuid);
void FinesseObjectRelease(finesse_object_table_t *Table, finesse_object_t *Object);
finesse_object_t *FinesseObjectCreate(finesse_object_table_t *Table, fuse_ino_t InodeNumber, uuid_t *Uuid);
uint64_t FinesseObjectGetTableSize(finesse_object_table_t *Table);
void FinesseInitializeTable(finesse_object_table_t *Table);
void FinesseDestroyTable(finesse_object_table_t *Table);
finesse_object_table_t *FinesseCreateTable(uint64_t EstimatedSize);
#endif // 0

static
MunitResult
test_table_basics(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    finesse_object_table_t *table = NULL;

    table = FinesseCreateTable(0);
    munit_assert(NULL != table);
    munit_assert(0 == FinesseObjectGetTableSize(table));
    FinesseDestroyTable(table);

    table = FinesseCreateTable(1024 * 1024);
    munit_assert(NULL != table);
    munit_assert(0 == FinesseObjectGetTableSize(table));
    FinesseDestroyTable(table);

    table = FinesseCreateTable((26 * 1024 * 1024) + (25 * 1024));
    munit_assert(NULL != table);
    munit_assert(0 == FinesseObjectGetTableSize(table));
    FinesseDestroyTable(table);

    table = FinesseCreateTable(1024 * 1024 * 1024);
    munit_assert(NULL != table);
    munit_assert(0 == FinesseObjectGetTableSize(table));
    FinesseDestroyTable(table);

    return MUNIT_OK;
}

typedef struct _test_object {
    fuse_ino_t      inode;
    uuid_t          uuid;
    unsigned        lookups;
    unsigned        releases;
} test_object_t;

static test_object_t *insert_multiple_objects(finesse_object_table_t *Table, uint32_t ObjectCount)
{
    test_object_t *objects = NULL;
    finesse_object_t *fobj = NULL;

    munit_assert(NULL != Table);
    munit_assert(ObjectCount < (1024 * 1024 * 1024));

    objects = (test_object_t *)malloc(sizeof(test_object_t) * ObjectCount);
    munit_assert(NULL != objects);

    for (uint32_t index = 0; index < ObjectCount; index++) {
        do {
            objects[index].inode = (fuse_ino_t) random();
        } while(0 == objects[index].inode);

        uuid_generate(objects[index].uuid);
        objects[index].lookups = 1;
        objects[index].releases = 0;
        fobj = FinesseObjectCreate(Table, objects[index].inode, &objects[index].uuid);
        munit_assert(NULL != fobj);
        munit_assert(fobj->inode == objects[index].inode);
        munit_assert(0 == uuid_compare(fobj->uuid, objects[index].uuid));
    }

    return objects;
}

static void lookup_multiple_objects_sequential(finesse_object_table_t *Table, test_object_t *Objects, unsigned Count)
{
    (void) Table;
    (void) Objects;
    (void) Count;

    munit_assert(0);
}

static void lookup_multiple_objects_random(finesse_object_table_t *Table, test_object_t *Objects, unsigned Count, unsigned Iterations)
{
    (void) Table;
    (void) Objects;
    (void) Count;
    (void) Iterations;

    munit_assert(0);
}

static void release_multiple_objects(finesse_object_table_t *Table, test_object_t *Objects, unsigned Count)
{

    finesse_object_t *fobj = NULL;
    finesse_object_t tobj;

    for (unsigned index = 0; index < Count; index++) {
        munit_assert(Objects[index].lookups >= Objects[index].releases);

        while (Objects[index].lookups > Objects[index].releases) {
            tobj.inode = Objects[index].inode;
            uuid_copy(tobj.uuid, Objects[index].uuid);
            FinesseObjectRelease(Table, &tobj);
            Objects[index].releases++;
        }

        fobj = FinesseObjectLookupByIno(Table, Objects[index].inode);
        munit_assert(NULL == fobj);
        fobj = FinesseObjectLookupByUuid(Table, &Objects[index].uuid);
        munit_assert(NULL == fobj);

    }

    free(Objects);
}


static
MunitResult
test_table_insert(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    finesse_object_table_t *table = NULL;
    test_object_t *objects = NULL;
    test_object_t *tobj = NULL;
    finesse_object_t fobj;
    finesse_object_t *tfobj;
    uuid_t scratch_uuid;
    const unsigned count = 1024;

    // First, a simple test
    table = FinesseCreateTable(0);
    munit_assert(NULL != table);

    tobj = insert_multiple_objects(table, 1);
    uuid_copy(fobj.uuid, tobj->uuid);
    fobj.inode = tobj->inode;
    FinesseObjectRelease(table, &fobj);

    free(tobj);
    tobj = NULL;

    // Now, let's create an object and make sure we can look it up
    
    objects = insert_multiple_objects(table, 1);

    tfobj = FinesseObjectLookupByIno(table, objects[0].inode);
    munit_assert(NULL != tfobj);
    munit_assert(tfobj->inode == objects[0].inode);
    munit_assert(0 == uuid_compare(tfobj->uuid, objects[0].uuid));
    objects[0].lookups++;

    tfobj = FinesseObjectLookupByIno(table, ~objects[0].inode);
    munit_assert(NULL == tfobj);

    tfobj = FinesseObjectLookupByUuid(table, &objects[0].uuid);
    munit_assert(NULL != tfobj);
    munit_assert(tfobj->inode == objects[0].inode);
    munit_assert(0 == uuid_compare(tfobj->uuid, objects[0].uuid));
    uuid_generate(scratch_uuid);
    objects[0].lookups++;

    fobj.inode = objects[0].inode;
    uuid_copy(fobj.uuid, objects[0].uuid);
    FinesseObjectRelease(table, &fobj);
    objects[0].releases++;

    tfobj = FinesseObjectLookupByUuid(table, &scratch_uuid);
    munit_assert(NULL == tfobj);

    fobj.inode = objects[0].inode;
    uuid_copy(fobj.uuid, objects[0].uuid);
    FinesseObjectRelease(table, &fobj);
    objects[0].releases++;
    munit_assert(objects[0].releases < objects[0].lookups);

    fobj.inode = objects[0].inode;
    uuid_copy(fobj.uuid, objects[0].uuid);
    FinesseObjectRelease(table, &fobj);
    objects[0].releases++;

    munit_assert(objects[0].lookups == objects[0].releases);

    tfobj = FinesseObjectLookupByIno(table, objects[0].inode);
    munit_assert(NULL == tfobj);
    tfobj = FinesseObjectLookupByUuid(table, &objects[0].uuid);
    munit_assert(NULL == tfobj);

    free(objects);
    tfobj = NULL;

    objects = insert_multiple_objects(table, count);
    munit_assert(NULL != objects);

    // Cleanup
    release_multiple_objects(table, objects, count);
    objects = NULL;

    // Now destroy the table
    FinesseDestroyTable(table);

    return MUNIT_OK;

    lookup_multiple_objects_sequential(table, objects, count);

    lookup_multiple_objects_random(table, objects, count, count * 10);

    return MUNIT_OK;

}


static
MunitResult
test_hash(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    const uint32_t check_crc32c = 0xe3069283;
    const char *check_crc32c_str = "123456789";

    munit_assert(check_crc32c == crc32c(0, check_crc32c_str, strlen(check_crc32c_str)));

    // TODO: add a check for the murmurhash...

    return MUNIT_OK;
}



static MunitTest tests[] = {
    TEST("/null", test_null, NULL),
    TEST("/hash", test_hash, NULL),
    TEST("/basics", test_table_basics, NULL),
    TEST("/insert", test_table_insert, NULL),
    TEST(NULL, NULL, NULL),
};


const MunitSuite fastlookup_suite = {
    .prefix = (char *)(uintptr_t)"/fastlookup",
    .tests = tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};

