/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#include <mntent.h>
#include "api-internal.h"
#include "callstats.h"

// this is the list of prefixes that we care about
// TODO: make this configuration or parameterizable
struct _finesse_prefix_table {
    char                    prefix[256];  // this is the mount prefix.  TODO: make this dynamic
    size_t                  prefix_length;
    char                    service_name[128];  // this is the name used to contact the Finesse+Fuse file server
    finesse_client_handle_t client_handle;      // this is the client handle (state)
} finesse_prefix_table[64];

static void finesse_dummy_init(void);
static void finesse_real_init(void);
static void finesse_dummy_shutdown(void);
static void finesse_real_shutdown(void);

void (*finesse_init)(void)     = finesse_real_init;
void (*finesse_shutdown)(void) = finesse_dummy_shutdown;

// TODO: this should be more flexible to supporting multiple mountpoints
// static finesse_client_handle_t finesse_client_handle;

static void finesse_dummy_init(void)
{
    return;
}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void finesse_setup_server_connections(void);

static void finesse_real_init(void)
{
    pthread_mutex_lock(&lock);

    while (finesse_init == finesse_real_init) {
        // Initialization logic goes here
        (void)finesse_init_file_state_mgr();

        // Find all the finesse connections
        finesse_setup_server_connections();

        // now overwrite the init function
        finesse_init     = finesse_dummy_init;
        finesse_shutdown = finesse_real_shutdown;
        // initialize connection to the finesse server
        // FinesseStartClientConnection(&finesse_client_handle);
        FinesseApiInitializeCallStatistics();
    }
    pthread_mutex_unlock(&lock);
}

static void finesse_real_shutdown(void)
{
    int status = 0;

    pthread_mutex_lock(&lock);

    while (finesse_shutdown == finesse_real_shutdown) {
        finesse_shutdown = finesse_dummy_shutdown;
        finesse_init     = finesse_real_init;
        // close connection to finesse servers

        for (unsigned index = 0; index < sizeof(finesse_prefix_table) / sizeof(struct _finesse_prefix_table); index++) {
            if (NULL == finesse_prefix_table[index].client_handle) {
                // done looking
                break;
            }

            status = FinesseStopClientConnection(finesse_prefix_table[index].client_handle);
            assert(0 == status);
            finesse_prefix_table[index].client_handle = NULL;
        }
        (void)finesse_terminate_file_state_mgr();
    }
    pthread_mutex_unlock(&lock);

    // Dump the call statistics

    FILE *                         log       = fopen("/home/tony/finesse-callstats.log", "wt");
    finesse_api_call_statistics_t *callstats = FinesseApiGetCallStatistics();
    const char *                   calldata  = FinesseApiFormatCallData(callstats, 0);
    assert(NULL != log);
    fprintf(log, "Finesse API Call Statistics:\n%s\n", calldata);
    fclose(log);
    FinesseApiFreeFormattedCallData(calldata);
    FinesseApiReleaseCallStatistics(callstats);
}

static void finesse_dummy_shutdown(void)
{
    // do nothing
}

finesse_client_handle_t *finesse_check_prefix(const char *name)
{
    finesse_client_handle_t *client_handle = NULL;
    size_t                   name_length   = 0;

    name_length = strlen(name);
    for (unsigned index = 0; index < sizeof(finesse_prefix_table) / sizeof(struct _finesse_prefix_table); index++) {
        if (NULL == finesse_prefix_table[index].client_handle) {
            // done looking
            break;
        }

        if (name_length < finesse_prefix_table[index].prefix_length) {
            // can't be this one
            continue;
        }

        if (0 == memcmp(name, finesse_prefix_table[index].prefix, finesse_prefix_table[index].prefix_length)) {
            // Same entry
            client_handle = finesse_prefix_table[index].client_handle;
            break;
        }
    }

    // Return what we found
    return client_handle;
}

static void finesse_setup_server_connections()
{
    struct mntent *         mount_entry = NULL;
    FILE *                  mtab        = NULL;
    char                    scratch[128];
    int                     status;
    finesse_client_handle_t client_handle = NULL;
    unsigned                index;

    mtab = setmntent("/etc/mtab", "r");
    if (NULL == mtab) {
        // Try to use fstab
        mtab = setmntent("/etc/fstab", "r");

        if (NULL == mtab) {
            // No file systems to use
            return;
        }
    }

    for (index = 0, mount_entry = getmntent(mtab); NULL != mount_entry; mount_entry = getmntent(mtab)) {
        // Check to see if there is a Finesse server for this mount
        status = GenerateServerName(mount_entry->mnt_dir, scratch, sizeof(scratch));
        if (0 != status) {
            assert(EOVERFLOW != status);
            continue;  // skip this entry
        }

        // Try to connect to Finesse server
        status = FinesseStartClientConnection(&client_handle, mount_entry->mnt_dir);
        if (0 != status) {
            continue;  // skip this entry
        }

        // At this point we are connected.  Let's store this information
        assert(index < sizeof(finesse_prefix_table) / sizeof(struct _finesse_prefix_table));  // don't overflow
        finesse_prefix_table[index].client_handle = client_handle;
        strncpy(finesse_prefix_table[index].service_name, scratch, sizeof(finesse_prefix_table[index].service_name));
        assert(strlen(finesse_prefix_table[index].service_name) == strlen(scratch));  // sanity check
        strncpy(finesse_prefix_table[index].prefix, mount_entry->mnt_dir, sizeof(finesse_prefix_table[index].prefix));
        assert(strlen(finesse_prefix_table[index].prefix) == strlen(mount_entry->mnt_dir));  // sanity check
        finesse_prefix_table[index].prefix_length = strlen(finesse_prefix_table[index].prefix);
        index++;
    }
}
