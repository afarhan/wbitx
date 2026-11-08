// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// font.h — the two fonts of the display, set in proportion. See font.c.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT_SMALL 0
#define FONT_MAIN 1
#define FONT_COLUMNS 10
#define FONT_WIDE 5          // the small font's widest, for letters in cells
#define FONT_HIGH 7          // the small font's capitals
#define FONT_SMALL_DEEP 8    // its rows, the descenders' too
#define FONT_MAIN_HIGH 8     // the main font's capitals
#define FONT_MAIN_DEEP 10    // its rows, the descenders' too

struct glyph {
	uint8_t advance;         // from this letter's left to the next's
	uint8_t left;            // blank columns before the first drawn
	uint8_t wide;            // columns drawn
	uint16_t columns[FONT_COLUMNS];  // the lowest bit the top row
};

const struct glyph *font_glyph(int font, char c);

#ifdef __cplusplus
}
#endif
