/*
 * Copyright (c) 2017, Tony Mason. All rights reserved.
 */

#include "../finesse-internal.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <string.h>

#include <defs.h>
// #include <crt/nstime.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include <uuid/uuid.h>

#include <pthread.h>

#include "munit.h"

extern char *files_in_path[];
extern char *libs_in_path[];

static int finesse_enabled = 0;

static void finesse_init_dummy(void);
static void finesse_init_dummy(void)
{
}
void (*finesse_init)(void) = finesse_init_dummy;

static void finesse_shutdown(void);
static void finesse_shutdown(void)
{
}

static MunitResult
test_null(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    return MUNIT_OK;
}

static MunitResult test_lookup_table_create(const MunitParameter params[], void *arg)
{

    (void) params;
    (void) arg;

    munit_assert(0 == finesse_init_file_state_mgr());

    finesse_terminate_file_state_mgr();

    return MUNIT_OK;
}

typedef struct {
    int min_fd;
    int max_fd;
    unsigned lookup_iterations;
} lt_test_params_t;

static void * ltworker(void *arg)
{
    finesse_file_state_t *fs = NULL;
    lt_test_params_t *params = (lt_test_params_t *)arg;
    char buf[128];
    char buf2[128];

    while (NULL != params) {

        // First - let's create them
        for (int index = params->min_fd; index <= params->max_fd; index++) {
            uuid_t uuid;

            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf), "/test/%016u", index);
            uuid_generate(uuid);
            fs = finesse_create_file_state(index, (void *)&uuid, buf);

            assert (NULL != fs);
        }

        // now let's read them back
        for (unsigned count = 0; count < params->lookup_iterations; count++) {
            for (int index = params->min_fd; index <= params->max_fd; index++) {

                memset(buf2, 0, sizeof(buf2));
                snprintf(buf2, sizeof(buf2), "/test/%016u", index);
                fs = finesse_lookup_file_state(index);
                assert(NULL != fs);
                assert(fs->fd == index);
                assert(0 == strcmp(buf2, fs->pathname));        
            }
        }

        // mow let's delete them
        for (int index = params->min_fd; index <= params->max_fd; index++) {
            if (0 == (index & 0x1)) {
                continue; // skip evens
            }

            fs = finesse_lookup_file_state(index);
            assert(NULL != fs);
            finesse_delete_file_state(fs);

            fs = finesse_lookup_file_state(index);
            assert(NULL == fs);
        }

        // let's make sure we cn still find the even ones
        for (int index = params->min_fd; index <= params->max_fd; index++) {
            if (0 != (index & 0x1)) {
                continue;
            }

            fs = finesse_lookup_file_state(index);
            assert(NULL != fs);
        }

        break;
        
    }

    // pthread_exit(NULL);
    return NULL;
}


static MunitResult test_lookup_table(const MunitParameter params[], void *arg)
{
    static const int fd_max_test = 65536;
    unsigned thread_count = 12;
    pthread_t threads[thread_count];
    pthread_attr_t thread_attr;
    int status;
    lt_test_params_t test_params[thread_count];

    (void) params;
    (void) arg;

    //munit_assert(0);

    finesse_init();

    pthread_attr_init(&thread_attr);

    memset(&threads, 0, sizeof(threads));
    
    munit_assert(0 == finesse_init_file_state_mgr());

    for (unsigned index = 0; index < thread_count; index++) {
        if (index > 0) {
            test_params[index].min_fd = test_params[index-1].max_fd + 1;
        }
        else {
            test_params[index].min_fd = 0;
        }

        if (index < thread_count - 1) {
            test_params[index].max_fd = test_params[index].min_fd + fd_max_test / thread_count;
        }
        else {
            test_params[index].max_fd = fd_max_test;
        }
        test_params[index].lookup_iterations = 100;
        status = pthread_create(&threads[index], &thread_attr, ltworker, &test_params[index]);
        munit_assert(0 == status);
    }

    pthread_attr_destroy(&thread_attr);

    for (unsigned index = 0; index < thread_count; index++) {
        void *result;
        status = pthread_join(threads[index], &result);
        munit_assert(0 == status);
        munit_assert(NULL == result);
    }
    
    finesse_terminate_file_state_mgr();

    finesse_shutdown();

    return MUNIT_OK;
}

