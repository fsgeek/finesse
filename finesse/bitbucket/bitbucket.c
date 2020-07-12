//
// FUSE: Bitbucket
//
// (C) Copyright 2020 Tony Mason (fsgeek@cs.ubc.ca)
// All Rights Reserved
//

#include "bitbucket.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse_common.h>
#include <fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bitbucketcalls.h"

#define BITBUCKET_DEFAULT_LOG_LEVEL FUSE_LOG_ERR

static enum fuse_log_level BitbucketLogLevel = BITBUCKET_DEFAULT_LOG_LEVEL;
static FILE *              logFile;

static void bitbucket_log_func(enum fuse_log_level level, const char *fmt, va_list ap)
{
    if (level <= BitbucketLogLevel) {
        vfprintf(logFile, fmt, ap);
    }
}

static const char *LogLevelToString(enum fuse_log_level LogLevel)
{
    const char *str = "Invalid";

    switch (LogLevel) {
        case FUSE_LOG_EMERG:
            str = "FUSE_LOG_EMERG";
            break;
        case FUSE_LOG_ALERT:
            str = "FUSE_LOG_ALERT";
            break;
        case FUSE_LOG_CRIT:
            str = "FUSE_LOG_CRIT";
            break;
        case FUSE_LOG_ERR:
            str = "FUSE_LOG_ERR";
            break;
        case FUSE_LOG_WARNING:
            str = "FUSE_LOG_WARNING";
            break;
        case FUSE_LOG_NOTICE:
            str = "FUSE_LOG_NOTICE";
            break;
        case FUSE_LOG_INFO:
            str = "FUSE_LOG_INFO";
            break;
        case FUSE_LOG_DEBUG:
            str = "FUSE_LOG_DEBUG";
            break;
    }

    return str;
}

static void BitbucketSetupLogging(bitbucket_userdata_t *BBud)
{
    assert(NULL != BBud);
    if (BBud->LogFile) {
        logFile = fopen(BBud->LogFile, "w+");
        if (NULL == logFile) {
            fuse_log(FUSE_LOG_ERR, "Unable to open file %s for writing\n", BBud->LogFile);
        }

        // Disable buffering; this is comparable to stderr
        setbuf(logFile, NULL);
    }

    if (NULL == logFile) {
        logFile = stderr;
    }

    if ((BBud->LogLevel < FUSE_LOG_EMERG) || (BBud->LogLevel > FUSE_LOG_DEBUG)) {
        // outside allowed range
        fuse_log(FUSE_LOG_ERR, "Specified invalid log level %d, defaulting to %d\n", (int)BBud->LogLevel, BitbucketLogLevel);
    }
    else {
        if ((int)BBud->LogLevel != BitbucketLogLevel) {
            BitbucketLogLevel = (enum fuse_log_level)BBud->LogLevel;
            fuse_log(FUSE_LOG_INFO, "Set log level to %s\n", LogLevelToString(BBud->LogLevel));
        }
    }

    fuse_set_log_func(bitbucket_log_func);
}

static const struct fuse_lowlevel_ops bitbucket_ll_oper = {
    .init            = bitbucket_init,
    .destroy         = bitbucket_destroy,
    .lookup          = bitbucket_lookup,
    .forget          = bitbucket_forget,
    .getattr         = bitbucket_getattr,
    .setattr         = bitbucket_setattr,
    .readlink        = bitbucket_readlink,
    .mknod           = bitbucket_mknod,
    .mkdir           = bitbucket_mkdir,
    .unlink          = bitbucket_unlink,
    .rmdir           = bitbucket_rmdir,
    .symlink         = bitbucket_symlink,
    .rename          = bitbucket_rename,
    .link            = bitbucket_link,
    .open            = bitbucket_open,
    .read            = bitbucket_read,
    .write           = bitbucket_write,
    .flush           = bitbucket_flush,
    .release         = bitbucket_release,
    .fsync           = bitbucket_fsync,
    .opendir         = bitbucket_opendir,
    .readdir         = bitbucket_readdir,
    .releasedir      = bitbucket_releasedir,
    .fsyncdir        = bitbucket_fsyncdir,
    .statfs          = bitbucket_statfs,
    .setxattr        = bitbucket_setxattr,
    .getxattr        = bitbucket_getxattr,
    .listxattr       = bitbucket_listxattr,
    .removexattr     = bitbucket_removexattr,
    .access          = bitbucket_access,
    .create          = bitbucket_create,
    .getlk           = bitbucket_getlk,
    .setlk           = bitbucket_setlk,
    .bmap            = bitbucket_bmap,
    .ioctl           = bitbucket_ioctl,
    .poll            = bitbucket_poll,
    .write_buf       = bitbucket_write_buf,
    .retrieve_reply  = bitbucket_retrieve_reply,
    .forget_multi    = bitbucket_forget_multi,
    .flock           = bitbucket_flock,
    .fallocate       = bitbucket_fallocate,
    .readdirplus     = bitbucket_readdirplus,
    .copy_file_range = bitbucket_copy_file_range,
    .lseek           = bitbucket_lseek,
};

