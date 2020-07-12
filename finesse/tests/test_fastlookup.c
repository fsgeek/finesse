
/*
 * Copyright (c) 2017-2020, Tony Mason. All rights reserved.
 */

#define _GNU_SOURCE

#include <finesse-fuse.h>
#include <stdlib.h>
#include <uuid/uuid.h>
#include "../include/crc32c.h"
#include "../include/murmurhash3.h"
#include "test_utils.h"

#define __packed __attribute__((packed))
#define __notused __attribute__((unused))

static MunitResult test_table_basics(const MunitParameter params[] __notused, void *prv __notused)
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
    fuse_ino_t       inode;
    uuid_t           uuid;
    unsigned         lookups;
    unsigned         releases;
    pthread_rwlock_t Lock;
} test_object_t;

static void lock_test_object(test_object_t *Object, int Exclusive)
{
    if (Exclusive) {
        pthread_rwlock_wrlock(&Object->Lock);
    }
    else {
        pthread_rwlock_rdlock(&Object->Lock);
    }
}

static void unlock_test_object(test_object_t *Object)
{
    pthread_rwlock_unlock(&Object->Lock);
}

static test_object_t *insert_multiple_objects(finesse_object_table_t *Table, uint32_t ObjectCount)
{
    test_object_t *   objects = NULL;
    finesse_object_t *fobj    = NULL;

    munit_assert(NULL != Table);
    munit_assert(ObjectCount < (1024 * 1024 * 1024));

    objects = (test_object_t *)malloc(sizeof(test_object_t) * ObjectCount);
    munit_assert(NULL != objects);

    for (uint32_t index = 0; index < ObjectCount; index++) {
        do {
            objects[index].inode = (fuse_ino_t)random();
            pthread_rwlock_init(&objects[index].Lock, NULL);
        } while (0 == objects[index].inode);

        uuid_generate(objects[index].uuid);
        objects[index].lookups  = 1;
        objects[index].releases = 0;
        fobj                    = FinesseObjectCreate(Table, objects[index].inode, &objects[index].uuid);
        munit_assert(NULL != fobj);
        munit_assert(fobj->inode == objects[index].inode);
        munit_assert(0 == uuid_compare(fobj->uuid, objects[index].uuid));
    }

    return objects;
}

static void lookup_multiple_objects_sequential(finesse_object_table_t *Table, test_object_t *Objects, unsigned Count,
                                               unsigned Iterations)
{
    unsigned          iterations = 0;
    unsigned          index      = 0;
    finesse_object_t *fo;
    int               inode_lookup = 0;
    unsigned          count        = 0;

    while (iterations++ < Iterations) {
        count = 0;

        for (index = 0; index < Count; index++) {
            lock_test_object(&Objects[index], 0);

            if (inode_lookup) {
                fo = FinesseObjectLookupByIno(Table, Objects[index].inode);
            }
            else {
                fo = FinesseObjectLookupByUuid(Table, &Objects[index].uuid);
            }

            if (NULL != fo) {
                // we switch back and forth between inode and uuid lookup
                inode_lookup = !inode_lookup;

                // This is a successful lookup
                Objects[index].lookups++;
                count++;
            }

            munit_assert((NULL != fo) || (Objects[index].lookups == Objects[index].releases));
            unlock_test_object(&Objects[index]);
        }
    }
}

static void lookup_multiple_objects_random(finesse_object_table_t *Table, test_object_t *Objects, unsigned Count,
                                           unsigned Iterations)
{
    unsigned          iterations = 0;
    unsigned          index      = 0;
    finesse_object_t *fo;
    int               inode_lookup = 0;
    unsigned          count        = 0;

    while (iterations++ < Iterations) {
        count = 0;

        while (count < Count) {
            index = random() % Count;

            lock_test_object(&Objects[index], 0);
            if (inode_lookup) {
                fo = FinesseObjectLookupByIno(Table, Objects[index].inode);
            }
            else {
                fo = FinesseObjectLookupByUuid(Table, &Objects[index].uuid);
            }

            if (NULL != fo) {
                // we switch back and forth between inode and uuid lookup
                inode_lookup = !inode_lookup;

                // This is a successful lookup
                Objects[index].lookups++;
                count++;
            }

            munit_assert((NULL != fo) || (Objects[index].lookups == Objects[index].releases));
            unlock_test_object(&Objects[index]);
        }
    }
}

static void release_multiple_objects(finesse_object_table_t *Table, test_object_t *Objects, unsigned Count)
{
    finesse_object_t *fobj = NULL;
    finesse_object_t  tobj;

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

        pthread_rwlock_destroy(&Objects[index].Lock);
    }

    free(Objects);
}

static MunitResult test_table_insert(const MunitParameter params[] __notused, void *prv __notused)
{
    finesse_object_table_t *table   = NULL;
    test_object_t *         objects = NULL;
    test_object_t *         tobj    = NULL;
    finesse_object_t        fobj;
    finesse_object_t *      tfobj;
    uuid_t                  scratch_uuid;
    const unsigned          count = 1024;

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

    lookup_multiple_objects_sequential(table, objects, count, count * 10);

    lookup_multiple_objects_random(table, objects, count, count * 10);

    return MUNIT_OK;
}