#define TEST_MOUNT_PREFIX (char *)(uintptr_t)"mountprefix"
#define TEST_OPEN_FILE_PARAM_FILE (char *)(uintptr_t)"file"
#define TEST_OPEN_FILE_PARAM_DIR (char *)(uintptr_t)"dir"
#define TEST_FILE_COUNT (char *)(uintptr_t)"filecount"
#define TEST_FINESSE_OPTION (char *)(uintptr_t)"niccolum"

#define TEST_FINESSE_OPTION_TRUE (char *)(uintptr_t)"true"
#define TEST_FINESSE_OPTION_FALSE (char *)(uintptr_t)"false"

static int get_finesse_option(const MunitParameter params[]) 
{
    const char *niccolum = munit_parameters_get(params, TEST_FINESSE_OPTION);
    int enabled = 0; // default is false

    if (NULL != niccolum) {
        if (0 == strcmp(TEST_FINESSE_OPTION_TRUE, niccolum)) {
            enabled = 1;
        }
    }    

    return enabled;
}

static char **test_files;

static void generate_files(const char *test_dir, const char *base_file_name, unsigned short filecount)
{
    size_t fn_length, table_length, td_length, bf_length;
    char **file_list = NULL;
    unsigned index;

    // 37 = size of a UUID - 32 digts + 4 dashes + 1 for the '\0' at the end
    td_length = strlen(test_dir);
    bf_length = strlen(base_file_name);
    fn_length = td_length + bf_length + 34 + 1;

    table_length = sizeof(char *) * (filecount + 1);
    file_list = (char **)malloc(table_length);
    munit_assert_not_null(file_list);

    for (index = 0; index < filecount; index++) {
        uuid_t test_uuid;
        char uuid_string[40];

        uuid_generate_time_safe(test_uuid);
        uuid_unparse_lower(test_uuid, uuid_string);
        munit_assert(strlen(uuid_string) == 36);
        file_list[index] = (char *)malloc(fn_length);
        munit_assert_not_null(file_list[index]);
        snprintf(file_list[index], fn_length, "%s/%s%s", test_dir, base_file_name, uuid_string);
    }
    file_list[filecount] = NULL;

    test_files = file_list;
}

#if 0
static void cleanup_files(void)
{
    unsigned index;

    while (NULL != test_files) {    

        index = 0;
        while (NULL != test_files[index]) {
            free(test_files[index]);
            index++;
        }

        free(test_files);
        test_files = NULL;
    }
}
#endif // 0

static void setup_test(const MunitParameter params[])
{
    const char *dir;
    const char *file;
    const char *filecount;
    unsigned file_count; 
    int dfd;

    dir = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(dir);

    file = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_FILE);
    munit_assert_not_null(file);

    filecount = munit_parameters_get(params, TEST_FILE_COUNT);
    munit_assert_not_null(filecount);
    file_count = strtoul(filecount, NULL, 0);
    munit_assert(file_count > 0);
    munit_assert(file_count < 65536); // arbitrary

    finesse_enabled = get_finesse_option(params);

    dfd = open(dir, 0);
    if (dfd < 0) {
        dfd = mkdir(dir, 0775);
    }
    munit_assert_int(dfd, >=, 0);
    munit_assert_int(close(dfd), >=, 0);

    generate_files(dir, file, file_count);

}

static void cleanup_test(const MunitParameter params[])
{
    (void) params;
#if 0
    const char *dir;
    int enabled = get_finesse_option(params);

    dir = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(dir);

    cleanup_files();
    if (enabled) {
        finesse_unlink(dir);
    }
    else {
        unlink(dir);
    }
    finesse_enabled = 0;
#endif // 0
}

