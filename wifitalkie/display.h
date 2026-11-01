// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// display.h — the 0.96 inch OLED, 128 by 64 dots. See display.c.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_WIDE 128
#define DISPLAY_HIGH 64
#define DISPLAY_ROWS 8
#define DISPLAY_ROW_HIGH 8

int display_open(void);
void display_clear(void);
void display_text(int x, int row, int scale, const char *text);
void display_fill(int x, int y, int wide, int high);
void display_bar(int x, int row, int wide, int filled);
void display_poll(void);
void display_off(void);
void display_on(void);

#ifdef __cplusplus
}
#endif
