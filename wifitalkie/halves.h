// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// halves.h — a letter chosen from two lines of them with two keys, keyed
// as its Morse is. See halves.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define HALVES_MS_A_WPM 1200
#define HALVES_WPM_LEAST 5
#define HALVES_WPM_MOST 40
#define HALVES_LETTER_UNITS 3
#define HALVES_FIGURES_HELD_MS 1000
#define HALVES_ACROSS 15
#define HALVES_DITS "HSVIFU?ELR.APWJ"
#define HALVES_DAHS "BDXNCKYTZGQM/O-"
#define HALVES_FIGURES ";:(9876543210)_,"
#define HALVES_FIGURES_ACROSS 16
#define HALVES_FIGURES_FIRST 12

struct halves {
	const char *lines[2];
	int line;
	int at;
	int step;
	int is_figures;
	long unit_ms;
	int was_up;
	int was_down;
	long down_ms;
	long let_go_ms;
};

void halves_begin(struct halves *h, int wpm);
int halves_keys(struct halves *h, int is_up, int is_down, long now, char *letter);
int halves_offered(const struct halves *h, int line, int at);
void halves_abort(struct halves *h);

#ifdef __cplusplus
}
#endif
