// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// test_units.c — the talkie's code that knows nothing of the ESP32, tried on
// this computer: mu-law, the filters, the duty, the keys, the dial, the text entry, the letters halved, the networks remembered, the speech's
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
#include "../wifitalkie/battery.h"
#include "../wifitalkie/button.h"
#include "../wifitalkie/dial.h"
#include "../wifitalkie/entry.h"
#include "../wifitalkie/halves.h"
#include "../wifitalkie/contacts.h"
#include "../wifitalkie/known.h"
#include "../wifitalkie/playback.h"
#include "../wifitalkie/settings.h"
#include "../wifitalkie/speech.h"
#include "../wifitalkie/vox.h"
#include "../wifitalkie/telephone.h"
#include "../wifitalkie/voip_packet.h"

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
	button_begin(&b, BUTTON_HOLDING_REPEATING);
	count = button_run(&b, 1, 0, BUTTON_DEBOUNCE_MS + BUTTON_HELD_MS + 2 * BUTTON_HELD_EVERY_MS + 1, &last);
	expect(count == 3 && last == BUTTON_REPEATED, "a holding, repeating key held says held, then repeats");
	count = button_run(&b, 0, BUTTON_DEBOUNCE_MS + BUTTON_HELD_MS + 2 * BUTTON_HELD_EVERY_MS + 1,
		3 * BUTTON_HELD_MS, &last);
	expect(count == 0, "a holding, repeating key let go after held says nothing");

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

static void test_entry(void) {
	struct entry e;
	int i;

	entry_begin(&e, ENTRY_CALLSIGN, "", SETTINGS_CALLSIGN_ROOM - 1);
	expect(entry_around(&e, 0) == 'A' && entry_around(&e, -1) == '/' && entry_around(&e, 1) == 'B',
		"the entry starts at the set's first, its neighbours round it");
	entry_scroll(&e, 0);
	expect(entry_around(&e, 0) == '/', "scrolling down from the first goes round to the last");
	entry_scroll(&e, 1);
	entry_scroll(&e, 1);
	entry_add(&e);
	for (i = 0; i < 21; i++)
		entry_scroll(&e, 1);
	entry_add(&e);
	expect(!strcmp(e.text, "BW"), "characters are added as chosen");
	entry_delete(&e);
	expect(!strcmp(e.text, "B"), "the last is taken off");
	expect(entry_delete(&e) && !entry_delete(&e) && e.text[0] == 0, "an empty text stays empty");
	entry_begin(&e, ENTRY_ANY, "VU2ESE", 8);
	expect(e.length == 6 && entry_around(&e, 0) == 'a', "an entry begins with the text it is given");
	entry_add(&e);
	entry_add(&e);
	expect(!entry_add(&e) && !strcmp(e.text, "VU2ESEaa"), "the text stops at its most");
	expect((int)strlen(ENTRY_ANY) == '~' - ' ' + 1, "the whole set is all of printable ASCII");
	printf("entry: scrolled round, added, taken off, kept to its length\n");
}

#define KEYED_WPM 12
#define KEYED_UNIT_MS (HALVES_MS_A_WPM / KEYED_WPM)
#define KEYED_TURN_MS 5

// the paddles held as they are from *now for ms, a turn at a time; the
// letter taken, if one is, in *letter
static int keyed_for(struct halves *h, int is_up, int is_down, long *now, long ms, char *letter) {
	long until = *now + ms;
	int taken = 0;

	for (; *now < until; *now += KEYED_TURN_MS) {
		if (halves_keys(h, is_up, is_down, *now, letter))
			taken = 1;
	}
	return taken;
}

// the letter keyed, '.' a dit and '-' a dah, each key pressed a turn and
// let go for a unit, then the gap between letters; 0 if nothing was taken
static char halved(const char *code) {
	struct halves h;
	long now = 0;
	char letter = 0;

	halves_begin(&h, KEYED_WPM);
	for (; *code != 0; code++) {
		keyed_for(&h, *code == '.', *code == '-', &now, KEYED_TURN_MS, &letter);
		keyed_for(&h, 0, 0, &now, KEYED_UNIT_MS, &letter);
	}
	if (!keyed_for(&h, 0, 0, &now, KEYED_UNIT_MS * HALVES_LETTER_UNITS, &letter))
		return 0;
	return letter;
}

