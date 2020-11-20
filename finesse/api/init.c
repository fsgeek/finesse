/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#include <mntent.h>
#include "api-internal.h"
#include "callstats.h"
#include "timestamp.h"

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
int finesse_api_init_in_progress;

// TODO: this should be more flexible to supporting multiple mountpoints
// static finesse_client_handle_t finesse_client_handle;

static void finesse_dummy_init(void)
{
    return;
}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void finesse_setup_server_connections(void);

static const char *call_stat_log_default       = "default";
static const char *call_stat_log_dir_default   = "tmp";
static const char *call_stat_log_env           = "FINESSE_CALL_STAT_LOG";
static const char *call_stat_log_dir_env       = "FINESSE_CALL_STAT_LOG_DIR";
static const char *call_stat_log_file_template = "/%s/finesse-callstats-%s-%s-%d.log";  // dir, name, timestamp, pid

static void finesse_real_init(void)
{
    static int count = 0;

    pthread_mutex_lock(&lock);
    while (finesse_init == finesse_real_init) {
        count++;

        finesse_api_init_in_progress = 1;

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

#if 0
        // This is some debug logic to see if I'm getting triggered.
        char debug_log[256];
        int  fd = -1;
        int  result;
        char timestamp[64];

        result = FinesseGenerateTimestamp(timestamp, sizeof(timestamp));
        assert(result < sizeof(timestamp));

        result = snprintf(debug_log, sizeof(debug_log), "/tmp/finesse-%s-%s-%d.log", __func__, timestamp, getpid());

        fd = open(debug_log, O_CREAT | O_EXCL | O_RDWR, 0600);

        if (fd > 0) {
            close(fd);
        }
        else {
            assert(EEXIST == errno);  // no idea why, but it seems to end up being there sometimes.
        }
#endif  // 0

        finesse_api_init_in_progress = 0;
    }
    pthread_mutex_unlock(&lock);
}

static void finesse_real_shutdown(void)
{
    int status          = 0;
    int save_call_stats = 0;

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

        save_call_stats = 1;

#if 0
        // This is some debug logic to see if I'm getting triggered.
        char debug_log[256];
        int  fd = -1;
        int  result;
        char timestamp[64];

        result = FinesseGenerateTimestamp(timestamp, sizeof(timestamp));
        assert(result < sizeof(timestamp));

        result = snprintf(debug_log, sizeof(debug_log), "/tmp/finesse-%s-%s-%d.log", __func__, timestamp, getpid());

        (void)unlink(debug_log);

        fprintf(stderr, "create file %s\n", debug_log);
        fd = open(debug_log, O_CREAT | O_EXCL | O_RDWR, 0600);
        assert(fd >= 0);
        close(fd);
#endif  // 0
    }
    pthread_mutex_unlock(&lock);

    if (save_call_stats) {
        // Dump the call statistics

        FILE *                         log       = NULL;
        finesse_api_call_statistics_t *callstats = FinesseApiGetCallStatistics();
        const char *                   calldata  = FinesseApiFormatCallData(callstats, 0);
        const char *                   log_name  = getenv(call_stat_log_env);
        const char *                   log_dir   = getenv(call_stat_log_dir_env);
        int                            retval;
        char                           call_stat_log[256];
        char                           timestamp[64];

        // If the environment variable isn't set OR the name specified won't fit
        // we just use the default name
        if ((NULL == log_name) || strlen(log_name) >= sizeof(call_stat_log)) {
            log_name = call_stat_log_default;
        }

        // Same thing for the directory
        if ((NULL == log_dir) || strlen(log_dir) >= sizeof(call_stat_log)) {
            log_dir = call_stat_log_dir_default;
        }

        retval = FinesseGenerateTimestamp(timestamp, sizeof(timestamp));
        assert(0 == retval);  // success

        retval =
            snprintf(call_stat_log, sizeof(call_stat_log), call_stat_log_file_template, log_dir, log_name, timestamp, getpid());

        assert(retval < sizeof(call_stat_log));

        log = fopen(call_stat_log, "wt+");

        assert(strlen(log_name) < sizeof(call_stat_log));
        strcpy(call_stat_log, log_name);

        assert(NULL != log);
        fprintf(log, "%s\n", calldata);
        fclose(log);
        FinesseApiFreeFormattedCallData(calldata);
        FinesseApiReleaseCallStatistics(callstats);
    }
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
