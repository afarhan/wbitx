// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// update.h — new firmware, sent from a browser to the setup portal. See update.cpp.

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UPDATE_TEXT_ROOM 32

void update_open(void);
void update_close(void);
int update_is_open(void);
int update_code(void);
void update_said(char *text, int room);
void update_poll(long now);

// for portal.cpp, as the file comes in
int update_begin(size_t total);
int update_take(const uint8_t *data, size_t length);
int update_end(void);
void update_abort(const char *why);

#ifdef __cplusplus
}
#endif
