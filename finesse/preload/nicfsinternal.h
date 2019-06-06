/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#include "nicfs.h"
//  #include "nictypes.h"
#include "niccolum_msg.h"
#include <pthread.h>
#include <uuid/uuid.h>

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

typedef struct _nicfs_key {
    enum {
        NICFS_KEY_TYPE_UUID = 6,
    } type;
    union {
        uuid_t uuid;
    } data;
} nicfs_key_t;


/*
 * this encapsulates per-file state information
 */
typedef struct _nicfs_file_state {
    int fd;                // process local file descriptor
    nicfs_key_t key;       // the remote key assigned by the Niccolum version of FUSE
    char *pathname;        // this is a captured copy of the path
    size_t current_offset; // this is the current byte offset (for read/write)
} nicfs_file_state_t;

extern int nicfs_init_file_state_mgr(void);
extern void nicfs_terminate_file_state_mgr(void);
extern nicfs_file_state_t *nicfs_create_file_state(int fd, nicfs_key_t *key, const char *pathname);
extern nicfs_file_state_t *nicfs_lookup_file_state(int fd);
extern void nicfs_update_offset(nicfs_file_state_t *file_state, size_t offset);
extern void nicfs_delete_file_state(nicfs_file_state_t *file_state);

extern int nicfs_fd_to_nfd(int fd);
extern int nicfs_nfd_to_fd(int nfd);

extern void nicfs_insert_new_fd(int fd, const char *path);

int nicfs_call_server(void *request, size_t req_length, void **response, size_t *rsp_length);
void nicfs_free_response(void *response);
void nicfs_server_close_connection(void);
void nicfs_server_open_connection(void);
int nicfs_map_name(const char *mapfile_name, uuid_t *uuid);
int nicfs_set_client_uuid(niccolum_uuid_t *uuid);
unsigned nicfs_generate_messageid(void); 
int nicfs_test_server(void);