static MunitResult test_hash(const MunitParameter params[] __notused, void *prv __notused)
{
    const uint32_t check_crc32c     = 0xe3069283;
    const char *   check_crc32c_str = "123456789";

    munit_assert(check_crc32c == crc32c(0, check_crc32c_str, strlen(check_crc32c_str)));

    // TODO: add a check for the murmurhash...

    return MUNIT_OK;
}

typedef struct {
    finesse_object_table_t *Table;
    test_object_t *         Objects;
    unsigned                ObjectCount;
    unsigned                Iterations;
} fl_params_t;

static void *deleter(void *arg)
{
    fl_params_t *    parameters = (arg);
    unsigned         iterations = 0;
    unsigned         index      = 0;
    finesse_object_t fo;

    munit_assert(NULL != parameters);

    while (iterations < parameters->Iterations) {
        index = random() % parameters->ObjectCount;

        lock_test_object(&parameters->Objects[index], 1);
        if (parameters->Objects[index].lookups > parameters->Objects[index].releases) {
            fo.inode = parameters->Objects[index].inode;
            uuid_copy(fo.uuid, parameters->Objects[index].uuid);
            FinesseObjectRelease(parameters->Table, &fo);
            parameters->Objects[index].releases++;
            iterations++;
        }
        unlock_test_object(&parameters->Objects[index]);
    }

    pthread_exit(arg);
}

static void *ino_reader(void *arg)
{
    fl_params_t *     parameters = (arg);
    unsigned          iterations = 0;
    unsigned          index      = 0;
    finesse_object_t *fo;

    munit_assert(NULL != parameters);

    while (iterations < parameters->Iterations) {
        index = random() % parameters->ObjectCount;

        lock_test_object(&parameters->Objects[index], 0);
        fo = FinesseObjectLookupByIno(parameters->Table, parameters->Objects[index].inode);

        munit_assert((NULL != fo) || (parameters->Objects[index].lookups == parameters->Objects[index].releases));
        if (NULL != fo) {
            // This is a lookup
            parameters->Objects[index].lookups++;
            iterations++;
        }
        unlock_test_object(&parameters->Objects[index]);
    }

    pthread_exit(arg);
}

static void *uuid_reader(void *arg)
{
    fl_params_t *     parameters = (arg);
    unsigned          iterations = 0;
    unsigned          index      = 0;
    finesse_object_t *fo;

    munit_assert(NULL != parameters);

    while (iterations < parameters->Iterations) {
        index = random() % parameters->ObjectCount;

        lock_test_object(&parameters->Objects[index], 0);
        fo = FinesseObjectLookupByUuid(parameters->Table, &parameters->Objects[index].uuid);

        munit_assert((NULL != fo) || (parameters->Objects[index].lookups == parameters->Objects[index].releases));
        if (NULL != fo) {
            // This is a lookup
            parameters->Objects[index].lookups++;
            iterations++;
        }
        unlock_test_object(&parameters->Objects[index]);
    }

    pthread_exit(arg);
}

static MunitResult test_mt(const MunitParameter params[] __notused, void *prv __notused)
{
    fl_params_t             p;
    finesse_object_table_t *table   = NULL;
    test_object_t *         objects = NULL;
    const unsigned          count   = 3;
    pthread_t               del_thread;
    pthread_t               ino_thread;
    pthread_t               uuid_thread;
    int                     status;
    void *                  thread_return = NULL;

    // First, a simple test
    table = FinesseCreateTable(0);
    munit_assert(NULL != table);

    objects = insert_multiple_objects(table, count);
    munit_assert(NULL != objects);

    p.Table       = table;
    p.Iterations  = 102400;
    p.ObjectCount = count;
    p.Objects     = objects;

    status = pthread_create(&ino_thread, NULL, ino_reader, &p);
    assert(0 == status);

    status = pthread_create(&uuid_thread, NULL, uuid_reader, &p);
    assert(0 == status);

    status = pthread_create(&del_thread, NULL, deleter, &p);
    assert(0 == status);

    status = pthread_join(del_thread, &thread_return);
    munit_assert(0 == status);
    munit_assert(&p == thread_return);

    status = pthread_join(uuid_thread, &thread_return);
    munit_assert(0 == status);
    munit_assert(&p == thread_return);

    status = pthread_join(ino_thread, &thread_return);
    munit_assert(0 == status);
    munit_assert(&p == thread_return);

    release_multiple_objects(table, objects, count);

    FinesseDestroyTable(table);

    return MUNIT_OK;
}

static MunitTest tests[] = {
    TEST("/null", test_null, NULL),           TEST("/hash", test_hash, NULL), TEST("/basics", test_table_basics, NULL),
    TEST("/insert", test_table_insert, NULL), TEST("/mt", test_mt, NULL),     TEST(NULL, NULL, NULL),
};

const MunitSuite fastlookup_suite = {
    .prefix     = (char *)(uintptr_t) "/fastlookup",
    .tests      = tests,
    .suites     = NULL,
    .iterations = 1,
    .options    = MUNIT_SUITE_OPTION_NONE,
};
