//
// (C) Copyright 2020 Tony Mason
// All Rights Reserved
//

//
// This is the finesse communications package

static int initialized = 0;

void fincomm_init(void)
{
    if (0 == initialized) {
        initialized = 1;
    }
    return;
}

void fincomm_shutdown(void)
{
    if (1 == initialized) {
        initialized = 0;
    }
    return;
}