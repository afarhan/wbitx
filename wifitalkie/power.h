// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// power.h — the talkie awake, or idle and listening on a little current.
// See power.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void power_open(void);
void power_poll(long now);
void power_wake(long now);
int power_is_idle(void);
int power_loop_ticks(void);
void power_describe(char *text, int room);

#ifdef __cplusplus
}
#endif
