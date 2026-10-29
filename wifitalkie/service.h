// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// service.h — the talkie's line to my.sbitx.net. See service.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void service_open(void);
void service_poll(long now);
void service_restart(void);
void service_describe(char *text, int room);

#ifdef __cplusplus
}
#endif
