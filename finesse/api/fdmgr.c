/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#include "api-internal.h"
#include "list.h"


#if !defined(offsetof)
#define offsetof(type, member)  __builtin_offsetof (type, member)
#endif // offsetof

/*
 * The purpose of the file descriptor manager is to provide a mechanism for mapping file descriptors to paths, since
 * the FUSE layer requires paths and not FDs.
 *
 * We do this via a table (of course!)
 */

#if !defined(container_of)
#define container_of(ptr, type, member) ({ \
    typeof( ((type *)0)->member ) \
    *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type,member) );})
#endif // container_of

typedef uint32_t (*lookup_table_hash_t)(void *key, size_t length);

/*
 * Note: at the present time this is set up for a single lock on the table
 *       this should be sufficient for single process use, but if not,
 *       this could be split into per-bucket locks for greater parallelism.
 */
typedef struct lookup_table {
    unsigned char       EntryCountShift;
    unsigned char       Name[7];
    pthread_rwlock_t    TableLock; /* protects table against changes */
    lookup_table_hash_t Hash;
    size_t              KeySize;
    struct list         TableBuckets[1];
} lookup_table_t;

typedef struct lookup_table_entry {
    struct list   ListEntry;
    void         *Object;
    unsigned char Key[1];
} lookup_table_entry_t, *plookup_table_entry_t;

static lookup_table_entry_t *lookup_table_entry_create(void *key, size_t key_size, void *object)
{
    size_t size = (offsetof(lookup_table_entry_t, Key) + key_size + 0x7) & ~0x7;
    lookup_table_entry_t *new_entry = malloc(size);

    while (NULL != new_entry) {
        new_entry->Object = object;
        memcpy(new_entry->Key, key, key_size);
        break;
    }

    return new_entry;
}

static void lookup_table_entry_destroy(lookup_table_entry_t *DeadEntry)
{
    free(DeadEntry);
}

/* simple generic hash */
static uint32_t default_hash(void *key, size_t length)
{
    uint32_t hash = ~0;
    const char *blob = (const char *)key;

    for (unsigned char index = 0; index < length; index += sizeof(uint32_t)) {
        hash ^= *(const uint32_t *)&blob[index];
    }

    return hash;
}

static uint32_t lookup_table_hash(lookup_table_t *Table, void *Key)
{
    return (Table->Hash(Key, Table->KeySize) & ((1<<Table->EntryCountShift)-1));
}

static
lookup_table_t *
lookup_table_create(unsigned int SizeHint, const char *Name, lookup_table_hash_t Hash, size_t KeySize)
{
    lookup_table_t *table = malloc(sizeof(struct lookup_table));
    unsigned char entry_count_shift = 0;
    unsigned entrycount;

    if (SizeHint > 65536) {
        SizeHint = 65536;
    }

    while (((unsigned int)(1<<entry_count_shift)) < SizeHint) {
        entry_count_shift++;
    }

    entrycount = 1 << entry_count_shift;

    table = malloc(offsetof(struct lookup_table, TableBuckets) + (sizeof(struct list) * entrycount));

    while (NULL != table) {
        table->EntryCountShift = entry_count_shift;
        memcpy(table->Name, Name, 7);
        table->Hash = Hash ? Hash : default_hash;
        table->KeySize = KeySize;
        pthread_rwlock_init(&table->TableLock, NULL);

        for (unsigned index = 0; index < entrycount; index++) {
            table->TableBuckets[index].prv = table->TableBuckets[index].nxt = &table->TableBuckets[index];
        }
        break;
    }

    return table;
}

static
void lookup_table_destroy(lookup_table_t *Table)
{
    unsigned bucket_index = 0;
    lookup_table_entry_t *table_entry;

    pthread_rwlock_wrlock(&Table->TableLock);

    for (bucket_index = 0; bucket_index < (unsigned) (1<<Table->EntryCountShift); bucket_index++) {
        while (!list_is_empty(&Table->TableBuckets[bucket_index])) {
            table_entry = container_of(list_head(&Table->TableBuckets[bucket_index]), struct lookup_table_entry, ListEntry);
            list_remove(&table_entry->ListEntry);
            lookup_table_entry_destroy(table_entry);
        }
    }

    pthread_rwlock_unlock(&Table->TableLock);

    free(Table);

    return;
}

