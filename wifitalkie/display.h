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
void display_text_at(int x, int y, int scale, const char *text);
int display_text_wide(const char *text, int scale);
void display_small_at(int x, int y, const char *text);
int display_small_wide(const char *text);
void display_letter_at(int x, int y, int scale, char c);
void display_fill(int x, int y, int wide, int high);
void display_invert(int x, int y, int wide, int high);
void display_bar(int x, int y, int wide, int filled);
void display_poll(void);
void display_describe(char *text, int room);
void display_off(void);
void display_on(void);

#ifdef __cplusplus
}
#endif
