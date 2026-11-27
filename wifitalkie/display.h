// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// display.h — the 0.96 inch OLED, 128 by 64 dots. See display.c.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// the panel, 128 by 64 dots, in DISPLAY_ROWS pages of DISPLAY_ROW_HIGH
#define DISPLAY_PANEL_WIDE 128
#define DISPLAY_PANEL_HIGH 64
#define DISPLAY_ROWS 8
#define DISPLAY_ROW_HIGH 8
// what is drawn on: the panel less DISPLAY_MARGIN all round, so that no
// text touches the edge, where on the white theme its dark dots ran into
// the dark border round the glass (the author's, 17 November 2026); x and
// y given to the functions below are from its top left
#define DISPLAY_MARGIN 1
#define DISPLAY_WIDE (DISPLAY_PANEL_WIDE - 2 * DISPLAY_MARGIN)
#define DISPLAY_HIGH (DISPLAY_PANEL_HIGH - 2 * DISPLAY_MARGIN)

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
void display_bar_leftward(int x, int y, int wide, int filled);
void display_poll(void);
void display_describe(char *text, int room);
void display_off(void);
void display_on(void);
void display_white(int is_white);

#ifdef __cplusplus
}
#endif
