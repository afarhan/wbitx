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

// the keyboard, ENTRY_KEYBOARD_ROWS of ENTRY_KEYBOARD_ACROSS, as the
// author laid it out on 11 November 2026; every key a character, so
// screen.c deletes with DOWN held
#define ENTRY_KEYBOARD_ACROSS 10
#define ENTRY_KEYBOARD_ROWS 4
#define ENTRY_KEYS (ENTRY_KEYBOARD_ACROSS * ENTRY_KEYBOARD_ROWS)
#define ENTRY_KEYBOARD "1234567890" "QWERTYUIOP" "ASDFGHJKL," "ZXCVBNM./?"
#define ENTRY_KEY_FIRST ENTRY_KEYBOARD_ACROSS
// a key moved to is typed once it has been left this long
#define ENTRY_KEY_SETTLE_MS 1000

struct entry {
	char text[ENTRY_ROOM];
	int length;
	int most;
	const char *set;
	int set_length;
	int at;
	int key_at;
	int is_key_pending;
	long key_moved_ms;
};

void entry_begin(struct entry *e, const char *set, const char *text, int most);
void entry_scroll(struct entry *e, int up);
int entry_put(struct entry *e, char letter);
int entry_add(struct entry *e);
int entry_delete(struct entry *e);
char entry_around(const struct entry *e, int offset);
void entry_key_move(struct entry *e, int by);
void entry_key_row(struct entry *e, int by);
char entry_key(const struct entry *e);
int entry_key_is_open(const struct entry *e, int at);
int entry_key_take(struct entry *e);
void entry_key_moved(struct entry *e, long now);
void entry_key_cancel(struct entry *e);
int entry_key_settle(struct entry *e, long now, int is_now);

#ifdef __cplusplus
}
#endif
