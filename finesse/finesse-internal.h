/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <aio.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include "finesse.h"

#ifndef offsetof
#define offsetof(_struct, _field) __builtin_offsetof(_struct, _field)
#endif

#ifndef containerof
#define containerof(_ptr, _struct, _field)                              \
    (typeof (_struct *))(                                               \
        (intptr_t)(_ptr) - offsetof(typeof(_struct), _field))
#endif


// The purpose of this header file is to describe functions and data structures 
// used internally by the nicfs library.

typedef struct _finesse_key {
    enum {
        finesse_KEY_TYPE_UUID = 6,
    } type;
    union {
        uuid_t uuid;
    } data;
} finesse_key_t;


/*
 * this encapsulates per-file state information
 */
typedef struct _finesse_file_state {
    int fd;                // process local file descriptor
    finesse_key_t key;       // the remote key assigned by the finesse version of FUSE
    char *pathname;        // this is a captured copy of the path
    size_t current_offset; // this is the current byte offset (for read/write)
} finesse_file_state_t;

extern finesse_client_handle_t finesse_client_handle;

extern int finesse_init_file_state_mgr(void);
extern void finesse_terminate_file_state_mgr(void);
extern finesse_file_state_t *finesse_create_file_state(int fd, finesse_key_t *key, const char *pathname);
extern finesse_file_state_t *finesse_lookup_file_state(int fd);
extern void finesse_update_offset(finesse_file_state_t *file_state, size_t offset);
extern void finesse_delete_file_state(finesse_file_state_t *file_state);

extern int finesse_fd_to_nfd(int fd);
extern int finesse_nfd_to_fd(int nfd);

extern void finesse_insert_new_fd(int fd, const char *path);

int finesse_call_server(void *request, size_t req_length, void **response, size_t *rsp_length);
void finesse_free_response(void *response);
void finesse_server_close_connection(void);
void finesse_server_open_connection(void);
int finesse_map_name(const char *mapfile_name, uuid_t *uuid);
int finesse_set_client_uuid(uuid_t *uuid);
unsigned finesse_generate_messageid(void); 
int finesse_test_server(void);

