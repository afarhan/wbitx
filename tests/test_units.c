// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// test_units.c — the talkie's code that knows nothing of the ESP32, tried on
// this computer: mu-law, the filters, the duty, the cookie and the stretch.
//
//     make check
//
// The stretch done a few rounds at a time must come out as sha256.c's
// pbkdf2_sha256_hex(), which zbitxre's daemon and tests already trust; the
// filters must pass speech at its level and keep what lies above 4 kHz
// down. tests/test_service.py tries the packets against the service's own
// code. Each failure prints a line; the exit status is the number of them.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../wifitalkie/auth.h"
#include "../wifitalkie/playback.h"
#include "../wifitalkie/telephone.h"

#define SETTLE_SAMPLES 200
#define MEASURE_SAMPLES 1600
#define TEST_AMPLITUDE 10000.0
#define PASS_HZ 1000.0
#define STOP_HZ 5000.0
#define PASS_TOLERANCE_DB 1.0
#define STOP_LEAST_DB 30.0
#define STEP_ROUNDS 777
#define PLAYBACK_SETTLE_SAMPLES 2000
#define PLAYBACK_MEASURE_SAMPLES 62500
#define HIGH_PASS_HZ 3000.0
#define IMAGE_LEAST_DB 50.0
#define SPEECH_TAPS 255
#define QUIET_LEVEL (12.0 * 256.0)
#define SHAPING_LEAST_DB 20.0

static int failures;

static void expect(int is_true, const char *what) {
	if (is_true)
		return;
	printf("FAIL: %s\n", what);
	failures++;
}

// the size of one frequency in a run of samples, by Goertzel's recurrence
static double size_at(const double *samples, int count, double hz, double rate) {
	double w = 2.0 * M_PI * hz / rate;
	double c = 2.0 * cos(w);
	double s1 = 0;
	double s2 = 0;
	double s;
	int i;

	for (i = 0; i < count; i++) {
		s = samples[i] + c * s1 - s2;
		s2 = s1;
		s1 = s;
	}
	return 2.0 * sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / count;
}

static double decibels(double ratio) {
	return 20.0 * log10(ratio);
}

static void test_ulaw(void) {
	int code;
	int is_round = 1;

	expect(telephone_ulaw_of(0) == 0xff, "mu-law of 0 is 0xff");
	expect(telephone_ulaw_of(32767) == 0x80, "mu-law of the most is 0x80");
	expect(telephone_ulaw_of(-32767) == 0x00, "mu-law of the least is 0x00");
	for (code = 0; code < 256; code++) {
		// 0x7f is the negative zero, which comes back as 0xff
		if (code == 0x7f)
			continue;
		if (telephone_ulaw_of(telephone_linear_of((uint8_t)code)) != code)
			is_round = 0;
	}
	expect(is_round, "every mu-law code comes back as itself");
}

static double down_size(double hz, double at_hz) {
	struct telephone_down d;
	double kept[MEASURE_SAMPLES];
	int16_t out;
	int made = 0;
	int i;

	telephone_down_begin(&d);
	for (i = 0; made < SETTLE_SAMPLES + MEASURE_SAMPLES; i++) {
		if (!telephone_down_take(&d, (int16_t)lrint(TEST_AMPLITUDE * sin(2.0 * M_PI * hz * i / DOUBLE_RATE_HZ)), &out))
			continue;
		if (made >= SETTLE_SAMPLES)
			kept[made - SETTLE_SAMPLES] = out;
		made++;
	}
	return size_at(kept, MEASURE_SAMPLES, at_hz, TELEPHONE_RATE_HZ);
}

static void test_down(void) {
	double passed = down_size(PASS_HZ, PASS_HZ);
	double folded = down_size(STOP_HZ, TELEPHONE_RATE_HZ - STOP_HZ);

	printf("down: 1 kHz at %+.2f dB, 5 kHz folded to 3 kHz at %+.1f dB\n",
		decibels(passed / TEST_AMPLITUDE), decibels(folded / TEST_AMPLITUDE));
	expect(fabs(decibels(passed / TEST_AMPLITUDE)) < PASS_TOLERANCE_DB, "down passes 1 kHz at its level");
	expect(decibels(folded / TEST_AMPLITUDE) < -STOP_LEAST_DB, "down keeps 5 kHz from folding");
}

// a tone at 8000 a second, made 62500 by playback.c, its level and the
// size of its image at the telephone's rate less it
static void playback_sizes(double hz, double *passed, double *image) {
	struct playback p;
	static double kept[PLAYBACK_MEASURE_SAMPLES];
	uint32_t step = (uint32_t)((uint64_t)PLAYBACK_ONE * TELEPHONE_RATE_HZ / PLAYBACK_RATE_HZ);
	double off = (double)step * PLAYBACK_RATE_HZ / PLAYBACK_ONE / TELEPHONE_RATE_HZ;
	int pushed = 0;
	int made;

	playback_begin(&p);
	for (made = 0; made < PLAYBACK_SETTLE_SAMPLES + PLAYBACK_MEASURE_SAMPLES; made++) {
		while (playback_wants(&p)) {
			playback_push(&p, (int16_t)lrint(TEST_AMPLITUDE * sin(2.0 * M_PI * hz * pushed / TELEPHONE_RATE_HZ)));
			pushed++;
		}
		if (made >= PLAYBACK_SETTLE_SAMPLES)
			kept[made - PLAYBACK_SETTLE_SAMPLES] = playback_make(&p, step);
		else
			playback_make(&p, step);
	}
	// step is rounded, so the tone comes out a hair off its frequency
	*passed = size_at(kept, PLAYBACK_MEASURE_SAMPLES, hz * off, PLAYBACK_RATE_HZ);
	*image = size_at(kept, PLAYBACK_MEASURE_SAMPLES, (TELEPHONE_RATE_HZ - hz) * off, PLAYBACK_RATE_HZ);
}