static MunitResult
test_open_existing_files(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int fd;
    const char *prefix;
    int index;
    char scratch[512];


    finesse_enabled = get_finesse_option(params);

    finesse_init();
    setup_test(params);

    prefix = munit_parameters_get(params, TEST_MOUNT_PREFIX);
    munit_assert_not_null(prefix);

    munit_assert_not_null(test_files);
    if (finesse_enabled) {
    	munit_assert(0 == finesse_init_file_state_mgr());
    }
 
    // create files
    index = 0;
    while (NULL != test_files[index]) {
        if (finesse_enabled) {
            fd = finesse_open(test_files[index], O_CREAT, 0664); // existing
        }
        else {
            fd = open(test_files[index], O_CREAT, 0664); // existing           
        }
        munit_assert_int(fd, >=, 0);
        if (finesse_enabled) {
            munit_assert_int(finesse_close(fd), >=, 0);
        }
        else {
            munit_assert_int(close(fd), >=, 0);
        }
        index++;
    }

    // now open the files
    index = 0;
    while (NULL != test_files[index]) {
        strncpy(scratch, prefix, sizeof(scratch));
        strncat(scratch, test_files[index], sizeof(scratch) - strlen(scratch));

        if (finesse_enabled) {
          fd = finesse_open(scratch, 0); // existing
        }
        else {
          fd = open(scratch, 0); // existing            
        }
        if (0 > fd) {
            perror("finesse_open");
        }
        munit_assert_int(fd, >=, 0);
        if (finesse_enabled) {
            munit_assert_int(finesse_close(fd), >=, 0);
        }
        else {
            munit_assert_int(close(fd), >=, 0);
        }
        index++;
    }

    cleanup_test(params);

    if (finesse_enabled)
        finesse_terminate_file_state_mgr();

    finesse_shutdown();

    return MUNIT_OK;
}

static MunitResult
test_open_nonexistant_files(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int fd;
    const char *prefix;
    const char *dir;
    const char *file;
    const char *filecount;
    char scratch[512];
    unsigned index;
    unsigned long file_count;

    finesse_init();

    prefix = munit_parameters_get(params, TEST_MOUNT_PREFIX);
    munit_assert_not_null(prefix);

    dir = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(dir);

    file = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_FILE);
    munit_assert_not_null(file);

    filecount = munit_parameters_get(params, TEST_FILE_COUNT);
    munit_assert_not_null(filecount);
    file_count = strtoul(filecount, NULL, 0);
    munit_assert(file_count > 0);
    munit_assert(file_count < 65536); // arbitrary

    generate_files(dir, file, file_count);
    munit_assert(strlen(prefix) < 256);
    if (finesse_enabled)
        munit_assert(0 == finesse_init_file_state_mgr());

    index = 0;
    while (NULL != test_files[index]) {
        strncpy(scratch, prefix, sizeof(scratch));
        strncat(scratch, test_files[index], sizeof(scratch) - strlen(scratch));

        if (finesse_enabled) {
            fd = finesse_open(scratch, 0);
        }
        else {
            fd = open(scratch, 0);
        }
        munit_assert_int(fd, <, 0);
        munit_assert_int(errno, ==, ENOENT);
        index++;
    }

    if (finesse_enabled)
        finesse_terminate_file_state_mgr();


    finesse_shutdown();

    return MUNIT_OK;
}


static MunitResult
test_open_dir(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int fd;
    const char *prefix;
    const char *pathname;
    char scratch[512];

    finesse_init();
    

    setup_test(params);

    prefix = munit_parameters_get(params, TEST_MOUNT_PREFIX);
    munit_assert_not_null(prefix);

    pathname = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(pathname);
    
    if (finesse_enabled)
        munit_assert(0 == finesse_init_file_state_mgr());

    strncpy(scratch, prefix, sizeof(scratch));
    strncat(scratch, pathname, sizeof(scratch) - strlen(scratch));

    if (finesse_enabled) {
        fd = finesse_open(scratch, 0);
    }
    else {
        fd = open(scratch, 0);
    }
    munit_assert_int(fd, >=, 0);

    if (finesse_enabled) {
        munit_assert_int(finesse_close(fd), >=, 0);
        finesse_terminate_file_state_mgr();

    }
    else {
        munit_assert_int(close(fd), >=, 0);
    }
    
    
    finesse_shutdown();

    return MUNIT_OK;
}