#define BITBUCKET_DEFAULT_INODE_TABLE_SIZE (8192)

static bitbucket_userdata_t BBud = {
    .Magic             = BITBUCKET_USER_DATA_MAGIC,
    .Debug             = 0,
    .RootDirectory     = NULL,
    .InodeTable        = NULL,
    .AttrTimeout       = 3600.0,  // pretty arbitrary value
    .StorageDir        = "/tmp/bitbucket",
    .Writeback         = 1,
    .CachePolicy       = 1,
    .FsyncDisable      = 1,
    .NoXattr           = 1,
    .BackgroundForget  = 0,
    .FlushEnable       = 0,
    .VerifyDirectories = 0,
    .InodeTableSize    = BITBUCKET_DEFAULT_INODE_TABLE_SIZE,
};

static void bitbucket_help(void)
{
    printf("    --no_writeback - disable writeback caching\n");
    printf(
        "    --storagedir=<path> - location to use for temporary storage"
        " (default /tmp/bitbucket)\n");
    printf(
        "    --callstat=<path> - location to write call statistics on "
        "dismount\n");
    printf("    --timeout=<seconds> - attribute timeout (default=3600)\n");
    printf("    --disable_cache - disables all caching (default=enabled)\n");
    printf("    --fsync - enables fsync (default=disabled)\n");
    printf("    --enable_xattr - enable xattr support (default=disabled)\n");
    printf(
        "    --bgforget - enable background forget handling "
        "(default=disabled)\n");
    printf("    --flush - enable flush handling (default=disabled)\n");
    printf(
        "    --verifydirectories - enable directory consistency checks "
        "(default=disabled)\n");
    printf("    --logfile=<path> - location to write log output (default stderr)\n");
    printf(
        "    --loglevel=<level> - logging level (least = %d to most = %d, "
        "default = %d)\n",
        (int)FUSE_LOG_EMERG, (int)FUSE_LOG_DEBUG, BitbucketLogLevel);
    printf(
        "    --inodetablesize=<numeric> - adjust the inode table size (default = "
        "%zu)\n",
        (size_t)BITBUCKET_DEFAULT_INODE_TABLE_SIZE);
}

static const struct fuse_opt bitbucket_opts[] = {
    {"--no_writeback", offsetof(bitbucket_userdata_t, Writeback), 0},
    {"--storagedir=%s", offsetof(bitbucket_userdata_t, StorageDir), 0},
    {"--timeout=%lf", offsetof(bitbucket_userdata_t, AttrTimeout), 0},
    {"--callstat=%s", offsetof(bitbucket_userdata_t, CallStatFile), 0},
    {"--disable_cache", offsetof(bitbucket_userdata_t, CachePolicy), 0},
    {"--fsync", offsetof(bitbucket_userdata_t, FsyncDisable), 0},
    {"--enable_xattr", offsetof(bitbucket_userdata_t, NoXattr), 0},
    {"--bgforget", offsetof(bitbucket_userdata_t, BackgroundForget), 1},
    {"--flush", offsetof(bitbucket_userdata_t, FlushEnable), 1},
    {"--verifydirectories", offsetof(bitbucket_userdata_t, VerifyDirectories), 1},
    {"--logfile=%s", offsetof(bitbucket_userdata_t, LogFile), 0},
    {"--loglevel=%d", offsetof(bitbucket_userdata_t, LogLevel), BITBUCKET_DEFAULT_LOG_LEVEL},
    {"--inodetablesize=%d", offsetof(bitbucket_userdata_t, InodeTableSize), BITBUCKET_DEFAULT_INODE_TABLE_SIZE},
    FUSE_OPT_END};

