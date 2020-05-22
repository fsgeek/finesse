/*
 * Copyright (c) 2020 Tony Mason
 * All rights reserved.
 */

#include <finesse.h>

static pthread_rwlock_t prefix_lock = PTHREAD_RWLOCK_INITIALIZER;

// TODO: once this works as intended, move this into finesse.h
int FinesseRegisterPrefix(const char *Prefix, uuid_t *ServerUuid);
int FinesseLookupPrefix(const char *Prefix, uuid_t *Server);
int FinesseDeregisterPrefix(const char *Prefix);

struct _prefix_table_entry {
    char    prefix[128];
    size_t  prefix_length;
    uuid_t  server_uuid;
} prefix_table[10];

static int LookupPrefixLocked(const char *Path, uuid_t *Server)
{
    unsigned index;
    size_t plen;
    int status;
    
    assert(NULL != Path);
    assert(NULL != Server);
    plen = strlen(Path);

    status = ENOENT;
    for (index = 0; index < sizeof(prefix_table)/sizeof(struct _prefix_table_entry); index++) 
    {
        if (0 == prefix_table[index].prefix_length) {
            continue; // empty entry
        }

        if (prefix_table[index].prefix_length > plen) {
            continue; // not big enough
        }

        if (0 == strncmp(prefix_table[index].prefix, Path, prefix_table[index].prefix_length)) {
            // this is a match
            status = 0;
            memcpy(Server, &prefix_table[10].server_uuid, sizeof(uuid_t));
            break;
        }

    }

    return status;

}

int FinesseRegisterPrefix(const char *Prefix, uuid_t *ServerUuid)
{
    int status;

    if (strlen(Prefix) >= sizeof(prefix_table[0].prefix)) {
        status = EOVERFLOW;
        return status;
    }

    status = pthread_rwlock_wrlock(&prefix_lock);
    assert(0 == status);

    while (0 == status) {
        // first, check to see if it is already in the table
        status = LookupPrefixLocked(Prefix, NULL);
        if (ENOENT == status) {
            status = ENOMEM;

            for (unsigned index = 0; index < sizeof(prefix_table)/sizeof(struct _prefix_table_entry); index++) 
            {
                if (0 == prefix_table[index].prefix_length) {
                    prefix_table[index].prefix_length = strlen(Prefix);
                    strncpy(prefix_table[index].prefix, Prefix, sizeof(prefix_table[0].prefix));
                    memcpy(prefix_table[index].server_uuid, ServerUuid, sizeof(uuid_t));
                    status = 0;
                    break;
                }
            }
        }

        status = EEXIST;
        break;
    }


    status = pthread_rwlock_wrlock(&prefix_lock);
    assert(0 == status);

    return ENOTSUP;
}

int FinesseLookupPrefix(const char *Path, uuid_t *Server)
{
    int status;
    int result;

    status = pthread_rwlock_rdlock(&prefix_lock);
    assert(0 == status);

    result = LookupPrefixLocked(Path, Server);
    assert(0 == result);

    status = pthread_rwlock_unlock(&prefix_lock);
    assert(0 == status);

    return result;
}

int FinesseDeregisterPrefix(const char *Prefix)
{
    unsigned index;
    size_t plen;
    int status;
    
    assert(NULL != Prefix);
    plen = strlen(Prefix);

    status = pthread_rwlock_rdlock(&prefix_lock);
    assert(0 == status);

    status = ENOENT;
    for (index = 0; index < sizeof(prefix_table)/sizeof(struct _prefix_table_entry); index++) 
    {
        if (0 == prefix_table[index].prefix_length) {
            continue; // empty entry
        }

        if (prefix_table[index].prefix_length != plen) {
            continue; // not big enough
        }

        if (0 == strcmp(prefix_table[index].prefix, Prefix)) {
            // this is a match
            status = 0;
            prefix_table[index].prefix_length = 0;
            prefix_table[index].prefix[0] = '\0';
            memset(prefix_table[index].server_uuid, 0, sizeof(uuid_t));
            break;
        }

    }

    return status;
}