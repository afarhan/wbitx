// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// playback.h — the telephone's 8000 a second made 62500, and eight bits of
// PWM duty. See playback.c.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLAYBACK_RATE_HZ 62500
#define PLAYBACK_TAPS 24
#define PLAYBACK_ONE 65536
#define PLAYBACK_DUTY_MIDDLE 128
#define PLAYBACK_DUTY_MOST 255

// the last PLAYBACK_TAPS samples heard, twice over so that they can be read
// in a row from any start; where between the two middle ones the next
// sample made falls, PLAYBACK_ONE a sample; and the last two errors of the
// duty
struct playback {
	int16_t past[2 * PLAYBACK_TAPS];
	int at;
	uint32_t fraction;
	int32_t error;
	int32_t error_before;
};

void playback_open(void);
void playback_begin(struct playback *p);
int playback_wants(const struct playback *p);
void playback_push(struct playback *p, int16_t sample);
int16_t playback_make(struct playback *p, uint32_t step);
uint8_t playback_duty(struct playback *p, int32_t level);

#ifdef __cplusplus
}
#endif
