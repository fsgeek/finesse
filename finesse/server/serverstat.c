/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"
#include "fincomm.h"

static FinesseServerStat stats = {
    .Version = FINESSE_SERVER_STAT_VERSION,
    .Length = FINESSE_SERVER_STAT_LENGTH,
};
FinesseServerStat *FinesseServerStats = &stats;

int FinesseServerNativeServerStatRequest(finesse_server_handle_t Fsh, void *Client, fincomm_message Message)
{
    int status;

    stats.ClientConnectionCount = FinesseGetActiveClientCount(Fsh);
    stats.ActiveNameMaps = finesse_object_get_table_size(); // only update when asked
    status = FinesseSendServerStatResponse(Fsh, Client, Message, FinesseServerStats, 0);

    if (0 == status) {
        FinesseCountNativeResponse(FINESSE_NATIVE_RSP_SERVER_STAT);
    }

    return status;
}
