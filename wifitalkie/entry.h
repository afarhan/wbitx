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

struct entry {
	char text[ENTRY_ROOM];
	int length;
	int most;
	const char *set;
	int set_length;
	int at;
};

void entry_begin(struct entry *e, const char *set, const char *text, int most);
void entry_scroll(struct entry *e, int up);
int entry_put(struct entry *e, char letter);
int entry_add(struct entry *e);
int entry_delete(struct entry *e);
char entry_around(const struct entry *e, int offset);

#ifdef __cplusplus
}
#endif
