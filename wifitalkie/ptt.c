// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// ptt.c — the push-to-talk, the button and the verb, and the lamp.
//
// The talkie speaks while the button is held, or from PTT ON on the console
// until PTT OFF; either is the PTT. The button is the ENTER key: keys.c sets
// its pin up and gives its presses to the screen, and this reads its level
// through a debounce of its own: it has to have been the same for
// DEBOUNCE_MS before a change is taken.
//
// A PTT left down is let go after ONE_OVER_MS, PTT ON is undone, and the
// button has to be let go before it speaks again: my.sbitx.net gives a speaker a
// minute and drops the rest, and a talkie that went on sending fifty
// packets a second to nobody would only be using the owner's WiFi and the
// service. PTT ON typed and forgotten is the case this is for.
//
// ALWAYS_SPEAKING, for measuring the noise with the talkie off its USB:
// while the WiFi is up the PTT is held down as if by the verb, minute after
// minute, with ALWAYS_GAP_MS let go between, so that my.sbitx.net hears it
// as overs. It is 0 in a talkie for use.
//
// ENTER speaks only on the home page; on every other page it is the
// screen's, and the screen says so with ptt_lend(). The PTT key, PIN_KEY_PTT
// on the right side, speaks on every page, debounced the same way, and is
// set up here, an input with its pull-up. The verb still speaks.
//
// VOX: the voice keys the talkie as the button does (vox.c, by way of
// microphone.c), with the same minute at most; PTT ON (VOX) says so.
// ptt_is_vox() says the talkie was keyed by the voice, for voip.c to send
// what came just before.
//
// The lamp is lit while the talkie speaks or hears; voip.c says which. It
// was the board's LED; that pin is now the UP key, and the lamp is shown on
// the display.

#include "driver/gpio.h"

#include "console.h"
#include "microphone.h"
#include "network.h"
#include "pins.h"
#include "ptt.h"

#define DEBOUNCE_MS 20
#define ONE_OVER_MS 60000
#define ALWAYS_SPEAKING 0
#define ALWAYS_GAP_MS 2000

static int is_verb_down;
static int is_button_down;
static int button_level_was = 1;
static long button_changed_ms;
static int is_key_down;
static int key_level_was = 1;
static long key_changed_ms;
static int is_key_open;
static int is_down;
static long down_since_ms;
static int is_cut;
static int is_lit;
static long cut_ms;
static int is_lent;
static int is_by_vox;

void ptt_set(int is_verb) {
	is_verb_down = is_verb;
}

static void key_open(void) {
	gpio_config_t config = {
		.pin_bit_mask = 1ULL << PIN_KEY_PTT,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};

	gpio_config(&config);
	is_key_open = 1;
}

void ptt_poll(long now) {
	int level = gpio_get_level(PIN_KEY_ENTER);
	int key_level;
	int is_wanted;

	if (!is_key_open)
		key_open();
	key_level = gpio_get_level(PIN_KEY_PTT);
	if (level != button_level_was) {
		button_level_was = level;
		button_changed_ms = now;
	}
	if (now - button_changed_ms >= DEBOUNCE_MS)
		is_button_down = level == 0;
	if (key_level != key_level_was) {
		key_level_was = key_level;
		key_changed_ms = now;
	}
	if (now - key_changed_ms >= DEBOUNCE_MS)
		is_key_down = key_level == 0;
	is_wanted = is_verb_down || is_key_down || (is_button_down && !is_lent);
	if (!is_wanted && !is_down)
		is_by_vox = microphone_vox();
	is_wanted = is_wanted || microphone_vox();
	if (ALWAYS_SPEAKING && network_is_up()) {
		is_wanted = 1;
		if (is_cut && now - cut_ms >= ALWAYS_GAP_MS)
			is_cut = 0;
	}
	if (!is_wanted)
		is_cut = 0;
	if (is_cut)
		is_wanted = 0;
	if (is_wanted && !is_down)
		down_since_ms = now;
	if (is_wanted && now - down_since_ms >= ONE_OVER_MS) {
		is_cut = 1;
		cut_ms = now;
		is_wanted = 0;
		is_verb_down = 0;
		console_say("PTT OFF: ONE MINUTE");
	}
	if (is_wanted && !is_down)
		if (is_by_vox) {
			// what keyed it, for setting the level: the mic over the threshold
			int size;
			int threshold;

			microphone_vox_meter(&size, &threshold);
			console_say("PTT ON (VOX) MIC %d OVER %d", size, threshold);
		} else {
			console_say("PTT ON");
		}
	if (!is_wanted && is_down && !is_cut)
		console_say("PTT OFF");
	is_down = is_wanted;
}

// 1 if the over now was begun by the voice, not the button or the verb
int ptt_is_vox(void) {
	return is_down && is_by_vox;
}

int ptt_is_down(void) {
	return is_down;
}

void ptt_lamp(int is_lighting) {
	is_lit = is_lighting;
}

int ptt_is_lit(void) {
	return is_lit;
}

// lent back still held, the button has to be let go before it speaks
void ptt_lend(int is_lending) {
	if (is_lent && !is_lending && is_button_down)
		is_cut = 1;
	is_lent = is_lending;
}