static MunitResult
test_fstatfs(
    const MunitParameter params[] __notused,
    void *prv __notused) 
{
#if 0
    int fd;
    int status;
    const char *prefix;
    const char *test_file;
    struct statvfs *statfsstruc;

    finesse_enabled = get_finesse_option(params);
   
    finesse_init();
    setup_test(params);
    
    prefix =  munit_parameters_get(params, TEST_MOUNT_PREFIX);
    munit_assert_not_null(prefix);

    munit_assert_not_null(test_files);
    test_file = test_files[0];
    munit_assert_not_null(test_file);
    
    if(finesse_enabled)
        munit_assert(0 == finesse_init_file_state_mgr());

    if (finesse_enabled) {
        fd = finesse_open(test_file, O_CREAT, 0664);
        munit_assert_int(fd, >=, 0);
        statfsstruc = malloc(sizeof(struct statvfs));
        status = finesse_fstatfs(fd, statfsstruc);
        free(statfsstruc);
        munit_assert_int(status, ==, 0);
    } 
    else {
        fd = open(test_file, O_CREAT, 0664);
        munit_assert_int(fd, >=, 0);
    }

    if (finesse_enabled) {
        munit_assert_int(finesse_close(fd), >=, 0);
    }
    else {
       munit_assert_int(close(fd), >=, 0); 
    }
    
    cleanup_test(params);
    
    if (finesse_enabled)
        finesse_terminate_file_state_mgr();
    
    finesse_shutdown();
#endif // 0
    return MUNIT_OK;
}

static const char *ld_library_path[] = {
"/usr/lib/x86_64-linux-gnu/libfakeroot",
"/usr/lib/i686-linux-gnu",
"/lib/i386-linux-gnu",
"/usr/lib/i686-linux-gnu",
"/usr/lib/i386-linux-gnu/mesa",
"/usr/local/lib",
"/lib/x86_64-linux-gnu",
"/usr/lib/x86_64-linux-gnu",
"/usr/lib/x86_64-linux-gnu/mesa-egl",
"/usr/lib/x86_64-linux-gnu/mesa",
"/lib32",
"/usr/lib32",
"/libx32",
"/usr/libx32",
};


//
// given a list of files and paths, find the first occurrence of the file in the path
//
static int find_file_in_path(const char **files, const char **paths, const char *prefix, char **file_found, char **path_found) 
{
    unsigned file_index, path_index;
    struct stat stat_buf;
    char scratch[4096]; // TODO: make this handle arbitrary long paths

    for (file_index = 0; NULL != files[file_index]; file_index++) {
        for (path_index = 0; NULL != paths[path_index]; path_index++) {
            /* this is done using the stat call */
            if (NULL != prefix) {
                sprintf(scratch, "%s/%s/%s", prefix, paths[path_index], files[file_index]);
            }
            else {
                sprintf(scratch, "%s/%s", paths[path_index], files[file_index]);
            }

            if (0 == stat(scratch, &stat_buf)) {
                // found it
                // TODO: may want to make this more resilient against "wrong kind of content" errors
                *file_found = (char *)(uintptr_t)files[file_index];
                *path_found = (char *)(uintptr_t)paths[path_index];
                return 0;
            }
        }
    }

    return -1;
}

static char **get_env_path(void)
{
    const char *path = getenv("PATH");
    size_t path_len;
    char *path_copy = NULL;
    unsigned index;
    unsigned index2;
    unsigned count;
    char **paths = NULL;
    char *cwd = NULL;

    munit_assert_not_null(path);

    path_len = strlen(path);
    path_copy = malloc(path_len + pathconf(".", _PC_PATH_MAX) + 2);
    munit_assert_not_null(path_copy);
    strcpy(path_copy, path);
    munit_assert(':' != path_copy[0]); // don't handle this case now
    cwd = getcwd(&path_copy[path_len+2], pathconf(".", _PC_PATH_MAX + 1));
    munit_assert_not_null(cwd);

    count = 0;
    // skip char 0 because if it is a colon then there's nothing in front of it anyway
    for (index = 0; index < path_len; index++) {
        if (':' == path_copy[index]) {
            count++;
        }
    }

    index2 = 0;
    paths = (char **)malloc(sizeof(char *) * (count + 1));
    paths[0] = path_copy;
    for (index = 0; index < path_len; index++) {
        if (':' == path_copy[index]) {
            path_copy[index] = '\0';
            munit_assert(':' != path_copy[index+1]); // don't handle this case
            if (0 == strcmp(paths[index2], ".")) {
                paths[++index2] = cwd;
            }
            else {
                paths[++index2] = &path_copy[index+1];
            }
        }
    }
    munit_assert(index2 == count);
    paths[index2] = NULL;

    return paths;    
}

