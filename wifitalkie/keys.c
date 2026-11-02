// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// keys.c — the front panel's four keys: UP, DOWN, MENU and ENTER.
//
// Each is a key to ground, with the C3's pull-up; button.c says what its
// presses mean. UP and DOWN repeat while held; MENU is BACK when pressed and
// HOME when held; ENTER is a press, given as it comes up, and held, which
// takes a character off while text is entered, and goes on taking them off
// while it is held. On the home page ENTER is the PTT as well; ptt.c reads
// its level. keys_poll() gives at most one press a
// turn of the loop; two keys' presses in the same millisecond give the
// first's, with how long its key has been held.

#include <stdio.h>

#include "driver/gpio.h"

#include "keys.h"
#include "pins.h"

static const int pins[KEYS] = {PIN_KEY_UP, PIN_KEY_DOWN, PIN_KEY_MENU, PIN_KEY_ENTER};
static const int kinds[KEYS] = {BUTTON_REPEATING, BUTTON_REPEATING, BUTTON_HOLDING, BUTTON_HOLDING_REPEATING};
static const char *names[KEYS] = {"UP", "DOWN", "MENU", "ENTER"};
static struct button buttons[KEYS];

void keys_open(void) {
	gpio_config_t config = {
		.pin_bit_mask = 0,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	int key;

	for (key = 0; key < KEYS; key++) {
		config.pin_bit_mask |= 1ULL << pins[key];
		button_begin(&buttons[key], kinds[key]);
	}
	gpio_config(&config);
}

int keys_poll(long now, struct key_press *press) {
	int found = 0;
	int what;
	int key;

	for (key = 0; key < KEYS; key++) {
		what = button_step(&buttons[key], gpio_get_level(pins[key]) == 0, now);
		if (what == BUTTON_NOTHING || found)
			continue;
		press->key = key;
		press->what = what;
		press->held_ms = button_held_ms(&buttons[key], now);
		found = 1;
	}
	return found;
}

const char *keys_name(int key) {
	return names[key];
}

// each key's pin as it is now, DOWN or up, for the verb KEYS:
// a key that says DOWN while nobody presses it is stuck or shorted
void keys_describe(char *text, int room) {
	int used = 0;
	int key;

	for (key = 0; key < KEYS; key++) {
		if (gpio_get_level(pins[key]) == 0)
			used += snprintf(text + used, room - used, "%s DOWN ", names[key]);
		else
			used += snprintf(text + used, room - used, "%s up ", names[key]);
	}
}
