// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// keys.h — the front panel's four keys. See keys.c.

#pragma once

#include "button.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KEY_UP 0
#define KEY_DOWN 1
#define KEY_MENU 2
#define KEY_ENTER 3
#define KEYS 4

struct key_press {
	int key;
	int what;
	long held_ms;
};

void keys_open(void);
int keys_poll(long now, struct key_press *press);
const char *keys_name(int key);
void keys_describe(char *text, int room);

#ifdef __cplusplus
}
#endif