static MunitResult
lib_search_internal(char *prefix)
{
    // char **paths = get_env_path();
    char *file_found, *path_found;
    char *files_to_find[2];
    unsigned found, not_found;
    unsigned index;

    munit_assert_not_null(ld_library_path);

    /* now let's search it for a list of things */

    found = 0;
    not_found = 0;
    for (index = 0; NULL != libs_in_path[index]; index++) {
        files_to_find[0] = libs_in_path[index];
        files_to_find[1] = NULL;

        if (0 == find_file_in_path((const char **)files_to_find, (const char **)ld_library_path, prefix, &file_found, &path_found)) {
            found++;
        }
        else {
            not_found++;
        }
    }

    munit_assert(found + not_found == index);

    return MUNIT_OK;
}

static MunitResult
path_search_internal(char *prefix)
{
    char **paths = get_env_path();
    char *file_found, *path_found;
    char *files_to_find[2];
    unsigned found, not_found;
    unsigned index;

    munit_assert_not_null(paths);

    /* now let's search it for a list of things */

    found = 0;
    not_found = 0;
    for (index = 0; NULL != files_in_path[index]; index++) {
        files_to_find[0] = files_in_path[index];
        files_to_find[1] = NULL;

        if (0 == find_file_in_path((const char **)files_to_find, (const char **)paths, prefix, &file_found, &path_found)) {
            found++;
        }
        else {
            not_found++;
        }
    }

    munit_assert(found + not_found == index);

    return MUNIT_OK;
}

static MunitResult
test_path_search_native(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    unsigned index; 

    //munit_assert(0);

    for (index = 0; index < 100; index++) {
        munit_assert(MUNIT_OK == path_search_internal(NULL));
    }
    return MUNIT_OK;
}

static MunitResult
test_path_search_pt(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    unsigned index; 

    //munit_assert(0);

    for (index = 0; index < 100; index++) {
        munit_assert(MUNIT_OK == path_search_internal((char *)(uintptr_t)"/mnt/pt"));
    }
    return MUNIT_OK;
}

static MunitResult
test_library_search_native(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    unsigned index;

    //munit_assert(0);

    for (index = 0; index < 100; index++) {
        munit_assert(MUNIT_OK == lib_search_internal(NULL));
    }
    return MUNIT_OK;
}

static MunitResult
test_library_search_pt(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    unsigned index;


    //munit_assert(0);

    for (index = 0; index < 100; index++) {
        munit_assert(MUNIT_OK == lib_search_internal((char *)(uintptr_t)"/mnt/pt"));
    }
    return MUNIT_OK;
}

static MunitResult
old_test_finesse_search(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    unsigned index;
    unsigned index2;

    finesse_init();

    for (index = 0; index < 100; index++) {
        for (index2 = 0; NULL != files_in_path[index2]; index2++) {
            //munit_assert(MUNIT_OK == finesse_test_server());
        }
    }

    finesse_shutdown();

    return MUNIT_OK;
}

int finesse_pack_strings(char **strings, void **data, size_t *data_length);
int finesse_unpack_strings(void *data, size_t max_length, char ***strings);

