// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// playback.c — the telephone's sound made fit for the PWM.
//
// WHY
//
// The speech comes at 8000 a second. Played as it came, each sample held
// until the next, it brings images of itself about 8 kHz, 16 kHz and on,
// a hiss above the speech that the filter outside the chip, a few kHz wide,
// cannot take away. And eight bits of duty are few: at the VOL that earbuds
// want, speech moves the duty by a dozen steps, and the error of rounding
// it to a step is a hiss of its own, spread evenly up to half the rate the
// duty changes at.
//
// So the duty is changed PLAYBACK_RATE_HZ times a second, nearly eight
// times the telephone's rate. Every sample made there comes from the
// telephone's by a low-pass filter, which leaves nothing of the images
// below 4 kHz short of 62.5 kHz; and the error of rounding each to a step is
// pushed up, out of the speech, to where the filter outside the chip and
// the earbuds take it away.
//
// The rate is 62500, not 64000, eight times 8000, because the PWM runs at
// 312500 cycles a second, and 62500 is a fifth of that: every sample is
// held for five whole cycles. At a rate that did not divide it, some
// samples would be held a cycle longer than others, over and over in a
// pattern, and the speech would be heard with copies of itself beside it.
//
// THE FILTER
//
// The next sample made falls somewhere between two of the telephone's, by
// fraction, and is the sum of the PLAYBACK_TAPS about it, each times a
// windowed sinc 3.6 kHz wide taken at its distance from that point. The
// sinc is worked out for PHASES points between two samples, each row of
// taps made to add up to one, so that a steady level stays the level it
// was; between two of the points, the two sums are taken and a straight
// line drawn between them. fraction goes up by step each sample made, a
// little more than 8000 / 62500 of PLAYBACK_ONE; speaker.c moves it a
// hair's breadth either way to keep its queue as full as it wants, and
// when fraction passes one, the next of the telephone's samples is wanted.
// No sample is ever dropped or made twice.
//
// THE DUTY
//
// The level to play comes in 256ths of a step of duty. Each is rounded to
// a step, after taking off twice the last error of rounding and putting
// back the one before: the error that reaches the earbuds is the rounding's
// own, less what it was a sample ago, less that again, which is little at
// the low frequencies of speech and most near 31 kHz. Above about 10 kHz it
// is more than plain rounding would make, and that is where the filter
// outside takes it off. The errors kept are held to ERROR_MOST so that a
// level clipped at the top or bottom of the duty does not wind them up.
//
// The C3 has no floating point unit; the taps are worked out once, in
// floating point, and kept as whole numbers, TAP_ONE being one.

#include <math.h>
#include <string.h>

#include "playback.h"
#include "telephone.h"

#define TAPS PLAYBACK_TAPS
#define HALF_TAPS (TAPS / 2)
#define PHASE_BITS 6
#define PHASES (1 << PHASE_BITS)
#define FRACTION_BITS 16
#define BETWEEN_BITS (FRACTION_BITS - PHASE_BITS)
#define BETWEEN_MASK ((1 << BETWEEN_BITS) - 1)
#define CUTOFF_HZ 3600.0
#define HAMMING_A 0.54
#define HAMMING_B 0.46
#define TAP_SHIFT 14
#define TAP_ONE (1 << TAP_SHIFT)
#define LEVEL_SHIFT 8
#define LEVEL_HALF_STEP (1 << (LEVEL_SHIFT - 1))
#define ERROR_MOST (2 << LEVEL_SHIFT)
#define INT16_MOST 32767

// taps[phase][tap]: tap 0 multiplies the oldest of the PLAYBACK_TAPS samples;
// a row more than PHASES, the next sample's first point, for the line
// drawn from the last
static int16_t taps[PHASES + 1][TAPS];

static int16_t clipped(int32_t sample) {
	if (sample > INT16_MOST)
		return INT16_MOST;
	if (sample < -INT16_MOST)
		return -INT16_MOST;
	return (int16_t)sample;
}

void playback_open(void) {
	double shape[TAPS];
	double wide = 2.0 * CUTOFF_HZ / TELEPHONE_RATE_HZ;
	double sum;
	double x;
	int phase;
	int i;

	for (phase = 0; phase <= PHASES; phase++) {
		sum = 0;
		for (i = 0; i < TAPS; i++) {
			// how far this sample is from the point made, in samples
			x = i - (HALF_TAPS - 1) - (double)phase / PHASES;
			if (x == 0)
				shape[i] = wide;
			else
				shape[i] = sin(M_PI * wide * x) / (M_PI * x);
			shape[i] *= HAMMING_A + HAMMING_B * cos(M_PI * x / HALF_TAPS);
			sum += shape[i];
		}
		for (i = 0; i < TAPS; i++)
			taps[phase][i] = (int16_t)lrint(shape[i] / sum * TAP_ONE);
	}
}

void playback_begin(struct playback *p) {
	memset(p, 0, sizeof(*p));
}

// whether the next of the telephone's samples must be pushed before the
// next is made
int playback_wants(const struct playback *p) {
	return p->fraction >= PLAYBACK_ONE;
}

void playback_push(struct playback *p, int16_t sample) {
	p->at++;
	if (p->at == TAPS)
		p->at = 0;
	p->past[p->at] = sample;
	p->past[p->at + TAPS] = sample;
	if (p->fraction >= PLAYBACK_ONE)
		p->fraction -= PLAYBACK_ONE;
}

static int32_t sum_of(const int16_t *row, const int16_t *samples) {
	int32_t sum = 0;
	int i;

	for (i = 0; i < TAPS; i++)
		sum += (int32_t)row[i] * samples[i];
	return sum;
}

// the next sample at PLAYBACK_RATE_HZ; step is how far the one after it
// lies, PLAYBACK_ONE being one of the telephone's samples
int16_t playback_make(struct playback *p, uint32_t step) {
	const int16_t *samples = &p->past[p->at + 1];
	uint32_t fraction = p->fraction;
	int phase;
	int32_t here;
	int32_t next;
	int32_t between;

	if (fraction >= PLAYBACK_ONE)
		fraction = PLAYBACK_ONE - 1;
	phase = (int)(fraction >> BETWEEN_BITS);
	between = (int32_t)(fraction & BETWEEN_MASK);
	here = (sum_of(taps[phase], samples) + TAP_ONE / 2) >> TAP_SHIFT;
	next = (sum_of(taps[phase + 1], samples) + TAP_ONE / 2) >> TAP_SHIFT;
	p->fraction += step;
	return clipped(here + (((next - here) * between) >> BETWEEN_BITS));
}

// level, in 256ths of a step of duty about the middle, made a duty with the
// error of rounding pushed above the speech
uint8_t playback_duty(struct playback *p, int32_t level) {
	int32_t wanted = level - 2 * p->error + p->error_before;
	int32_t step = (wanted + LEVEL_HALF_STEP) >> LEVEL_SHIFT;
	int32_t error;

	if (step < -PLAYBACK_DUTY_MIDDLE)
		step = -PLAYBACK_DUTY_MIDDLE;
	if (step > PLAYBACK_DUTY_MOST - PLAYBACK_DUTY_MIDDLE)
		step = PLAYBACK_DUTY_MOST - PLAYBACK_DUTY_MIDDLE;
	error = (step << LEVEL_SHIFT) - wanted;
	if (error > ERROR_MOST)
		error = ERROR_MOST;
	if (error < -ERROR_MOST)
		error = -ERROR_MOST;
	p->error_before = p->error;
	p->error = error;
	return (uint8_t)(PLAYBACK_DUTY_MIDDLE + step);
}