int main(int argc, char *argv[])
{
    struct fuse_args         args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_session *    se;
    struct fuse_cmdline_opts opts;
    struct fuse_loop_config  config;
    int                      ret = -1;

    if (fuse_parse_cmdline(&args, &opts) != 0)
        return 1;
    if (opts.show_help) {
        printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
        fuse_cmdline_help();
        fuse_lowlevel_help();
        bitbucket_help();
        ret = 0;
        goto err_out1;
    }
    else if (opts.show_version) {
        printf("FUSE library version %s\n", fuse_pkgversion());
        fuse_lowlevel_version();
        ret = 0;
        goto err_out1;
    }

    if (opts.mountpoint == NULL) {
        printf("usage: %s [options] <mountpoint>\n", argv[0]);
        printf("       %s --help\n", argv[0]);
        ret = 1;
        goto err_out1;
    }

    if (opts.debug) {
        BBud.Debug = 1;
    }

    if (fuse_opt_parse(&args, &BBud, bitbucket_opts, NULL)) {
        ret = 1;
        goto err_out1;
    }

    if (NULL != BBud.LogFile) {
        BitbucketSetupLogging(&BBud);
    }

    if (NULL != BBud.StorageDir) {
        struct stat stbuf;
        int         status   = 0;
        pid_t       childpid = 0;
        unsigned    retries  = 0;

        if (0 == stat(BBud.StorageDir, &stbuf)) {
            // It exists, let's clean it up first
            childpid = vfork();
            ret      = -1;
            if (childpid < 0) {
                fuse_log(FUSE_LOG_ERR, "fork failed, errno %d (%s)\n", errno, strerror(errno));
                ret = 1;
                goto err_out1;
            }

            if (0 == childpid) {
                fuse_log(FUSE_LOG_DEBUG, "child invoking rm -rf %s\n", BBud.StorageDir);
                char *const rmargs[] = {(char *)(uintptr_t) "rm", (char *)(uintptr_t) "-rf", (char *)(uintptr_t)BBud.StorageDir,
                                        NULL};
                status               = execv("/bin/rm", rmargs);
                if (0 != status) {
                    ret = 1;
                    goto err_out1;
                }
            }
        }

        status  = 0;
        retries = 0;
        do {
            if (0 != status) {
                sleep(1);
            }
            status = mkdir(BBud.StorageDir, 0700);
            retries++;
        } while ((status != 0) && (retries < 10));

        if (0 != status) {
            fuse_log(FUSE_LOG_ERR, "Unable to create %s (errno  = %d - %s)\n", BBud.StorageDir, errno, strerror(errno));
            ret = 1;
            goto err_out1;
        }
        else {
            fuse_log(FUSE_LOG_DEBUG, "Created %s successfully\n", BBud.StorageDir);
        }
    }

    se = fuse_session_new(&args, &bitbucket_ll_oper, sizeof(bitbucket_ll_oper), &BBud);
    if (se == NULL)
        goto err_out1;

    if (fuse_set_signal_handlers(se) != 0)
        goto err_out2;

    if (fuse_session_mount(se, opts.mountpoint) != 0)
        goto err_out3;

    fuse_daemonize(opts.foreground);

    /* Block until ctrl+c or fusermount -u */
    if (opts.singlethread) {
        ret = fuse_session_loop(se);
    }
    else {
        config.clone_fd         = opts.clone_fd;
        config.max_idle_threads = opts.max_idle_threads;

        ret = fuse_session_loop_mt(se, &config);
    }

    fuse_session_unmount(se);
err_out3:
    fuse_remove_signal_handlers(se);
err_out2:
    fuse_session_destroy(se);
err_out1:
    free(opts.mountpoint);
    fuse_opt_free_args(&args);

    return ret ? 1 : 0;
}