static MunitResult
test_finesse_search(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
#if 0
    // TODO: this needs to be replaced with calls to the library routine
    size_t packed_file_length;
    size_t packed_path_length;
    void *packed_files;
    void *packed_paths;
    char **unpacked_files = NULL;
    char **unpacked_paths = NULL;

    char *files[] = {
        "file1",
        "file2",
        "file3",
        "file4",
        "file5",
        NULL,
    };

    char *paths[] = {
        "/tmp",
        "/bin",
        "/usr/bin",
        "/sbin",
        NULL,
    };

    munit_assert(0 == finesse_pack_strings(files, &packed_files, &packed_file_length));
    munit_assert_not_null(packed_files);
    munit_assert(0 == finesse_pack_strings(paths, &packed_paths, &packed_path_length));
    munit_assert_not_null(packed_paths);

    munit_assert(0 == finesse_unpack_strings(packed_files, packed_file_length, &unpacked_files));
    munit_assert(0 == finesse_unpack_strings(packed_paths, packed_path_length, &unpacked_paths));

    munit_assert_not_null(unpacked_files);
    munit_assert_not_null(unpacked_paths);

    for (unsigned index = 0; NULL != unpacked_files[index]; index++) {
        size_t flen = strlen(files[index]);
        size_t uflen = strlen(unpacked_files[index]);

        munit_assert(flen == uflen);
        munit_assert(0 == memcmp(files[index], unpacked_files[index], strlen(files[index])));
    }

    for (unsigned index = 0; NULL != unpacked_paths[index]; index++) {       
        munit_assert(strlen(paths[index]) == strlen(unpacked_paths[index]));
        munit_assert(0 == memcmp(paths[index], unpacked_paths[index], strlen(paths[index])));
    }


    free(unpacked_files);
    free(unpacked_paths);
    free(packed_files);
    free(packed_paths);
#endif // 0

    return MUNIT_OK;

    old_test_finesse_search(params, prv);
}


static const char *mount_prefix[] = {"", /* "/mnt/pt",*/ NULL};
static const char *files[] = { "testfile1", NULL};
static const char *dirs[] = { "/tmp/nicfs/dir1", NULL};
static const char *file_counts[] = { "1", /* "100", "1000",  "10000",*/ NULL};
static char *TEST_FINESSE_OPTIONS[] = {/*TEST_FINESSE_OPTION_FALSE,*/ TEST_FINESSE_OPTION_TRUE, NULL};

MunitParameterEnum open_params[] = 
{
    {.name = (char *)(uintptr_t)TEST_MOUNT_PREFIX, .values = (char **)(uintptr_t)mount_prefix}, 
    {.name = (char *)(uintptr_t)TEST_OPEN_FILE_PARAM_FILE, .values = (char **)(uintptr_t)files},
    {.name = (char *)(uintptr_t)TEST_OPEN_FILE_PARAM_DIR,  .values = (char **)(uintptr_t)dirs},
    {.name = (char *)(uintptr_t)TEST_FILE_COUNT, .values = (char **)(uintptr_t)file_counts},
    {.name = (char *)(uintptr_t)TEST_FINESSE_OPTION, .values = (char **)(uintptr_t)TEST_FINESSE_OPTIONS},
    {.name = NULL, .values = NULL},
};


#define TEST(_name, _func, _params)             \
    {                                           \
        .name = (char *)(uintptr_t)(_name),                        \
        .test = (_func),                        \
        .setup = NULL,                          \
        .tear_down = NULL,                      \
        .options = MUNIT_TEST_OPTION_NONE,      \
        .parameters = (_params),                     \
    }

int
main(
    int argc,
    char **argv)
{
    static MunitTest tests[] = {
        TEST("/one", test_null, NULL),
        TEST("/open/dir", test_open_dir, open_params),
        TEST("/open/existing-files", test_open_existing_files, open_params),
        TEST("/open/nonexistant-files", test_open_nonexistant_files, open_params),
        TEST("/fstatfs", test_fstatfs, open_params),
	TEST("/lookup/create", test_lookup_table_create, NULL),
        TEST("/lookup/test_table", test_lookup_table, NULL),
        TEST("/search/path/niccolum", test_finesse_search, NULL),
        TEST("/search/path/native", test_path_search_native, NULL),
        TEST("/search/path/passthrough", test_path_search_pt, NULL),
        TEST("/search/library/native", test_library_search_native, NULL),
        TEST("/search/library/passthrough", test_library_search_pt, NULL),
        TEST(NULL, NULL, NULL),
    };
    static const MunitSuite suite = {
        .prefix = (char *)(uintptr_t)"/nicfs",
        .tests = tests,
        .suites = NULL,
        .iterations = 1,
        .options = MUNIT_SUITE_OPTION_NONE,
    };

    return munit_suite_main(&suite, NULL, argc, argv);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
