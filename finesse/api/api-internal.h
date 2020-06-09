/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#if !defined(__API_INTERNAL_H__)
#define __API_INTERNAL_H__

#include <finesse.h>
#include <dlfcn.h>

typedef struct _finesse_file_state {
    int fd;                // process local file descriptor
    uuid_t key;            // the remote key assigned by the finesse version of FUSE
    void *client;          // this is the finesse client handle (opaque)
    char *pathname;        // this is a captured copy of the path
    size_t current_offset; // this is the current byte offset (for read/write)
} finesse_file_state_t;

int finesse_init_file_state_mgr(void);
void finesse_terminate_file_state_mgr(void);
finesse_file_state_t *finesse_create_file_state(int fd, void *client, uuid_t *key, const char *pathname);
finesse_file_state_t *finesse_lookup_file_state(int fd);
void finesse_update_offset(finesse_file_state_t *file_state, size_t offset);
void finesse_delete_file_state(finesse_file_state_t *file_state);

int finesse_fd_to_nfd(int fd);
int finesse_nfd_to_fd(int nfd);

fuse_ino_t LookupInodeForKey(uuid_t *Key);

#endif // __API_INTERNAL_H__