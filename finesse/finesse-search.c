/*
  FINESSE: Low Energy effcient extension to FUSE
  Copyright (C) 2017  Tony Mason <fsgeek@cs.ubc.ca>

*/

#define _GNU_SOURCE

#include "config.h"
#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_opt.h"
#include "fuse_misc.h"
#include <fuse_lowlevel.h>
#include "finesse_msg.h"
#include "finesse_fuse.h"
#include "finesse-lookup.h"
#include "finesse-list.h"
#include "finesse-search.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <sys/file.h>
#include <time.h>
#include <mqueue.h>
#include <uuid/uuid.h>

//
// given a list of files and paths, find the first occurrence of the file in the path
//
static int finesse_find_file_in_path(char **files, char **paths, char *prefix, char **file_found, char **path_found)
{
    unsigned file_index, path_index;
    struct stat stat_buf;
    char scratch[4096]; // TODO: make this handle arbitrary long paths

    for (file_index = 0; NULL != files[file_index]; file_index++)
    {
        for (path_index = 0; NULL != paths[path_index]; path_index++)
        {
            /* this is done using the stat call */
            if (NULL != prefix)
            {
                sprintf(scratch, "%s/%s/%s", prefix, paths[path_index], files[file_index]);
            }
            else
            {
                sprintf(scratch, "%s/%s", paths[path_index], files[file_index]);
            }

            if (0 == stat(scratch, &stat_buf))
            {
                // found it
                // TODO: may want to make this more resilient against "wrong kind of content" errors
                *file_found = files[file_index];
                *path_found = paths[path_index];
                return 0;
            }
        }
    }

    return -1;
}

#if 0
typedef struct finesse_path_search_request {
	//
	// The search data consists of two lists:
	//
	// <string><null><string><null><string><null><null>
	// <string><null><string><null><null><null>
	//
	// TODO: we may need to have some way of specifying
	//       this is sent in shared memory.
	//
	u_int16_t SearchDataFlags;
	u_int16_t SearchDataLength;
	char SearchData[1];
} finesse_path_search_request_t;

#define FINESSE_SEARCH_DATA_RESIDENT (0x1)
#define FINESSE_SEARCH_DATA_SHARED_MEM (0x2)
#endif // 0

int finesse_process_search_request(finesse_path_search_request_t *search_request, finesse_path_search_response_t **search_response)
{
    (void)search_request;
    (void)search_response;

    return -ENOSYS;
}

#if 0
/////
//
// given a list of files and paths, find the first occurrence of the file in the path
//
static int find_file_in_path(char **files, char **paths, char *prefix, char **file_found, char **path_found)
{
    unsigned file_index, path_index;
    struct stat stat_buf;
    char scratch[4096]; // TODO: make this handle arbitrary long paths

    for (file_index = 0; NULL != files[file_index]; file_index++)
    {
        for (path_index = 0; NULL != paths[path_index]; path_index++)
        {
            /* this is done using the stat call */
            if (NULL != prefix)
            {
                sprintf(scratch, "%s/%s/%s", prefix, paths[path_index], files[file_index]);
            }
            else
            {
                sprintf(scratch, "%s/%s", paths[path_index], files[file_index]);
            }

                // TODO: need to wire this up
#if 0
			{
				// ugly hack - simulate a call to getattr
				struct lo_inode {
					struct lo_inode *next;
					struct lo_inode *prev;
					int fd;
					ino_t ino;
					dev_t dev;
					uint64_t nlookup;
				};

			}
			finesse_original_ops.getattr();
#endif // 0
            if (0 == stat(scratch, &stat_buf))
            {
                // found it
                // TODO: may want to make this more resilient against "wrong kind of content" errors
                *file_found = files[file_index];
                *path_found = paths[path_index];
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

    assert(NULL != path);

    path_len = strlen(path);
    path_copy = malloc(path_len + pathconf(".", _PC_PATH_MAX) + 2);
    assert(NULL != path_copy);
    strcpy(path_copy, path);
    assert(':' != path_copy[0]); // don't handle this case now
    cwd = getcwd(&path_copy[path_len + 2], pathconf(".", _PC_PATH_MAX + 1));
    assert(NULL != cwd);

    count = 0;
    // skip char 0 because if it is a colon then there's nothing in front of it anyway
    for (index = 0; index < path_len; index++)
    {
        if (':' == path_copy[index])
        {
            count++;
        }
    }

    index2 = 0;
    paths = (char **)malloc(sizeof(char *) * (count + 1));
    assert(NULL != paths);
    paths[0] = path_copy;
    for (index = 0; index < path_len; index++)
    {
        if (':' == path_copy[index])
        {
            path_copy[index] = '\0';
            assert(':' != path_copy[index + 1]); // don't handle this case
            if (0 == strcmp(paths[index2], "."))
            {
                paths[++index2] = cwd;
            }
            else
            {
                paths[++index2] = &path_copy[index + 1];
            }
        }
    }
    assert(index2 == count);
    paths[index2] = NULL;

    return paths;
}

static void
lib_search_internal(char *prefix)
{
    char *file_found, *path_found;
    char *files_to_find[2];
    static unsigned file_to_use = 0;
    if (NULL == files_in_path[file_to_use])
    {
        file_to_use = 0;
    }

    assert(NULL != ld_library_path);

    /* now let's search it for a list of things */

    files_to_find[0] = libs_in_path[file_to_use];
    files_to_find[1] = NULL;

    (void)find_file_in_path(files_to_find, ld_library_path, prefix, &file_found, &path_found);

    file_to_use++;
}

static void
path_search_internal(char *prefix)
{
    static char **paths;
    char *file_found, *path_found;
    char *files_to_find[2];
    unsigned found, not_found;
    // unsigned index;
    static unsigned file_to_use = 0;

    if (NULL == paths)
    {
        paths = get_env_path();
    }

    if (NULL == files_in_path[file_to_use])
    {
        file_to_use = 0;
    }

    files_to_find[0] = files_in_path[file_to_use];
    files_to_find[1] = NULL;

    (void)find_file_in_path(files_to_find, paths, prefix, &file_found, &path_found);

    file_to_use++;
}

// this is a search test to simulate until I get the marshalling/unmarshalling code done
void finesse_test_search(void)
{
    lib_search_internal(NULL);
    // path_search_internal(NULL);
}
