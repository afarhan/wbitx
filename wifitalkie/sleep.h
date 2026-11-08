// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// sleep.h — the talkie asleep. See sleep.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void sleep_now(void);
void sleep_poll(long now);

#ifdef __cplusplus
}
#endif
