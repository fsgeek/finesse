/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#include "finesse-internal.h"
#include "finesse.h"
#include <pthread.h>

// this is the list of prefixes that we care about
// TODO: make this configuration or parameterizable
const char *finesse_prefixes[] = { "/mnt/pt", NULL};

static void finesse_dummy_init(void);
static void finesse_real_init(void);
static void finesse_dummy_shutdown(void);
static void finesse_real_shutdown(void);

void (*finesse_init)(void) = finesse_real_init;
void (*finesse_shutdown)(void) = finesse_dummy_shutdown;

finesse_client_handle_t finesse_client_handle;

static void finesse_dummy_init(void) 
{
    return;
}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void finesse_real_init(void) 
{
    pthread_mutex_lock(&lock);

    while (finesse_init == finesse_real_init) {
        // Initialization logic goes here
        (void) finesse_init_file_state_mgr();

        // now overwrite the init function
        finesse_init = finesse_dummy_init;
        finesse_shutdown = finesse_real_shutdown;
        // initialize connection to the finesse server
        FinesseStartClientConnection(&finesse_client_handle);
    }
    pthread_mutex_unlock(&lock);

}

static void finesse_real_shutdown(void) 
{
    pthread_mutex_lock(&lock);

    while (finesse_shutdown == finesse_real_shutdown) {
        (void) finesse_terminate_file_state_mgr();
        finesse_shutdown = finesse_dummy_shutdown;
        finesse_init = finesse_real_init;
        // close connection to finesse server
        FinesseStopClientConnection(finesse_client_handle);
    }
    pthread_mutex_unlock(&lock);
}

static void finesse_dummy_shutdown(void)
{
    // do nothing
}


int finesse_check_prefix(const char *name)
{
    const char **prefix;
    int found = 0;
    size_t prefix_length;

    for (prefix = finesse_prefixes; NULL != *prefix; prefix++) {
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
