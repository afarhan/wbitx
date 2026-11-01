// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// dial.h — the frequency stepped by the keys. See dial.c.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DIAL_STEP_HZ 1000
#define DIAL_TENS_AFTER_MS 3000
#define DIAL_HUNDREDS_AFTER_MS 6000
#define DIAL_LEAST_HZ 1000
#define DIAL_MOST_HZ 999999000

uint32_t dial_step(uint32_t hz, int up, long held_ms);

#ifdef __cplusplus
}
#endif
