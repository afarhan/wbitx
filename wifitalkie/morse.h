// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// morse.h — an iambic keyer on two keys, and the characters it keys. See
// morse.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MORSE_MS_A_WPM 1200
#define MORSE_WPM_LEAST 5
#define MORSE_WPM_MOST 40
#define MORSE_LETTER_UNITS 3
#define MORSE_DIT_UNITS 1
#define MORSE_DAH_UNITS 3
#define MORSE_MOST 7

#define MORSE_NOTHING 0
#define MORSE_DIT 1
#define MORSE_DAH 2
#define MORSE_LETTER 3
#define MORSE_UNKNOWN 4

#define MORSE_IDLE 0
#define MORSE_SENDING 1
#define MORSE_SPACING 2

struct morse {
	char code[MORSE_MOST + 1];
	int length;
	long unit_ms;
	int state;
	int is_last_dit;
	int is_dit_kept;
	int is_dah_kept;
	int was_dit;
	int was_dah;
	long until_ms;
	long ended_ms;
};

void morse_begin(struct morse *m, int wpm);
int morse_keys(struct morse *m, int is_dit, int is_dah, long now, char *letter);
void morse_abort(struct morse *m);
char morse_letter_of(const char *code);
const char *morse_code_of(char letter);

#ifdef __cplusplus
}
#endif
