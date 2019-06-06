/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#include "nicfs.h"
#include "nicfsinternal.h"
#include <pthread.h>

// this is the list of prefixes that we care about
// TODO: make this configuration or parameterizable
const char *nicfs_prefixes[] = { "/mnt/pt", NULL};

static void nicfs_dummy_init(void);
static void nicfs_real_init(void);
static void nicfs_dummy_shutdown(void);
static void nicfs_real_shutdown(void);

void (*nicfs_init)(void) = nicfs_real_init;
void (*nicfs_shutdown)(void) = nicfs_dummy_shutdown;

static void nicfs_dummy_init(void) 
{
    return;
}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void nicfs_real_init(void) 
{
    pthread_mutex_lock(&lock);

    while (nicfs_init == nicfs_real_init) {
        // Initialization logic goes here
        (void) nicfs_init_file_state_mgr();

        // now overwrite the init function
        nicfs_init = nicfs_dummy_init;
        nicfs_shutdown = nicfs_real_shutdown;
        // initialize connection to the niccolum server
        nicfs_server_open_connection();
    }
    pthread_mutex_unlock(&lock);

}

static void nicfs_real_shutdown(void) 
{
    pthread_mutex_lock(&lock);

    while (nicfs_shutdown == nicfs_real_shutdown) {
        (void) nicfs_terminate_file_state_mgr();
        nicfs_shutdown = nicfs_dummy_shutdown;
        nicfs_init = nicfs_real_init;
        // close connection to niccolum server
        nicfs_server_close_connection();
    }
    pthread_mutex_unlock(&lock);
}

static void nicfs_dummy_shutdown(void)
{
    // do nothing
}


int nicfs_check_prefix(const char *name)
{
    const char **prefix;
    int found = 0;
    size_t prefix_length;

    for (prefix = nicfs_prefixes; NULL != *prefix; prefix++) {
        prefix_length = strlen(*prefix);

        if (prefix_length > strlen(name)) {
            // can't possibly match
            continue;
        }

        if (0 == memcmp(*prefix, name, prefix_length)) {
            // match
            found = 1;
            break;
        }
    }

    return found;
}
