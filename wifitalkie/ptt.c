// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// ptt.c — the push-to-talk, the button and the verb, and the lamp.
//
// The talkie speaks while the button is held, or from PTT ON on the console
// until PTT OFF; either is the PTT. The button is read through a debounce:
// it has to have been the same for DEBOUNCE_MS before a change is taken.
//
// A PTT left down is let go after ONE_OVER_MS, PTT ON is undone, and the
// button has to be let go before it speaks again: my.sbitx.net gives a speaker a
// minute and drops the rest, and a talkie that went on sending fifty
// packets a second to nobody would only be using the owner's WiFi and the
// service. PTT ON typed and forgotten is the case this is for.
//
// The lamp is lit while the talkie speaks or hears; voip.c says which.

#include "driver/gpio.h"

#include "console.h"
#include "pins.h"
#include "ptt.h"

#define DEBOUNCE_MS 20
#define ONE_OVER_MS 60000

static int is_verb_down;
static int is_button_down;
static int button_level_was = 1;
static long button_changed_ms;
static int is_down;
static long down_since_ms;
static int is_cut;

void ptt_open(void) {
	gpio_config_t button = {
		.pin_bit_mask = 1ULL << PIN_PTT,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	gpio_config_t lamp = {
		.pin_bit_mask = 1ULL << PIN_LAMP,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};

	gpio_config(&button);
	gpio_config(&lamp);
	ptt_lamp(0);
}

void ptt_set(int is_verb) {
	is_verb_down = is_verb;
}

void ptt_poll(long now) {
	int level = gpio_get_level(PIN_PTT);
	int is_wanted;

	if (level != button_level_was) {
		button_level_was = level;
		button_changed_ms = now;
	}
	if (now - button_changed_ms >= DEBOUNCE_MS)
		is_button_down = level == 0;
	is_wanted = is_verb_down || is_button_down;
	if (!is_wanted)
		is_cut = 0;
	if (is_cut)
		is_wanted = 0;
	if (is_wanted && !is_down)
		down_since_ms = now;
	if (is_wanted && now - down_since_ms >= ONE_OVER_MS) {
		is_cut = 1;
		is_wanted = 0;
		is_verb_down = 0;
		console_say("PTT OFF: ONE MINUTE");
	}
	if (is_wanted && !is_down)
		console_say("PTT ON");
	if (!is_wanted && is_down && !is_cut)
		console_say("PTT OFF");
	is_down = is_wanted;
}

int ptt_is_down(void) {
	return is_down;
}

void ptt_lamp(int is_lit) {
	if (is_lit)
		gpio_set_level(PIN_LAMP, LAMP_LIT_LEVEL);
	else
		gpio_set_level(PIN_LAMP, !LAMP_LIT_LEVEL);
}
