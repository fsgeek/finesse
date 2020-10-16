
/*
 * Copyright (c) 2017-2020, Tony Mason. All rights reserved.
 */

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE /* See feature_test_macros(7) */
#endif              // _GNU_SOURCE

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
    finesse_object_t *fobj2   = NULL;

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
        FinesseObjectRelease(Table, fobj);
        fobj2 = FinesseObjectLookupByIno(Table, objects[index].inode);
        munit_assert(fobj == fobj2);
        FinesseObjectRelease(Table, fobj2);
        fobj2 = NULL;
    }

    return objects;
}

static finesse_object_t *lookup_by_inode(finesse_object_table_t *Table, test_object_t *Objects, unsigned Index)
{
    finesse_object_t *fo = FinesseObjectLookupByIno(Table, Objects[Index].inode);

    if (NULL != fo) {
        Objects[Index].lookups++;
    }

    return fo;
}

static finesse_object_t *lookup_by_uuid(finesse_object_table_t *Table, test_object_t *Objects, unsigned Index)
{
    finesse_object_t *fo = FinesseObjectLookupByUuid(Table, &Objects[Index].uuid);

    if (NULL != fo) {
        Objects[Index].lookups++;
    }

    return fo;
}

static void release(finesse_object_table_t *Table, test_object_t *Objects, unsigned Index)
{
    finesse_object_t fo;

    fo.inode = Objects[Index].inode;
    uuid_copy(fo.uuid, Objects[Index].uuid);

    if (Objects[Index].releases >= Objects[Index].lookups) {
        fprintf(stderr, "Invalid release");
    }

    munit_assert(Objects[Index].releases < Objects[Index].lookups);
    FinesseObjectRelease(Table, &fo);
    Objects[Index].releases++;
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
                fo = lookup_by_inode(Table, Objects, index);
            }
            else {
                fo = lookup_by_uuid(Table, Objects, index);
            }

            if (NULL != fo) {
                // we switch back and forth between inode and uuid lookup
                inode_lookup = !inode_lookup;

                // This is a successful lookup
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
                fo = lookup_by_inode(Table, Objects, index);
            }
            else {
                fo = lookup_by_uuid(Table, Objects, index);
            }

            if (NULL != fo) {
                // we switch back and forth between inode and uuid lookup
                inode_lookup = !inode_lookup;

                // This is a successful lookup
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
    unsigned          off = 0;

    for (unsigned index = 0; index < Count; index++) {
        munit_assert(Objects[index].lookups >= Objects[index].releases);

        while (Objects[index].lookups > Objects[index].releases) {
            tobj.inode = Objects[index].inode;
            uuid_copy(tobj.uuid, Objects[index].uuid);
            release(Table, Objects, index);
        }

        fobj = lookup_by_inode(Table, Objects, index);
        if (NULL != fobj) {
            while (NULL != fobj) {
                off++;
                Objects[index].lookups++;
                release(Table, Objects, index);
                release(Table, Objects, index);
                fobj = lookup_by_inode(Table, Objects, index);
            }
            fobj = lookup_by_inode(Table, Objects, index);
        }
        else {
            munit_assert(NULL == fobj);
        }
        fobj = lookup_by_uuid(Table, Objects, index);
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
    finesse_object_t *      tfobj;
    const unsigned          count = 1024;

    // First, a simple test
    table = FinesseCreateTable(0);
    munit_assert(NULL != table);

    tobj = insert_multiple_objects(table, 1);
    release(table, tobj, 0);
    free(tobj);
    tobj = NULL;

    // Now, let's create an object and make sure we can look it up

    objects = insert_multiple_objects(table, 1);

    tfobj = lookup_by_inode(table, objects, 0);
    munit_assert(NULL != tfobj);
    munit_assert(tfobj->inode == objects[0].inode);
    munit_assert(0 == uuid_compare(tfobj->uuid, objects[0].uuid));
    release(table, objects, 0);

    tfobj = lookup_by_uuid(table, objects, 0);
    munit_assert(NULL != tfobj);
    munit_assert(tfobj->inode == objects[0].inode);
    munit_assert(0 == uuid_compare(tfobj->uuid, objects[0].uuid));
    release(table, objects, 0);

    // this is the release for the original create
    release(table, objects, 0);

    // This should be the last release
    munit_assert(objects[0].lookups == objects[0].releases);
    tfobj = NULL;

    // make sure we can't find it by inode
    tfobj = lookup_by_inode(table, objects, 0);
    munit_assert(NULL == tfobj);

    // make sure we can't find it by uuid
    tfobj = lookup_by_uuid(table, objects, 0);
    munit_assert(NULL == tfobj);

    free(objects);
    objects = NULL;

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

static MunitResult test_collision(const MunitParameter params[] __notused, void *prv __notused)
{
    finesse_object_t *      fobj  = NULL;
    finesse_object_t *      tobj  = NULL;
    finesse_object_table_t *table = NULL;
    uuid_t                  test_uuid;
    uuid_t                  test_uuid2;

    // First, a simple test
    table = FinesseCreateTable(0);
    munit_assert(NULL != table);

    uuid_generate(test_uuid);
    munit_assert(!uuid_is_null(test_uuid));

    fobj = FinesseObjectCreate(table, 2, &test_uuid);
    munit_assert(NULL != fobj);

    tobj = FinesseObjectCreate(table, 2, &test_uuid2);
    munit_assert(NULL != tobj);

    munit_assert(0 == uuid_compare(fobj->uuid, tobj->uuid));

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
            release(parameters->Table, parameters->Objects, index);
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
        fo = lookup_by_inode(parameters->Table, parameters->Objects, index);

        munit_assert((NULL != fo) || (parameters->Objects[index].lookups == parameters->Objects[index].releases));
        if (NULL != fo) {
            // This is a lookup
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
        fo = lookup_by_uuid(parameters->Table, parameters->Objects, index);

        munit_assert((NULL != fo) || (parameters->Objects[index].lookups == parameters->Objects[index].releases));
        if (NULL != fo) {
            // This is a lookup
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

static MunitResult test_refcount(const MunitParameter params[] __notused, void *prv __notused)
{
#if 0
    finesse_object_table_t *table   = NULL;
    test_object_t *         objects = NULL;
    test_object_t *         tobj    = NULL;
    finesse_object_t *      tfobj;
    const unsigned          count = 1024;

    // First, a simple test
    table = FinesseCreateTable(0);
    munit_assert(NULL != table);

    tobj = insert_multiple_objects(table, 1);
    release(table, tobj, 0);
    free(tobj);
    tobj = NULL;

    // Now, let's create an object and make sure we can look it up

    objects = insert_multiple_objects(table, 1);

    tfobj = lookup_by_inode(table, objects, 0);
    munit_assert(NULL != tfobj);
    munit_assert(tfobj->inode == objects[0].inode);
    munit_assert(0 == uuid_compare(tfobj->uuid, objects[0].uuid));
    release(table, objects, 0);

    tfobj = lookup_by_uuid(table, objects, 0);
    munit_assert(NULL != tfobj);
    munit_assert(tfobj->inode == objects[0].inode);
    munit_assert(0 == uuid_compare(tfobj->uuid, objects[0].uuid));
    release(table, objects, 0);

    // this is the release for the original create
    release(table, objects, 0);

    // This should be the last release
    munit_assert(objects[0].lookups == objects[0].releases);
    tfobj = NULL;

    // make sure we can't find it by inode
    tfobj = lookup_by_inode(table, objects, 0);
    munit_assert(NULL == tfobj);

    // make sure we can't find it by uuid
    tfobj = lookup_by_uuid(table, objects, 0);
    munit_assert(NULL == tfobj);

    free(objects);
    objects = NULL;

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
#endif  // 0

    return MUNIT_OK;
}

static MunitTest tests[] = {
    TEST("/null", test_null, NULL),           TEST("/hash", test_hash, NULL), TEST("/basics", test_table_basics, NULL),
    TEST("/insert", test_table_insert, NULL), TEST("/mt", test_mt, NULL),     TEST("/collision", test_collision, NULL),
    TEST("/refcount", test_refcount, NULL),   TEST(NULL, NULL, NULL),
};

const MunitSuite fastlookup_suite = {
    .prefix     = (char *)(uintptr_t) "/fastlookup",
    .tests      = tests,
    .suites     = NULL,
    .iterations = 1,
    .options    = MUNIT_SUITE_OPTION_NONE,
};
