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
// Or the keyboard (the author's, 2026-11-25): three layers, the small
// letters, the capitals and the symbols, each four rows of eleven keys. The
// letter layers have the space, the digits and . , - / @ ! ? too; the
// symbols layer every printable character that is not a letter. The
// layers are one strip: entry_key_move() goes along a row, on from its end
// to the next row's start, from a layer's last key to the next layer's
// first, and round from the last layer to the first, and back the same
// way. Keys not of the set are passed over and not shown, and so is a
// layer that has nothing of the set the others have not (a callsign's is
// all on the capitals). entry_key_take() types the chosen.
//
// It knows nothing of the ESP32, so tests/test_units.c tries it.

#include <stdio.h>
#include <string.h>

#include "entry.h"

static void settle_on_open(struct entry *e, int at);

void entry_begin(struct entry *e, const char *set, const char *text, int most) {
	if (most > ENTRY_ROOM - 1)
		most = ENTRY_ROOM - 1;
	e->most = most;
	snprintf(e->text, (size_t)most + 1, "%s", text);
	e->length = (int)strlen(e->text);
	e->set = set;
	e->set_length = (int)strlen(set);
	e->at = 0;
	// at the small letters' first, or the first open after it
	settle_on_open(e, ENTRY_LOWER * ENTRY_KEYS);
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

// the layers, row by row
static const char *const layers[ENTRY_LAYERS] = {
	"abcdefghijk" "lmnopqrstuv" "wxyz .,-/@?" "0123456789!",
	"ABCDEFGHIJK" "LMNOPQRSTUV" "WXYZ .,-/@?" "0123456789!",
	"!\"#$%&'()*+" ",-./:;<=>?@" "[\\]^_`{|}~ " "0123456789 ",
};

const char *entry_keyboard(int layer) {
	return layers[layer];
}

static int layer_of(int at) {
	return at / ENTRY_KEYS;
}

// the key at, a place on the strip, layer by layer
char entry_key_at(const struct entry *e, int at) {
	(void)e;
	return layers[layer_of(at)][at % ENTRY_KEYS];
}

char entry_key(const struct entry *e) {
	return entry_key_at(e, e->key_at);
}

static int of_set(const struct entry *e, char c) {
	return memchr(e->set, c, (size_t)e->set_length) != NULL;
}

// 1 if layer has a character of the set that neither other layer has
static int layer_adds(const struct entry *e, int layer) {
	const char *k;
	int other;

	for (k = layers[layer]; *k; k++) {
		int is_elsewhere = 0;

		if (!of_set(e, *k))
			continue;
		for (other = 0; other < ENTRY_LAYERS; other++)
			if (other != layer && strchr(layers[other], *k) != NULL)
				is_elsewhere = 1;
		if (!is_elsewhere)
			return 1;
	}
	return 0;
}

// 1 if layer is gone through: it adds to the set, or, with none that
// does, it is the capitals
int entry_layer_is_open(const struct entry *e, int layer) {
	int any = 0;
	int i;

	for (i = 0; i < ENTRY_LAYERS; i++)
		any |= layer_adds(e, i);
	return any ? layer_adds(e, layer) : layer == ENTRY_UPPER;
}

// 1 if the key at, a place on the strip, is to be shown and may be chosen
int entry_key_is_open(const struct entry *e, int at) {
	return entry_layer_is_open(e, layer_of(at)) && of_set(e, entry_key_at(e, at));
}

// what a key shows, if not its character: the space
const char *entry_key_label(char key) {
	return key == ' ' ? "sp" : NULL;
}

// along the strip by -1 or 1, round its ends, on past keys not open
void entry_key_move(struct entry *e, int by) {
	int all = ENTRY_LAYERS * ENTRY_KEYS;
	int i;

	for (i = 0; i < all; i++) {
		e->key_at = (e->key_at + by + all) % all;
		if (entry_key_is_open(e, e->key_at))
			return;
	}
}

// the chosen on the first open key of the strip from at
static void settle_on_open(struct entry *e, int at) {
	e->key_at = at - 1;
	if (e->key_at < 0)
		e->key_at += ENTRY_LAYERS * ENTRY_KEYS;
	entry_key_move(e, 1);
}

// the chosen key typed: 1 if it was, 0 with the text full or it not open
int entry_key_take(struct entry *e) {
	if (!entry_key_is_open(e, e->key_at))
		return 0;
	return entry_put(e, entry_key(e));
}