// the letter keyed with a key held for ms, then let go
static char held(int is_up, int is_down, long ms) {
	struct halves h;
	long now = 0;
	char letter = 0;

	halves_begin(&h, KEYED_WPM);
	keyed_for(&h, is_up, is_down, &now, ms, &letter);
	if (!keyed_for(&h, 0, 0, &now, KEYED_UNIT_MS * HALVES_LETTER_UNITS + KEYED_TURN_MS, &letter))
		return 0;
	return letter;
}

static void test_halves(void) {
	static const char *codes[] = {
		"E.", "T-", "I..", "A.-", "N-.", "M--", "S...", "U..-",
		"R.-.", "W.--", "D-..", "K-.-", "O---", "H....", "V...-",
		"F..-.", "L.-..", "P.--.", "J.---", "B-...", "X-..-", "C-.-.",
		"Y-.--", "Z--..", "Q--.-", "?..--", "0-----", "1-----.", ")------",
	};
	struct halves h;
	char letter = 0;
	long now;
	int i;

	for (i = 0; i < (int)(sizeof(codes) / sizeof(codes[0])); i++)
		expect(halved(codes[i] + 1) == codes[i][0], "each character is its Morse code, UP a dit");
	expect(held(1, 0, 3 * HALVES_FIGURES_HELD_MS) == 'E' && held(0, 1, HALVES_FIGURES_HELD_MS / 2) == 'T',
		"a key held is one element, and does not repeat");
	expect(held(0, 1, HALVES_FIGURES_HELD_MS + KEYED_TURN_MS) == '0', "DOWN held brings the figures, at 0");
	expect(halved("-----.....") == '5' && halved("------------") == ',', "and along them, kept to their ends");
	halves_begin(&h, KEYED_WPM);
	expect(halves_offered(&h, 0, 7) && halves_offered(&h, 1, 7) && !halves_offered(&h, 0, 3),
		"E and T are offered first");
	now = 0;
	keyed_for(&h, 1, 0, &now, KEYED_TURN_MS, &letter);
	expect(halves_offered(&h, 0, 3) && halves_offered(&h, 0, 11) && !halves_offered(&h, 1, 7),
		"after E, I and A");
	expect(!keyed_for(&h, 0, 0, &now, KEYED_UNIT_MS * HALVES_LETTER_UNITS - KEYED_TURN_MS, &letter),
		"nothing is taken before the gap");
	expect(keyed_for(&h, 0, 0, &now, 2 * KEYED_TURN_MS, &letter) && letter == 'E' && h.at < 0,
		"taken after the gap, then nothing is chosen");
	keyed_for(&h, 0, 1, &now, KEYED_TURN_MS, &letter);
	halves_abort(&h);
	expect(h.at < 0 && !keyed_for(&h, 0, 0, &now, 10 * HALVES_FIGURES_HELD_MS, &letter), "dropped, nothing is taken");
	halves_begin(&h, 1);
	expect(h.unit_ms == HALVES_MS_A_WPM / HALVES_WPM_LEAST, "the speed is kept to its least");
	halves_begin(&h, KEYED_WPM);
	expect(h.unit_ms == 100, "12 WPM is a unit of 100 ms");
	printf("halves: thirty letters and sixteen figures, a press an element\n");
}

