// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// test_units.c — the talkie's code that knows nothing of the ESP32, tried on
// this computer: mu-law, the filters, the duty, the keys, the dial, the speech's
// high-pass and gate, the cookie and the stretch.
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
#include <stdlib.h>
#include <string.h>

#include "../wifitalkie/auth.h"
#include "../wifitalkie/button.h"
#include "../wifitalkie/dial.h"
#include "../wifitalkie/playback.h"
#include "../wifitalkie/speech.h"
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
#define HUM_HZ 50.0
#define HUM_LEAST_DB 25.0
#define SPEECH_RATE 8000
#define NOISE_SIZE 30
#define VOICE_SIZE 3000.0
#define QUIET_VOICE_SIZE 120.0
#define GATE_SETTLE_MS 20
#define GATE_LEARN_MS 1000
#define BOUNCE_MS 16
#define KEY_HELD_TRY_MS 3000
#define KEY_SHORT_MS 200

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

// a key's level held from one millisecond to another: how many things it
// said, and the last
static int button_run(struct button *b, int is_pressed, long from, long to, int *last) {
	int count = 0;
	int what;
	long now;

	*last = BUTTON_NOTHING;
	for (now = from; now < to; now++) {
		what = button_step(b, is_pressed, now);
		if (what == BUTTON_NOTHING)
			continue;
		*last = what;
		count++;
	}
	return count;
}

static void test_buttons(void) {
	struct button b;
	int last;
	int count;
	long at;

	button_begin(&b, BUTTON_PLAIN);
	count = 0;
	for (at = 0; at < BOUNCE_MS; at += 2)
		count += button_run(&b, (int)(at / 2 % 2), at, at + 2, &last);
	expect(count == 0, "a key bouncing says nothing");
	count = button_run(&b, 1, BOUNCE_MS, BOUNCE_MS + KEY_HELD_TRY_MS, &last);
	expect(count == 1 && last == BUTTON_PRESSED, "a plain key held says pressed once");
	count = button_run(&b, 0, BOUNCE_MS + KEY_HELD_TRY_MS, BOUNCE_MS + 2 * KEY_HELD_TRY_MS, &last);
	expect(count == 0, "a plain key let go says nothing");

	button_begin(&b, BUTTON_REPEATING);
	count = button_run(&b, 1, 0, BUTTON_DEBOUNCE_MS + BUTTON_REPEAT_AFTER_MS + 2 * BUTTON_REPEAT_EVERY_MS + 1, &last);
	expect(count == 4 && last == BUTTON_REPEATED, "a repeating key held says pressed, then repeats");

	button_begin(&b, BUTTON_HOLDING);
	count = button_run(&b, 1, 0, KEY_SHORT_MS, &last);
	expect(count == 0, "a holding key says nothing going down");
	count = button_run(&b, 0, KEY_SHORT_MS, 2 * KEY_SHORT_MS, &last);
	expect(count == 1 && last == BUTTON_PRESSED, "a holding key let go soon says pressed");
	count = button_run(&b, 1, 2 * KEY_SHORT_MS, 2 * KEY_SHORT_MS + KEY_HELD_TRY_MS, &last);
	expect(count == 1 && last == BUTTON_HELD, "a holding key held says held once");
	count = button_run(&b, 0, 2 * KEY_SHORT_MS + KEY_HELD_TRY_MS, 3 * KEY_SHORT_MS + KEY_HELD_TRY_MS, &last);
	expect(count == 0, "a holding key let go after held says nothing");
	button_begin(&b, BUTTON_REPEATING);
	button_run(&b, 1, 0, BUTTON_FAST_AFTER_MS, &last);
	count = button_run(&b, 1, BUTTON_FAST_AFTER_MS, BUTTON_FAST_AFTER_MS + BUTTON_REPEAT_EVERY_MS, &last);
	expect(count >= BUTTON_REPEAT_EVERY_MS / BUTTON_FAST_EVERY_MS - 1, "a repeating key held long repeats faster");
	printf("buttons: debounce, repeat, press and hold as they should\n");
}

// a tone's level through the high-pass, in dB
static double high_pass_db(double hz) {
	struct speech s;
	double sum = 0;
	int16_t out;
	int i;

	speech_begin(&s, SPEECH_RATE);
	for (i = 0; i < SETTLE_SAMPLES + MEASURE_SAMPLES; i++) {
		out = speech_high_pass(&s, (int16_t)lrint(TEST_AMPLITUDE * sin(2 * M_PI * hz * i / SPEECH_RATE)));
		if (i >= SETTLE_SAMPLES)
			sum += (double)out * out;
	}
	return 20 * log10(sqrt(sum / MEASURE_SAMPLES) / (TEST_AMPLITUDE / sqrt(2)));
}

static int16_t noise_sample(void) {
	static uint32_t next = 1;

	next = next * 1103515245 + 12345;
	return (int16_t)((int)(next >> 16) % (2 * NOISE_SIZE + 1) - NOISE_SIZE);
}

