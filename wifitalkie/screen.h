// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// screen.h — what the display shows and what the keys do. See screen.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void screen_open(void);
void screen_poll(long now);

#ifdef __cplusplus
}
#endif
