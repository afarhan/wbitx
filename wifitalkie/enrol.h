// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// enrol.h — the talkie learning its callsign from my.sbitx.net. See enrol.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void enrol_poll(long now);
int enrol_is_waiting(void);
int enrol_took(void);

#ifdef __cplusplus
}
#endif
