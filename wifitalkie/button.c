// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// button.c — what a key's presses mean, from its level over time.
//
// A key is read each turn of the loop, pressed or not, and a change is
// believed once it has held BUTTON_DEBOUNCE_MS, as ptt.c does the PTT. What
// a press says depends on the key's kind:
//
//     BUTTON_PLAIN       pressed, as it goes down (ENTER)
//     BUTTON_REPEATING   pressed as it goes down, and repeated while it is
//                        held, after BUTTON_REPEAT_AFTER_MS, every
//                        BUTTON_REPEAT_EVERY_MS, and every
//                        BUTTON_FAST_EVERY_MS once it has been held
//                        BUTTON_FAST_AFTER_MS (UP and DOWN)
//     BUTTON_HOLDING     pressed as it comes up, if it was down less than
//                        BUTTON_HELD_MS; held, once, when it has been down
//                        that long, and then nothing as it comes up (MENU,
//                        which is BACK pressed and HOME held)
//     BUTTON_HOLDING_REPEATING   as BUTTON_HOLDING, but after held it is
//                        repeated every BUTTON_HELD_EVERY_MS while it
//                        stays down (ENTER, which held takes characters
//                        off one after another)
//
// It knows nothing of the ESP32, so tests/test_units.c tries it.

#include "button.h"

void button_begin(struct button *b, int kind) {
	b->kind = kind;
	b->was_pressed = 0;
	b->changed_ms = 0;
	b->is_down = 0;
	b->down_ms = 0;
	b->repeat_ms = 0;
	b->is_held_given = 0;
}

static int is_holding(const struct button *b) {
	return b->kind == BUTTON_HOLDING || b->kind == BUTTON_HOLDING_REPEATING;
}

static int going_down(struct button *b, long now) {
	b->is_down = 1;
	b->down_ms = now;
	b->repeat_ms = now + BUTTON_REPEAT_AFTER_MS;
	b->is_held_given = 0;
	if (is_holding(b))
		return BUTTON_NOTHING;
	return BUTTON_PRESSED;
}

static int coming_up(struct button *b) {
	b->is_down = 0;
	if (is_holding(b) && !b->is_held_given)
		return BUTTON_PRESSED;
	return BUTTON_NOTHING;
}

static int staying_down(struct button *b, long now) {
	if (b->kind == BUTTON_REPEATING && now >= b->repeat_ms) {
		if (now - b->down_ms >= BUTTON_FAST_AFTER_MS)
			b->repeat_ms += BUTTON_FAST_EVERY_MS;
		else
			b->repeat_ms += BUTTON_REPEAT_EVERY_MS;
		return BUTTON_REPEATED;
	}
	if (is_holding(b) && !b->is_held_given && now - b->down_ms >= BUTTON_HELD_MS) {
		b->is_held_given = 1;
		b->repeat_ms = now + BUTTON_HELD_EVERY_MS;
		return BUTTON_HELD;
	}
	if (b->kind == BUTTON_HOLDING_REPEATING && b->is_held_given && now >= b->repeat_ms) {
		b->repeat_ms += BUTTON_HELD_EVERY_MS;
		return BUTTON_REPEATED;
	}
	return BUTTON_NOTHING;
}

int button_step(struct button *b, int is_pressed, long now) {
	if (is_pressed != b->was_pressed) {
		b->was_pressed = is_pressed;
		b->changed_ms = now;
	}
	if (now - b->changed_ms < BUTTON_DEBOUNCE_MS)
		return BUTTON_NOTHING;
	if (is_pressed && !b->is_down)
		return going_down(b, now);
	if (!is_pressed && b->is_down)
		return coming_up(b);
	if (is_pressed)
		return staying_down(b, now);
	return BUTTON_NOTHING;
}

// how long the key has been down, 0 if it is up
long button_held_ms(const struct button *b, long now) {
	if (!b->is_down)
		return 0;
	return now - b->down_ms;
}
