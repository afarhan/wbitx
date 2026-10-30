// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// telephone.h — the telephone's sound, mu-law at 8000 a second, and the
// filter down to it from the 16000 a second of the microphone. See
// telephone.c.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TELEPHONE_RATE_HZ 8000
#define TELEPHONE_FRAME_SAMPLES 160
#define TELEPHONE_FRAME_MS 20
#define DOUBLE_RATE_HZ 16000
#define TELEPHONE_TAPS 32

// down from 16000 a second: the last TELEPHONE_TAPS samples, going round
struct telephone_down {
	int16_t past[TELEPHONE_TAPS];
	int at;
	int is_second;
};

void telephone_open(void);
uint8_t telephone_ulaw_of(int sample);
int16_t telephone_linear_of(uint8_t ulaw);
void telephone_down_begin(struct telephone_down *d);
int telephone_down_take(struct telephone_down *d, int16_t sample, int16_t *out);

#ifdef __cplusplus
}
#endif
