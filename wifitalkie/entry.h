// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// entry.h — text entered a character at a time with the keys. See entry.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define ENTRY_ROOM 65

#define ENTRY_CALLSIGN "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/"
#define ENTRY_ANY "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" \
	" !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"

// the keyboard (the author's, 2026-11-25): three layers, small letters,
// capitals and symbols, each ENTRY_KEYBOARD_ROWS of ENTRY_KEYBOARD_ACROSS,
// gone through as one strip, a layer's last key on to the next's first
#define ENTRY_KEYBOARD_ACROSS 11
#define ENTRY_KEYBOARD_ROWS 4
#define ENTRY_KEYS (ENTRY_KEYBOARD_ACROSS * ENTRY_KEYBOARD_ROWS)
#define ENTRY_LAYERS 3
#define ENTRY_LOWER 0
#define ENTRY_UPPER 1
#define ENTRY_SYMBOLS 2
// what a key on the keyboard did, for screen.c
#define ENTRY_GOING 0
#define ENTRY_OK 1
#define ENTRY_CANCEL 2

struct entry {
	char text[ENTRY_ROOM];
	int length;
	int most;
	const char *set;
	int set_length;
	int at;
	int key_at;
};

void entry_begin(struct entry *e, const char *set, const char *text, int most);
void entry_scroll(struct entry *e, int up);
int entry_put(struct entry *e, char letter);
int entry_add(struct entry *e);
int entry_delete(struct entry *e);
char entry_around(const struct entry *e, int offset);
const char *entry_keyboard(int layer);
int entry_layer_is_open(const struct entry *e, int layer);
void entry_key_move(struct entry *e, int by);
char entry_key(const struct entry *e);
char entry_key_at(const struct entry *e, int at);
int entry_key_is_open(const struct entry *e, int at);
const char *entry_key_label(char key);
int entry_key_take(struct entry *e);

#ifdef __cplusplus
}
#endif
