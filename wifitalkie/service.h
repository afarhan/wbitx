// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// service.h — the talkie's line to my.sbitx.net. See service.c.

#pragma once

#include "auth.h"

#ifdef __cplusplus
extern "C" {
#endif

void service_open(void);
void service_poll(long now);
void service_restart(void);
void service_new_key(void);
void service_describe(char *text, int room);
void service_brief(char *text, int room);
void service_code(char shown[AUTH_COOKIE_SHOWN_ROOM]);
int service_is_online(void);
int service_is_unregistered(void);

#ifdef __cplusplus
}
#endif
