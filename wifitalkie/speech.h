// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// speech.h — the microphone's samples made fit to send: the hum off and the
// gaps between words quiet. See speech.c.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SPEECH_HIGH_PASS_HZ 300
#define SPEECH_OPEN_TIMES 3
#define SPEECH_OPEN_LEAST 50
#define SPEECH_AHEAD_SAMPLES 160
#define SPEECH_HOLD_MS 100
#define SPEECH_OPENING_MS 2
#define SPEECH_CLOSING_MS 30
#define SPEECH_COMFORT_PART 2

struct speech {
	int64_t b0;
	int64_t b1;
	int64_t b2;
	int64_t a1;
	int64_t a2;
	int32_t x1;
	int32_t x2;
	int32_t y1;
	int32_t y2;
	int32_t envelope;
	int is_open;
	int held;
	int32_t gain;
	int32_t background;
	uint32_t seed;
	int16_t ahead[SPEECH_AHEAD_SAMPLES];
	int ahead_at;
};

void speech_begin(struct speech *s, int rate_hz);
int16_t speech_high_pass(struct speech *s, int16_t sample);
int16_t speech_gate(struct speech *s, int16_t sample, int rate_hz);
int speech_is_open(const struct speech *s);

#ifdef __cplusplus
}
#endif
