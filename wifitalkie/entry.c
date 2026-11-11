// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// entry.c — text entered a character at a time with the keys.
//
// The characters that may be entered are a set, a string in the order they
// are scrolled through: ENTRY_CALLSIGN, the capitals, the digits and the
// stroke, for a callsign; ENTRY_ANY, all of printable ASCII with the small
// letters first, for a WiFi network's key. One of the set is the chosen,
// the one shown in the middle of the screen with its neighbours on either
// side; UP and DOWN scroll it, round from the end to the start. The text
// so far is kept with the chosen added to its end, or its last character
// taken off, up to most characters. screen.c says which key does which.
//
// Or the keyboard, ENTRY_KEYBOARD, forty keys in four rows of ten, with a
// key chosen, key_at: entry_key_move() goes along them one at a time, on
// from the end of a row to the start of the next and round from the last
// to the first, and entry_key_row() goes a row down or up, round from the
// bottom to the top and the top to the bottom. A key moved to is typed
// once it has been left ENTRY_KEY_SETTLE_MS (entry_key_moved(),
// entry_key_settle()). A character from the chosen to any other is so at
// most one or two rows down and five along, five presses at the most and
// about four on the mean, where along all forty would be ten on the mean.
// entry_key_take() adds the chosen to the text, if it is of the set; a
// key not of the set is passed over, and screen.c leaves it out.
//
// It knows nothing of the ESP32, so tests/test_units.c tries it.

#include <stdio.h>
#include <string.h>

#include "entry.h"

void entry_begin(struct entry *e, const char *set, const char *text, int most) {
	if (most > ENTRY_ROOM - 1)
		most = ENTRY_ROOM - 1;
	e->most = most;
	snprintf(e->text, (size_t)most + 1, "%s", text);
	e->length = (int)strlen(e->text);
	e->set = set;
	e->set_length = (int)strlen(set);
	e->at = 0;
	e->key_at = ENTRY_KEY_FIRST;
	e->is_key_pending = 0;
	e->key_moved_ms = 0;
}

void entry_scroll(struct entry *e, int up) {
	if (up)
		e->at = (e->at + 1) % e->set_length;
	else
		e->at = (e->at + e->set_length - 1) % e->set_length;
}

// 1 if letter was added, 0 with the text full
int entry_put(struct entry *e, char letter) {
	if (e->length >= e->most)
		return 0;
	e->text[e->length++] = letter;
	e->text[e->length] = 0;
	return 1;
}

// 1 if the chosen was added, 0 with the text full
int entry_add(struct entry *e) {
	return entry_put(e, e->set[e->at]);
}

// 1 if the last character was taken off, 0 with the text empty
int entry_delete(struct entry *e) {
	if (e->length == 0)
		return 0;
	e->text[--e->length] = 0;
	return 1;
}

// the character offset places from the chosen, round the set
char entry_around(const struct entry *e, int offset) {
	int at = (e->at + offset) % e->set_length;

	if (at < 0)
		at += e->set_length;
	return e->set[at];
}

// 1 if the keyboard's key at is of the set, to be shown and taken
int entry_key_is_open(const struct entry *e, int at) {
	return memchr(e->set, ENTRY_KEYBOARD[at], (size_t)e->set_length) != NULL;
}

// by keys along, -1 or 1, on past those not of the set
void entry_key_move(struct entry *e, int by) {
	int i;

	for (i = 0; i < ENTRY_KEYS; i++) {
		e->key_at = (e->key_at + by + ENTRY_KEYS) % ENTRY_KEYS;
		if (entry_key_is_open(e, e->key_at))
			return;
	}
}

// a row down, by 1, or up, by -1, and on past a key not of the set
void entry_key_row(struct entry *e, int by) {
	int i;

	for (i = 0; i < ENTRY_KEYBOARD_ROWS; i++) {
		e->key_at = (e->key_at + by * ENTRY_KEYBOARD_ACROSS + ENTRY_KEYS) % ENTRY_KEYS;
		if (entry_key_is_open(e, e->key_at))
			return;
	}
}

char entry_key(const struct entry *e) {
	return ENTRY_KEYBOARD[e->key_at];
}

// the chosen key added to the text: 1 if it was, 0 with the text full or
// the key not of the set
int entry_key_take(struct entry *e) {
	if (!entry_key_is_open(e, e->key_at))
		return 0;
	return entry_put(e, entry_key(e));
}

// the chosen key moved to at now: it is typed once it has been left
// ENTRY_KEY_SETTLE_MS
void entry_key_moved(struct entry *e, long now) {
	e->is_key_pending = 1;
	e->key_moved_ms = now;
}

// the key moved to is not to be typed
void entry_key_cancel(struct entry *e) {
	e->is_key_pending = 0;
}

// the key moved to typed, if it has been left long enough or is_now:
// 1 if it was
int entry_key_settle(struct entry *e, long now, int is_now) {
	if (!e->is_key_pending)
		return 0;
	if (!is_now && now - e->key_moved_ms < ENTRY_KEY_SETTLE_MS)
		return 0;
	e->is_key_pending = 0;
	return entry_key_take(e);
}