// the largest sample out of the gate over ms milliseconds of noise, with a
// tone of voice_size on it, and the mean size of what came out
static int gate_run(struct speech *s, double voice_size, int ms, double *mean_size) {
	int largest = 0;
	double sum = 0;
	int16_t out;
	int count = ms * SPEECH_RATE / 1000;
	int i;

	for (i = 0; i < count; i++) {
		out = speech_gate(s, (int16_t)(noise_sample() + lrint(voice_size * sin(2 * M_PI * PASS_HZ * i / SPEECH_RATE))), SPEECH_RATE);
		if (abs(out) > largest)
			largest = abs(out);
		sum += abs(out);
	}
	if (mean_size)
		*mean_size = sum / count;
	return largest;
}

// the largest of a voice's first cycle out of the gate, the gate shut when
// it began
static int gate_onset(struct speech *s) {
	int largest = 0;
	int16_t out;
	int i;

	for (i = 0; i < SPEECH_AHEAD_SAMPLES + SPEECH_RATE / (int)PASS_HZ; i++) {
		out = speech_gate(s, (int16_t)(noise_sample() + lrint(VOICE_SIZE * sin(2 * M_PI * PASS_HZ * i / SPEECH_RATE))), SPEECH_RATE);
		if (i >= SPEECH_AHEAD_SAMPLES && abs(out) > largest)
			largest = abs(out);
	}
	return largest;
}

static void test_dial(void) {
	expect(dial_step(21080000, 1, 0) == 21081000, "the dial steps up a kilohertz");
	expect(dial_step(21080000, 0, 0) == 21079000, "the dial steps down a kilohertz");
	expect(dial_step(7074500, 1, 0) == 7075000 && dial_step(7074500, 0, 0) == 7074000,
		"a frequency off the kilohertz comes onto it");
	expect(dial_step(21083000, 1, DIAL_TENS_AFTER_MS) == 21090000, "held, the dial steps ten kilohertz, on round ones");
	expect(dial_step(21083000, 0, DIAL_HUNDREDS_AFTER_MS) == 21000000, "held longer, a hundred");
	expect(dial_step(DIAL_LEAST_HZ, 0, 0) == DIAL_LEAST_HZ, "the dial stops at its bottom");
	expect(dial_step(DIAL_MOST_HZ, 1, DIAL_HUNDREDS_AFTER_MS) == DIAL_MOST_HZ, "the dial stops at its top");
	printf("dial: kilohertz steps, faster held, and its ends\n");
}

static void test_speech(void) {
	struct speech s;
	double noise_size = NOISE_SIZE / 2.0;
	double size;

	expect(high_pass_db(HUM_HZ) < -HUM_LEAST_DB, "the high-pass takes the mains' hum off");
	expect(fabs(high_pass_db(PASS_HZ)) < PASS_TOLERANCE_DB, "the high-pass passes speech at 1 kHz");
	speech_begin(&s, SPEECH_RATE);
	gate_run(&s, 0, GATE_LEARN_MS, NULL);
	gate_run(&s, 0, GATE_SETTLE_MS * 10, &size);
	expect(size > noise_size / SPEECH_COMFORT_PART / 2 && size < noise_size * 0.75,
		"the gate sends noise alone as comfort noise, quieter than it");
	gate_run(&s, VOICE_SIZE, GATE_SETTLE_MS, NULL);
	expect(gate_run(&s, VOICE_SIZE, GATE_SETTLE_MS, NULL) > VOICE_SIZE * 0.95, "the gate passes a voice whole");
	gate_run(&s, 0, SPEECH_HOLD_MS / 2, &size);
	expect(size > noise_size * 0.75, "the gate holds open a while after the voice");
	gate_run(&s, 0, SPEECH_HOLD_MS + SPEECH_CLOSING_MS, NULL);
	gate_run(&s, 0, GATE_SETTLE_MS * 10, &size);
	expect(size < noise_size * 0.75, "the gate shuts after its hold");
	expect(gate_onset(&s) > VOICE_SIZE * 0.9, "the gate is open by a word's first sound");
	speech_begin(&s, SPEECH_RATE);
	gate_run(&s, 0, GATE_LEARN_MS, NULL);
	gate_run(&s, QUIET_VOICE_SIZE, GATE_SETTLE_MS, NULL);
	expect(gate_run(&s, QUIET_VOICE_SIZE, GATE_SETTLE_MS, NULL) > QUIET_VOICE_SIZE * 0.9,
		"a voice a few times the noise opens the gate");
	printf("speech: hum off, speech through, comfort noise %.1f in the gaps for noise of %.1f\n", size, noise_size);
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
	test_buttons();
	test_dial();
	test_speech();
	test_cookie();
	test_stretch();
	if (failures == 0)
		printf("test_units: all passed\n");
	return failures;
}
