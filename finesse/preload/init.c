/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <finesse.h>
#include "preload.h"

extern void (*finesse_init)(void);
extern void (*finesse_shutdown)(void);

static void finesse_preload_init(void) __attribute__((constructor));
static void finesse_preload_deinit(void) __attribute__((destructor));

static void finesse_preload_init()
{
    finesse_init();
    return;
}

static void finesse_preload_deinit()
{
    finesse_shutdown();
    return;
}