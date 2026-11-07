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
