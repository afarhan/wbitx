// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// speech.c — the microphone's samples made fit to send, at 8000 a second.
//
// First a high-pass at SPEECH_HIGH_PASS_HZ, two poles, Butterworth: the
// talkie's noise, measured on 1 November 2026, has a comb of the mains' 50
// Hz and its harmonics below 400 Hz, and speech on a telephone has nothing
// there that is missed. Its coefficients are worked out once, by the
// bilinear transform, and kept as fractions of 2^COEFFICIENT_BITS.
//
// Then a gate. The level is followed as the mean of the samples' size over
// a few milliseconds, and so is the background's, slowly, while the gate is
// shut. The gate opens when the level is SPEECH_OPEN_TIMES the background,
// and shuts SPEECH_HOLD_MS after the level was last that high, so a word's
// quiet end and the breath between words are not cut. Measured against the
// background, a quiet voice opens it in a quiet room, and a noisier talkie
// does not hold it open; SPEECH_OPEN_LEAST, in 16-bit samples' size, keeps
// it shut while the background is still being learnt. There is no lower
// level to shut it at: the noise's own level, on 1 November 2026, went
// above twice the background often enough to have held it open for ever. The noise's mean size, at MIC 50, was
// some 13 to 25 on 1 November 2026, and speech at 5 cm some hundreds. The
// background is not learnt while the gate is open, or a long over would
// raise it and shut out the quiet words that follow; a steady new noise
// loud enough to open the gate keeps it open, and is sent, as it would be
// without a gate.
//
// The gate decides on each sample as it comes but lets out what came
// SPEECH_AHEAD_SAMPLES before, 20 ms at 8000 a second: the gate is open by
// the time a word's first sound goes through it, not opening on it. It
// opens over SPEECH_OPENING_MS and shuts over SPEECH_CLOSING_MS, not at
// once, which would click.
//
// A gate shut is not sent as silence, which sounds as if the line had
// dropped, but as comfort noise: random samples whose mean size is the
// background's over SPEECH_COMFORT_PART. The gate's gain fades between the
// voice and the comfort noise, so the one gives way to the other.

#include <math.h>
#include <string.h>

#include "speech.h"

#define COEFFICIENT_BITS 28
#define ENVELOPE_BITS 8
#define ENVELOPE_SHIFT 5
#define BACKGROUND_SHIFT 10
#define SEED 1
#define RANDOM_MULTIPLIER 1664525u
#define RANDOM_INCREMENT 1013904223u
#define RANDOM_SHIFT 16
#define GAIN_FULL 32768
#define GAIN_BITS 15
#define INT16_MOST 32767
#define MS_A_SECOND 1000
#define PI 3.14159265358979323846

void speech_begin(struct speech *s, int rate_hz) {
	double k = tan(PI * SPEECH_HIGH_PASS_HZ / rate_hz);
	double root2 = sqrt(2.0);
	double norm = 1.0 / (1.0 + root2 * k + k * k);
	double one = (double)((int64_t)1 << COEFFICIENT_BITS);

	s->b0 = (int64_t)llround(norm * one);
	s->b1 = -2 * s->b0;
	s->b2 = s->b0;
	s->a1 = (int64_t)llround(2.0 * (k * k - 1.0) * norm * one);
	s->a2 = (int64_t)llround((1.0 - root2 * k + k * k) * norm * one);
	s->x1 = 0;
	s->x2 = 0;
	s->y1 = 0;
	s->y2 = 0;
	s->envelope = 0;
	s->is_open = 0;
	s->held = 0;
	s->gain = 0;
	s->background = 0;
	s->seed = SEED;
	memset(s->ahead, 0, sizeof(s->ahead));
	s->ahead_at = 0;
}

static int16_t clamp(int64_t value) {
	if (value > INT16_MOST)
		return INT16_MOST;
	if (value < -INT16_MOST)
		return -INT16_MOST;
	return (int16_t)value;
}

int16_t speech_high_pass(struct speech *s, int16_t sample) {
	int64_t sum = s->b0 * sample + s->b1 * s->x1 + s->b2 * s->x2 - s->a1 * s->y1 - s->a2 * s->y2;
	int32_t out = (int32_t)(sum >> COEFFICIENT_BITS);

	s->x2 = s->x1;
	s->x1 = sample;
	s->y2 = s->y1;
	s->y1 = out;
	return clamp(out);
}

// a random sample whose mean size is size: the mean size of one spread
// evenly from -2 size to 2 size
static int32_t comfort(struct speech *s, int32_t size) {
	int32_t spread = 2 * size;

	s->seed = s->seed * RANDOM_MULTIPLIER + RANDOM_INCREMENT;
	if (spread == 0)
		return 0;
	return (int32_t)((s->seed >> RANDOM_SHIFT) % (uint32_t)(2 * spread + 1)) - spread;
}

// the level that opens the gate: SPEECH_OPEN_TIMES the background, or
// SPEECH_OPEN_LEAST
static int32_t above(const struct speech *s) {
	int32_t level = s->background * SPEECH_OPEN_TIMES;

	if (level < (SPEECH_OPEN_LEAST << ENVELOPE_BITS))
		level = SPEECH_OPEN_LEAST << ENVELOPE_BITS;
	return level;
}

int16_t speech_gate(struct speech *s, int16_t sample, int rate_hz) {
	int32_t size = sample;
	int32_t noise;
	int16_t out;
	int32_t opening = GAIN_FULL * MS_A_SECOND / (SPEECH_OPENING_MS * rate_hz);
	int32_t closing = GAIN_FULL * MS_A_SECOND / (SPEECH_CLOSING_MS * rate_hz);

	if (size < 0)
		size = -size;
	s->envelope += ((size << ENVELOPE_BITS) - s->envelope) >> ENVELOPE_SHIFT;
	if (s->envelope >= above(s)) {
		s->is_open = 1;
		s->held = 0;
	} else if (s->is_open) {
		s->held++;
		if (s->held >= SPEECH_HOLD_MS * rate_hz / MS_A_SECOND)
			s->is_open = 0;
	}
	if (!s->is_open)
		s->background += (s->envelope - s->background) >> BACKGROUND_SHIFT;
	if (s->is_open)
		s->gain += opening;
	else
		s->gain -= closing;
	if (s->gain > GAIN_FULL)
		s->gain = GAIN_FULL;
	if (s->gain < 0)
		s->gain = 0;
	out = s->ahead[s->ahead_at];
	s->ahead[s->ahead_at] = sample;
	s->ahead_at = (s->ahead_at + 1) % SPEECH_AHEAD_SAMPLES;
	noise = comfort(s, (s->background >> ENVELOPE_BITS) / SPEECH_COMFORT_PART);
	return clamp(((int64_t)out * s->gain + (int64_t)noise * (GAIN_FULL - s->gain)) >> GAIN_BITS);
}

int speech_is_open(const struct speech *s) {
	return s->is_open;
}
