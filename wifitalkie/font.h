// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// font.h — letters five dots wide and seven high, for the display. See
// font.c.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT_WIDE 5
#define FONT_HIGH 7
#define FONT_STEP 6

const uint8_t *font_letter(char c);

#ifdef __cplusplus
}
#endif
