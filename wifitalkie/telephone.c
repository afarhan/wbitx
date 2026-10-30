// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// telephone.c — the telephone's sound, and the filters to and from it.
//
// WHY
//
// my.sbitx.net's repeater carries the telephone's sound: G.711 mu-law, a
// byte a sample, 8000 a second, 160 bytes to a packet of twenty
// milliseconds. The microphone is read at twice that, 16000 a second, so
// that what lies above 4 kHz can be filtered off in numbers rather than
// left to fold down among the speech. The speaker's way, up to 62500 a
// second, is playback.c's.
//
// MU-LAW
//
// As G.711 writes it, and as clients/remote/telephone.c in zbitxre does:
// the sign, three bits for which of eight segments the size falls in, four
// for where in the segment, all inverted. The way back is a table of the
// 256, made once.
//
// THE FILTER
//
// 32 taps of a windowed sinc 3.6 kHz wide at 16000 a second, the shape
// zbitxre's gateway uses at 48000; every second sample of the filtered
// sound is kept.
//
// The ESP32-C3 has no floating point unit, so the taps are worked out once
// in floating point and kept as whole numbers, TAP_ONE being one; samples
// are 16-bit and the sums 32-bit. The sum of the taps' sizes is a little
// over one, so a sum of a full-scale sample times every tap stays inside
// 32 bits with TAP_ONE at 2 to the 14th.

#include <math.h>
#include <string.h>

#include "telephone.h"

#define TAPS TELEPHONE_TAPS
#define CUTOFF_HZ 3600.0
#define HAMMING_A 0.54
#define HAMMING_B 0.46
#define TAP_SHIFT 14
#define TAP_ONE (1 << TAP_SHIFT)
#define ULAW_BIAS 0x84
#define ULAW_CLIP 32635
#define ULAW_SEGMENTS 8
#define ULAW_CODES 256
#define INT16_MOST 32767

static int32_t taps[TAPS];
static int16_t linear[ULAW_CODES];

uint8_t telephone_ulaw_of(int sample) {
	int sign = 0;
	int segment = 0;
	int top;

	if (sample < 0) {
		sample = -sample;
		sign = 0x80;
	}
	if (sample > ULAW_CLIP)
		sample = ULAW_CLIP;
	sample += ULAW_BIAS;
	top = sample >> 7;
	while (segment < ULAW_SEGMENTS - 1 && top > 1) {
		top >>= 1;
		segment++;
	}
	return (uint8_t)~(sign | (segment << 4) | ((sample >> (segment + 3)) & 0x0f));
}

int16_t telephone_linear_of(uint8_t ulaw) {
	return linear[ulaw];
}

static int16_t clipped(int32_t sample) {
	if (sample > INT16_MOST)
		return INT16_MOST;
	if (sample < -INT16_MOST)
		return -INT16_MOST;
	return (int16_t)sample;
}

void telephone_open(void) {
	double shape[TAPS];
	double centre = (TAPS - 1) / 2.0;
	double wide = 2.0 * CUTOFF_HZ / DOUBLE_RATE_HZ;
	double sum = 0;
	double x;
	int value;
	int size;
	int i;

	for (i = 0; i < TAPS; i++) {
		x = i - centre;
		shape[i] = sin(M_PI * wide * x) / (M_PI * x)
			* (HAMMING_A - HAMMING_B * cos(2.0 * M_PI * i / (TAPS - 1)));
		sum += shape[i];
	}
	for (i = 0; i < TAPS; i++)
		taps[i] = (int32_t)lrint(shape[i] / sum * TAP_ONE);
	for (i = 0; i < ULAW_CODES; i++) {
		value = ~i & 0xff;
		size = (((value & 0x0f) << 3) + ULAW_BIAS) << ((value & 0x70) >> 4);
		if (value & 0x80)
			linear[i] = (int16_t)(ULAW_BIAS - size);
		else
			linear[i] = (int16_t)(size - ULAW_BIAS);
	}
}

void telephone_down_begin(struct telephone_down *d) {
	memset(d, 0, sizeof(*d));
}

// a sample at 16000 a second is taken; every second one, a sample at 8000
// is made, and 1 is returned
int telephone_down_take(struct telephone_down *d, int16_t sample, int16_t *out) {
	int32_t sum = 0;
	int at;
	int tap;

	d->past[d->at] = sample;
	d->at++;
	if (d->at == TAPS)
		d->at = 0;
	d->is_second = !d->is_second;
	if (d->is_second)
		return 0;
	at = d->at;
	for (tap = 0; tap < TAPS; tap++) {
		sum += taps[tap] * d->past[at];
		at++;
		if (at == TAPS)
			at = 0;
	}
	*out = clipped((sum + TAP_ONE / 2) >> TAP_SHIFT);
	return 1;
}