static struct lookup_table_entry *lookup_table_locked(lookup_table_t *Table, void *Key)
{
    uint32_t bucket_index = lookup_table_hash(Table, Key);
    struct lookup_table_entry *table_entry = NULL;
    struct list *le;

    list_for_each(&Table->TableBuckets[bucket_index], le) {
        table_entry = container_of(le, struct lookup_table_entry, ListEntry);
        if (0 == memcmp(Key, table_entry->Key, Table->KeySize)) {
            return table_entry;
        }
    }

    return NULL;

}

static
int
lookup_table_insert(lookup_table_t *Table, void *Key, void *Object)
{
    lookup_table_entry_t *entry = lookup_table_entry_create(Key, Table->KeySize, Object);
    int status = ENOMEM;
    uint32_t bucket_index = lookup_table_hash(Table, Key);
    struct lookup_table_entry *table_entry = NULL;

    while (NULL != entry) {

        pthread_rwlock_wrlock(&Table->TableLock);
        table_entry = lookup_table_locked(Table, Key);

        if (table_entry) {
            status = EEXIST;
        }
        else {
            list_insert_tail(&Table->TableBuckets[bucket_index], &entry->ListEntry);
            status = 0;
        }
        pthread_rwlock_unlock(&Table->TableLock);

        break;
    }

    if (0 != status) {
        if (NULL != entry) {
            free(entry);
            entry = NULL;
        }
    }

    return status;
}

static
int
lookup_table_lookup(lookup_table_t *Table, void *Key, void **Object)
{
    struct lookup_table_entry *entry;

    pthread_rwlock_rdlock(&Table->TableLock);
    entry = lookup_table_locked(Table, Key);
    pthread_rwlock_unlock(&Table->TableLock);

    if (entry) {
        *Object = entry->Object;
    }
    else {
        *Object = NULL;
    }

    return NULL == entry ? ENODATA : 0;
}

static int lookup_table_remove(lookup_table_t *Table, void *Key)
{
    struct lookup_table_entry *entry;
    int status = ENODATA;

    pthread_rwlock_wrlock(&Table->TableLock);
    entry = lookup_table_locked(Table, Key);

    if (entry) {
        list_remove(&entry->ListEntry);
    }
    pthread_rwlock_unlock(&Table->TableLock);

    if (entry) {
        lookup_table_entry_destroy(entry);
        status = 0;
    }

    return status;

}


/* local lookup table based on file descriptors */
/* static */ lookup_table_t *fd_lookup_table;

/*
 * Note: at the present time, the finesse_file_state_t structure is **Not** reference counted.
 *       Instead, it relies upon the open/close management logic to know when it is time
 *       to delete the state.
 */

finesse_file_state_t *finesse_create_file_state(int fd, uuid_t *key, const char *pathname)
{
    size_t pathlen = strlen(pathname) + sizeof('\0');
    size_t size = (sizeof(finesse_file_state_t) + pathlen + 0x7) & ~0x7;
    finesse_file_state_t *file_state;
    int status;

    assert(fd_lookup_table);

    file_state = malloc(size);
    while (NULL != file_state) {
        file_state->fd = fd;
        memcpy(&file_state->key, key, sizeof(uuid_t));
        file_state->pathname = (char *)(file_state+1);
        strncpy(file_state->pathname, pathname, pathlen);
        file_state->current_offset = 0;

        // Try to insert it
        status = lookup_table_insert(fd_lookup_table, &fd, file_state);

        if (0 != status) {
            // note: we *could* do a lookup in case of a collision, but we shouldn't need to do so
            free(file_state);
            file_state = NULL;
            break;
        }

        /* done */
        break;
    }

    return file_state;
}

