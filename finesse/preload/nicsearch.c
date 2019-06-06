/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#include "nicfsinternal.h"
#include <niccolum_msg.h>
#include <mqueue.h>
#include <uuid/uuid.h>

/*
 * Given a null terminated list of pointers, marshal them into a single contiguous block
 */
int niccolum_pack_strings(char **strings, void **data, size_t *data_length);
int niccolum_pack_strings(char **strings, void **data, size_t *data_length)
{
    unsigned index;
    size_t buf_size = 0;
    size_t offset;
    char *buf = NULL;

    assert(NULL != strings);

    for (index = 0; NULL != strings[index]; index++) {
        buf_size += strlen(strings[index]);
    }

    // index = # of pointers, including the trailing NULL pointer
    buf_size += sizeof(char) * index;

    buf = (char *)malloc(buf_size);
    if (NULL == buf) {
        return -ENOMEM;
    }

    for (offset = 0, index = 0; NULL != strings[index]; index++) {
        size_t len = strlen(strings[index]);
        memcpy(&buf[offset], strings[index], len);
        offset += len;
        buf[offset++] = '\0';
    }

    buf[offset] = '\0';
    assert(offset == buf_size);

    *data = (void *)buf;
    *data_length = buf_size;

    return 0;
}

/*
 * given a flat buffer of <string><null><string><null>...<string><null><null>
 * convert it to an array of string pointers.
 * 
 * max_length is just here to ensure we don't run off the end of an externally provided
 * buffer.
 */
int niccolum_unpack_strings(void *data, size_t max_length, char ***strings);
int niccolum_unpack_strings(void *data, size_t max_length, char ***strings)
{
    unsigned index;
    unsigned count;
    char *buf = (char *)data;

    *strings = NULL;
    for (count = 0, index = 0; index < max_length;) {
        if ('\0' == buf[index++]) {
            count++;
            if (index > max_length) {
                return -EINVAL;
            }
            if ('\0' == buf[index]) {
                index++;
                break;
            }
        }
    }

    *strings = malloc(sizeof(char *) * (count + 1));

    if (NULL == *strings) {
        return -ENOMEM;
    }

    (*strings)[0] = buf;
    for (count = 1, index = 0; index < max_length;) {
        if ('\0' == buf[index++]) {
            if ('\0' == buf[index]) {
                // done parsing buffer
                (*strings)[count] = NULL;
                break;
            }
            else {
                (*strings)[count++] = &buf[index];
            }
            assert(index <= max_length);
        }
    }

    return 0;
}

int nicfs_search_path(char **files, char **paths, char **found);

int nicfs_search_path(char **files, char **paths, char **found) 
{
    niccolum_message_t *request = NULL, *response = NULL;
    size_t message_length;
    size_t response_length;
    int status = ENOMEM;
    void *packed_file_data = NULL;
    void *packed_path_data = NULL;
    size_t packed_file_data_len;
    size_t packed_path_data_len;
    niccolum_path_search_request_t *npsreq = NULL;
    niccolum_path_search_response_t *npsrsp = NULL;
    uuid_t clientUuid;

    while (NULL != files) {
        status = niccolum_pack_strings(files, &packed_file_data, &packed_file_data_len);
        if (0 != status) {
            break;
        }

        status = niccolum_pack_strings(paths, &packed_path_data, &packed_path_data_len);
        if (0 != status) {
            break;
        }

        message_length = offsetof(niccolum_message_t, Message);
        message_length += offsetof(niccolum_path_search_request_t, SearchData);
        message_length += packed_file_data_len + packed_path_data_len;
        request = (niccolum_message_t *)malloc(message_length);

        if (NULL == request) {
            status = ENOMEM;
            break;
        }

        //
        // TODO: we do the same basic operation repeatedly here, why not generalize this?
        //
        memcpy(request->MagicNumber, NICCOLUM_MESSAGE_MAGIC, NICCOLUM_MESSAGE_MAGIC_SIZE);
        assert(sizeof(request->SenderUuid) == sizeof(uuid_t));
        (void) nicfs_set_client_uuid(&request->SenderUuid);
        request->MessageType = NICCOLUM_PATH_SEARCH_REQUEST;
        request->MessageId = nicfs_generate_messageid();
        request->MessageLength = message_length - offsetof(niccolum_message_t, Message);
        npsreq = (niccolum_path_search_request_t *)request->Message;
        npsreq->SearchDataFlags = NICCOLUM_SEARCH_DATA_RESIDENT;
        npsreq->SearchDataLength = packed_file_data_len + packed_path_data_len;
        memcpy(npsreq->SearchData, packed_file_data, packed_file_data_len);
        memcpy(&npsreq->SearchData[packed_file_data_len], packed_path_data, packed_path_data_len);

        /* send the message */
        status = nicfs_call_server(request, message_length, (void **)&response, &response_length);
        if (0 > status) {
            break;
        }

        /* validate message */
        assert(0 == memcmp(NICCOLUM_MESSAGE_MAGIC, response->MagicNumber, NICCOLUM_MESSAGE_MAGIC_SIZE));
        assert(0 != memcmp(&response->SenderUuid, &clientUuid, sizeof(uuid_t)));
        assert(NICCOLUM_NAME_MAP_RESPONSE == response->MessageType);
        assert(response->MessageId == request->MessageId);
        assert(response_length >= offsetof(niccolum_path_search_response_t, Path));

        npsrsp = (niccolum_path_search_response_t *)response->Message;
        assert(response_length >= offsetof(niccolum_path_search_response_t, Path) + npsrsp->PathLength);

        *found = (char *)malloc(npsrsp->PathLength + sizeof(char));

        assert(NULL != *found);
        if (NULL == found) {
            status = ENOMEM;
            break;
        }

        memcpy(*found, npsrsp->Path, npsrsp->PathLength);
        (*found)[npsrsp->PathLength] = '\0';

        status = 0;

        // done
        break;
    }

    // cleanup
    if (NULL != request) {
        free(request);
    }

    if (NULL != response) {
        nicfs_free_response(response);
    }

    return status;

}