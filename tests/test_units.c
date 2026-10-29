// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// test_units.c — the talkie's code that knows nothing of the ESP32, tried on
// this computer: mu-law, the filters, the cookie and the stretch.
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
#include "../wifitalkie/telephone.h"

#define SETTLE_SAMPLES 200
#define MEASURE_SAMPLES 1600
#define TEST_AMPLITUDE 10000.0
#define PASS_HZ 1000.0
#define STOP_HZ 5000.0
#define PASS_TOLERANCE_DB 1.0
#define STOP_LEAST_DB 30.0
#define STEP_ROUNDS 777

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

static void test_up(void) {
	struct telephone_up u;
	double kept[2 * MEASURE_SAMPLES];
	int16_t out[2];
	double passed;
	double image;
	int i;

	telephone_up_begin(&u);
	for (i = 0; i < SETTLE_SAMPLES + MEASURE_SAMPLES; i++) {
		telephone_up_make(&u, (int16_t)lrint(TEST_AMPLITUDE * sin(2.0 * M_PI * PASS_HZ * i / TELEPHONE_RATE_HZ)), out);
		if (i < SETTLE_SAMPLES)
			continue;
		kept[2 * (i - SETTLE_SAMPLES)] = out[0];
		kept[2 * (i - SETTLE_SAMPLES) + 1] = out[1];
	}
	passed = size_at(kept, 2 * MEASURE_SAMPLES, PASS_HZ, DOUBLE_RATE_HZ);
	image = size_at(kept, 2 * MEASURE_SAMPLES, TELEPHONE_RATE_HZ - PASS_HZ, DOUBLE_RATE_HZ);
	printf("up: 1 kHz at %+.2f dB, its image at 7 kHz at %+.1f dB\n",
		decibels(passed / TEST_AMPLITUDE), decibels(image / TEST_AMPLITUDE));
	expect(fabs(decibels(passed / TEST_AMPLITUDE)) < PASS_TOLERANCE_DB, "up passes 1 kHz at its level");
	expect(decibels(image / TEST_AMPLITUDE) < -STOP_LEAST_DB, "up keeps the image at 7 kHz down");
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
	test_ulaw();
	test_down();
	test_up();
	test_cookie();
	test_stretch();
	if (failures == 0)
		printf("test_units: all passed\n");
	return failures;
}