finesse_file_state_t *finesse_lookup_file_state(int fd)
{
    finesse_file_state_t *file_state;
    int status;
    
    assert(fd_lookup_table);
    status = lookup_table_lookup(fd_lookup_table, &fd, (void **)&file_state);

    if (0 != status) {
        file_state = NULL;
    }

    return file_state;
}

void finesse_update_offset(finesse_file_state_t *file_state, size_t offset)
{
    file_state->current_offset = offset;
}

void finesse_delete_file_state(finesse_file_state_t *file_state)
{
    int fd;
    int status; 

    assert(fd_lookup_table);
    fd = file_state->fd;
    status = lookup_table_remove(fd_lookup_table, &fd);
    assert(0 == status);
    if (0 != status) {
        return;
    }

    // cleanup the file state
    free(file_state);
    
}

int finesse_init_file_state_mgr(void)
{
    lookup_table_t *new_table = NULL;
    int status = 0;

    while (NULL == fd_lookup_table) {
        /* 
         * table size: this is a speed/space trade-off.  I did a quick run with various power-of-two values with
         * a test of 64K file descriptors (which is the max on my linux box at this time for a single process).
         * I don't want to add dynamic resizing into the mix, though someone _could_ do so, if they thought it
         * worth the cost (I've seldom found it to be worthwhile).  Another approach would be to use a more
         * efficient secondary scheme, such as b-tree or AVL tree.  That still gives good parallelism across
         * buckets.
         *
         * My test iteratively created 64K entries, then looked each of them up 100 times.  I picked the value
         * that seemed to be a reasonable compromise between space and efficiency.
         *
         * 8192 -  2.420 seconds
         * 4096 -  3.022 seconds
         * 2048 -  4.050 seconds
         * 1024 -  7.654 seconds
         *  512 - 14.118 seconds
         * 
         */
        new_table = lookup_table_create(4096, "nicfd", NULL, sizeof(int));

        if (NULL == new_table) {
            status = ENOMEM;
            break;
        }

        if (!__sync_bool_compare_and_swap(&fd_lookup_table, NULL, new_table)) {
            // presumably a race that we've lost
            lookup_table_destroy(new_table);
            new_table = NULL;
            status = EINVAL;
            break;
        }

        break;
    }

    return status;
}

void finesse_terminate_file_state_mgr(void)
{
    lookup_table_t *existing_table = fd_lookup_table;

    if (NULL != existing_table) {
        if (__sync_bool_compare_and_swap(&fd_lookup_table, existing_table, NULL)) {
            lookup_table_destroy(existing_table);
        }
        /* else: don't do anything because it changed and someone else must be doing something */
    }

}

//
// We can map this into a different space for testing purposes; otherwise, we just leave it alone.
// Testing: find calls that are bypassing the library.
//
#if defined(NIC_FD_SHIFT)
const unsigned int nic_fd_shift = (unsigned int) (1 << ((sizeof(int)) * 8 - 1));
#else
const unsigned int nic_fd_shift = 0;
#endif // NIC_FD_SHIFT

int finesse_fd_to_nfd(int fd)
{
    unsigned int nfd = ((unsigned int) fd) | nic_fd_shift;

    return nfd;
}

int finesse_nfd_to_fd(int nfd)
{
    unsigned fd = ((unsigned int ) nfd) & (~nic_fd_shift);

    return fd;
}


//
// This inserts a file descriptor into the table if possible.  If not, nothing happens
//
void finesse_insert_new_fd(int fd, const char *path)
{
    (void) fd;
    (void) path;
#if 0
    finesse_key_t *finesse_key = malloc(sizeof(finesse_key_t));

    (void) fd;
    (void) path;

    if (NULL == fd_lookup_table) {
        finesse_init_file_state_mgr();

        if (NULL == fd_lookup_table) {
            return;
        }
        (void) finesse_key;
    }

    /* call the FUSE implementation and ask for an ID for this file */
#endif // 0

}