static void test_known(void) {
	struct settings s;
	char ssid[SETTINGS_SSID_ROOM];
	int i;

	memset(&s, 0, sizeof(s));
	expect(known_find(&s, "home") < 0 && known_find(&s, "") < 0, "nothing is known at first");
	expect(known_remember(&s, "home", "one") && known_find(&s, "home") == 0, "a network joined is remembered");
	expect(!known_remember(&s, "home", "one"), "the same again changes nothing");
	known_remember(&s, "hotspot", "two");
	expect(known_find(&s, "hotspot") == 0 && known_find(&s, "home") == 1, "the last joined is first");
	known_remember(&s, "home", "three");
	expect(known_find(&s, "home") == 0 && !strcmp(s.known_password[0], "three")
		&& known_find(&s, "hotspot") == 1, "joined again, it comes to the front with its new key");
	for (i = 0; i < SETTINGS_KNOWN; i++) {
		snprintf(ssid, sizeof(ssid), "club%d", i);
		known_remember(&s, ssid, "x");
	}
	expect(known_find(&s, "home") < 0 && known_find(&s, "club0") == SETTINGS_KNOWN - 1,
		"the one joined longest ago is forgotten for a new one");
	expect(!known_remember(&s, "", "x"), "a network without a name is not remembered");
	expect(known_count(&s) == SETTINGS_KNOWN, "the list is full");
	expect(known_forget(&s, 0) && known_find(&s, "club8") == 0 && known_count(&s) == SETTINGS_KNOWN - 1
		&& s.known_ssid[SETTINGS_KNOWN - 1][0] == 0, "one forgotten, those after it move up");
	expect(!known_forget(&s, SETTINGS_KNOWN - 1) && !known_forget(&s, -1), "nothing there, nothing forgotten");
	printf("known: remembered, last first, the oldest forgotten\n");
}