static void test_playback(void) {
	double passed;
	double image;
	double high;
	double high_image;

	playback_sizes(PASS_HZ, &passed, &image);
	playback_sizes(HIGH_PASS_HZ, &high, &high_image);
	printf("playback: 1 kHz at %+.2f dB, its image at 7 kHz at %+.1f dB; 3 kHz at %+.2f dB, its image at 5 kHz at %+.1f dB\n",
		decibels(passed / TEST_AMPLITUDE), decibels(image / TEST_AMPLITUDE),
		decibels(high / TEST_AMPLITUDE), decibels(high_image / TEST_AMPLITUDE));
	expect(fabs(decibels(passed / TEST_AMPLITUDE)) < PASS_TOLERANCE_DB, "playback passes 1 kHz at its level");
	expect(fabs(decibels(high / TEST_AMPLITUDE)) < PASS_TOLERANCE_DB, "playback passes 3 kHz at its level");
	expect(decibels(image / TEST_AMPLITUDE) < -IMAGE_LEAST_DB, "playback keeps the image at 7 kHz down");
	expect(decibels(high_image / TEST_AMPLITUDE) < -IMAGE_LEAST_DB, "playback keeps the image at 5 kHz down");
}

// the power below 4 kHz of a run of errors, by a long windowed sinc in
// floating point
static double speech_power(const double *errors, int count) {
	double shape[SPEECH_TAPS];
	double wide = 2.0 * (TELEPHONE_RATE_HZ / 2) / PLAYBACK_RATE_HZ;
	double centre = (SPEECH_TAPS - 1) / 2.0;
	double power = 0;
	double sum;
	double x;
	int i;
	int tap;

	for (tap = 0; tap < SPEECH_TAPS; tap++) {
		x = tap - centre;
		if (x == 0)
			shape[tap] = wide;
		else
			shape[tap] = sin(M_PI * wide * x) / (M_PI * x);
		shape[tap] *= 0.54 - 0.46 * cos(2.0 * M_PI * tap / (SPEECH_TAPS - 1));
	}
	for (i = SPEECH_TAPS; i < count; i++) {
		sum = 0;
		for (tap = 0; tap < SPEECH_TAPS; tap++)
			sum += shape[tap] * errors[i - tap];
		power += sum * sum;
	}
	return power / (count - SPEECH_TAPS);
}

// a quiet tone, as VOL 10 makes it, rounded to steps of duty plainly and by
// playback_duty(): the latter's error must be far less below 4 kHz
static void test_duty(void) {
	struct playback p;
	static double shaped[PLAYBACK_MEASURE_SAMPLES];
	static double plain[PLAYBACK_MEASURE_SAMPLES];
	double level;
	double gain;
	int duty;
	int i;

	playback_begin(&p);
	for (i = 0; i < PLAYBACK_MEASURE_SAMPLES; i++) {
		level = QUIET_LEVEL * sin(2.0 * M_PI * PASS_HZ * i / PLAYBACK_RATE_HZ);
		duty = playback_duty(&p, (int32_t)lrint(level)) - PLAYBACK_DUTY_MIDDLE;
		shaped[i] = duty * 256.0 - level;
		plain[i] = floor(level / 256.0 + 0.5) * 256.0 - level;
	}
	gain = 10.0 * log10(speech_power(plain, PLAYBACK_MEASURE_SAMPLES) / speech_power(shaped, PLAYBACK_MEASURE_SAMPLES));
	printf("duty: the rounding's error below 4 kHz is %.1f dB less than plain rounding's\n", gain);
	expect(gain > SHAPING_LEAST_DB, "the duty's rounding error is pushed above the speech");
}

static uint8_t counted_byte(void) {
	static uint8_t next;

	next = (uint8_t)(next * 37 + 11);
	return next;
}

static void test_cookie(void) {
	char cookie[AUTH_COOKIE_ROOM];
	char shown[AUTH_COOKIE_SHOWN_ROOM];

	auth_cookie_make(cookie, counted_byte);
	expect(auth_is_cookie(cookie), "a made cookie is a cookie");
	auth_cookie_show(cookie, shown);
	expect(strlen(shown) == 14 && shown[4] == '-' && shown[9] == '-', "a cookie is shown in fours");
	expect(!auth_is_cookie("abcdefghijkl"), "i and l are not in a cookie");
	expect(!auth_is_cookie("abcd"), "a cookie has twelve letters");
}

static void test_stretch(void) {
	struct auth_stretch s;
	char ours[SHA256_HEX_ROOM];
	char theirs[SHA256_HEX_ROOM];

	auth_stretch_begin(&s, "abcdefghjkmn", "VU2ESE");
	while (!auth_stretch_step(&s, STEP_ROUNDS))
		continue;
	auth_stretch_hex(&s, ours);
	pbkdf2_sha256_hex("abcdefghjkmn", "zbitx-1 vu2ese", AUTH_STRETCH_ROUNDS, theirs);
	expect(!strcmp(ours, theirs), "the stretch in steps is sha256.c's PBKDF2");
}

int main(void) {
	telephone_open();
	playback_open();
	test_ulaw();
	test_down();
	test_playback();
	test_duty();
	test_cookie();
	test_stretch();
	if (failures == 0)
		printf("test_units: all passed\n");
	return failures;
}
