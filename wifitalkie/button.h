// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// button.h — what a key's presses mean, from its level over time. See
// button.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define BUTTON_NOTHING 0
#define BUTTON_PRESSED 1
#define BUTTON_REPEATED 2
#define BUTTON_HELD 3

#define BUTTON_PLAIN 0
#define BUTTON_REPEATING 1
#define BUTTON_HOLDING 2

#define BUTTON_DEBOUNCE_MS 20
#define BUTTON_REPEAT_AFTER_MS 500
#define BUTTON_REPEAT_EVERY_MS 250
#define BUTTON_FAST_AFTER_MS 2000
#define BUTTON_FAST_EVERY_MS 50
#define BUTTON_HELD_MS 1000

struct button {
	int kind;
	int was_pressed;
	long changed_ms;
	int is_down;
	long down_ms;
	long repeat_ms;
	int is_held_given;
};

void button_begin(struct button *b, int kind);
int button_step(struct button *b, int is_pressed, long now);
long button_held_ms(const struct button *b, long now);

#ifdef __cplusplus
}
#endif