static void test_contacts(void) {
	struct settings s;
	char call[SETTINGS_CALLSIGN_ROOM];
	int i;

	memset(&s, 0, sizeof(s));
	expect(contacts_count(&s) == 0 && contacts_find(&s, "VU2ESE") < 0, "no contacts at first");
	expect(contacts_add(&s, " vu2ese ") && !strcmp(s.contacts[0], "VU2ESE"), "added in capitals, trimmed");
	expect(!contacts_add(&s, "VU2ESE"), "not added twice");
	expect(!contacts_add(&s, "VU2 ESE") && !contacts_add(&s, "VU2-ESE") && !contacts_add(&s, "  "),
		"not a callsign, not added");
	expect(!contacts_add(&s, "ABCDEFGHIJKLMNOP"), "too long, not added");
	expect(contacts_add(&s, "VU3XYZ/P") && contacts_find(&s, "VU3XYZ/P") == 1, "a stroke is kept");
	expect(contacts_forget(&s, 0) && !strcmp(s.contacts[0], "VU3XYZ/P") && contacts_count(&s) == 1,
		"one taken out, those after it move up");
	for (i = 1; i < SETTINGS_CONTACTS; i++) {
		snprintf(call, sizeof(call), "VU%d", i);
		contacts_add(&s, call);
	}
	expect(contacts_count(&s) == SETTINGS_CONTACTS && !contacts_add(&s, "W1AW"), "full, no more added");
	printf("contacts: added in capitals, once, taken out\n");
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

// Local's packet: the callsign written in comes out, cut to ten, and the
// tag is checked
static void test_local_packet(void) {
	static const uint8_t id[VOIP_STREAM_ID_BYTES] = {'W', 'T', 'L', 'O', 'C', 'A', 'L', '1'};
	uint8_t packet[VOIP_PACKET_BYTES];
	uint8_t sound[VOIP_SOUND_BYTES];
	struct voip_heard heard;

	memset(sound, 0x55, sizeof(sound));
	voip_packet_make_as(packet, id, "local", "VU2TUV", VOIP_KIND_SPEECH, 21090000, 7, sound);
	expect(voip_packet_open(packet, sizeof(packet), id, "local", &heard) == VOIP_OPENED,
		"a local packet opens");
	expect(!strcmp(heard.callsign, "VU2TUV") && heard.hz == 21090000 && heard.number == 7,
		"a local packet says who sent it, where and its number");
	voip_packet_make_as(packet, id, "local", "VU2TUV/P/QRP", VOIP_KIND_SPEECH, 1, 1, sound);
	voip_packet_open(packet, sizeof(packet), id, "local", &heard);
	expect(!strcmp(heard.callsign, "VU2TUV/P/Q"), "a callsign past ten is cut");
	packet[VOIP_PACKET_BYTES - 1] ^= 1;
	expect(voip_packet_open(packet, sizeof(packet), id, "local", &heard) == VOIP_WRONG_TAG,
		"a local packet spoilt is refused");
}

// the pin's volts for a cell, as the divider and the pull-up make them
static int pin_for(double cell) {
	double r1 = BATTERY_TOP_OHMS, r2 = BATTERY_BOTTOM_OHMS, rp = BATTERY_PULLUP_OHMS;
	double rail = (BATTERY_SAG_AT_0_MV + cell * BATTERY_SAG_PER_MILLE) / 1000.0;

	if (rail > BATTERY_RAIL_MV / 1000.0)
		rail = BATTERY_RAIL_MV / 1000.0;

	return (int)(1000 * (cell / r1 + rail / rp) / (1 / r1 + 1 / r2 + 1 / rp) + 0.5);
}

static void test_battery(void) {
	expect(abs(pin_for(3.7) - 2345) <= 2, "the pin at a cell of 3.7 V, the rail sagged, is 2.35 V");
	expect(pin_for(3.7) * 1000 >= 750 * (BATTERY_SAG_AT_0_MV + 3.7 * BATTERY_SAG_PER_MILLE),
		"at 3.7 V the pin still reads high on the sagged rail");
	expect(abs(battery_cell_mv(pin_for(4.0), 0) - 4000) <= 5, "4.0 V, under the knee, reads back");
	expect(abs(battery_cell_mv(pin_for(5.0), 0) - 5000) <= 5, "5.0 V, over the knee, reads back");
	expect(abs(battery_cell_mv(2361, 0) - 3740) <= 20, "the bench's 3.7 V point reads 3.74 V");
	expect(pin_for(4.4) < 2950, "at 4.4 V the pin is under the ADC's top");
	expect(abs(battery_cell_mv(pin_for(3.7), 0) - 3700) <= 5, "3.7 V reads back");
	expect(abs(battery_cell_mv(pin_for(4.2), 0) - 4200) <= 5, "4.2 V reads back");
	expect(abs(battery_cell_mv(pin_for(4.4), 0) - 4400) <= 5, "4.4 V reads back");
	expect(battery_cell_mv(pin_for(4.0), 120) - battery_cell_mv(pin_for(4.0), 0) == 120,
		"the offset is added");
	expect(battery_cell_mv(pin_for(3.9), battery_offset_for(pin_for(3.9), 3950)) == 3950,
		"the offset found makes the meter's reading");
	expect(battery_cell_mv(0, 0) == 0, "a pin at 0 V is no cell, not less");
}

// a second of samples of mean size near size, a square wave of it
static void vox_feed(struct vox *v, int size, int ms, int level, int is_held_off) {
	int i;

	for (i = 0; i < ms * 8; i++)
		vox_take(v, (int16_t)(i % 2 ? size : -size), level, is_held_off, 8000);
}

static void test_vox(void) {
	struct vox v;

	vox_begin(&v);
	vox_feed(&v, 20, 3000, 5, 0);
	expect(!vox_is_keyed(&v), "the noise alone does not key it");
	vox_feed(&v, 400, 10, 5, 0);
	expect(!vox_is_keyed(&v), "a click of 10 ms does not key it");
	vox_feed(&v, 20, 500, 5, 0);
	vox_feed(&v, 400, 100, 5, 0);
	expect(vox_is_keyed(&v), "a voice of 100 ms keys it");
	vox_feed(&v, 20, 800, 5, 0);
	expect(vox_is_keyed(&v), "a gap of 800 ms between words holds it");
	vox_feed(&v, 20, 1500, 5, 0);
	expect(!vox_is_keyed(&v), "1.5 s quiet lets it go");
	vox_feed(&v, 400, 200, VOX_OFF, 0);
	expect(!vox_is_keyed(&v), "off, a voice does not key it");
	vox_feed(&v, 400, 200, 5, 1);
	expect(!vox_is_keyed(&v), "held off, a voice does not key it");
	vox_begin(&v);
	vox_feed(&v, 20, 3000, 1, 0);
	vox_feed(&v, 120, 200, 1, 0);
	expect(!vox_is_keyed(&v), "at 1, a quiet voice does not key it");
	vox_begin(&v);
	vox_feed(&v, 20, 3000, 10, 0);
	vox_feed(&v, 120, 200, 10, 0);
	expect(vox_is_keyed(&v), "at 10, the same quiet voice keys it");
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
	test_entry();
	test_halves();
	test_known();
	test_contacts();
	test_speech();
	test_cookie();
	test_stretch();
	test_local_packet();
	test_battery();
	test_vox();
	if (failures == 0)
		printf("test_units: all passed\n");
	return failures;
}
