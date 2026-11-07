// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// halves.c — a letter chosen from two lines of them with two keys, keyed
// as its Morse is.
//
// The lines are the Morse tree written out in order, dits to the left, of
// the letters of up to four elements: HALVES_DITS is E's side of the tree
// and HALVES_DAHS T's, HALVES_ACROSS each, E and T in their middles. A
// press of UP is a dit and of DOWN a dah, one element a press, however
// long it is held.
//
// Nothing is chosen at first, and the two offered are E and T, the middles
// of the lines: a dit chooses E, a dah T. Then the two offered are those
// either side of the chosen on its line, at step, which starts at a quarter
// of the line, four, and halves with each element: after E, I and A; after
// I, S and U; after U, F and ?. A dit chooses the left of the two, a dah
// the right. Once step is down to nothing, nothing more is offered, and an
// element does nothing; but a fifth dah, -----, Morse's 0, puts the two
// lines away for one, HALVES_FIGURES, its 0 chosen, along which a dit goes
// a character left and a dah one right. DOWN held HALVES_FIGURES_HELD_MS
// goes there at once, whatever was chosen.
//
// The unit is HALVES_MS_A_WPM over the words a minute. HALVES_LETTER_UNITS
// of it after the last key was let go, the gap between letters, and the
// chosen is taken and nothing is chosen again. halves_abort() drops the
// chosen, with nothing taken.
//
// screen.c shows the lines and adds what is taken to the text. It knows
// nothing of the ESP32, so tests/test_units.c tries it.

#include "halves.h"

#define FIRST_STEP ((HALVES_ACROSS + 1) / 4)

void halves_abort(struct halves *h) {
	h->at = -1;
	h->step = 0;
	h->is_figures = 0;
}

void halves_begin(struct halves *h, int wpm) {
	if (wpm < HALVES_WPM_LEAST)
		wpm = HALVES_WPM_LEAST;
	if (wpm > HALVES_WPM_MOST)
		wpm = HALVES_WPM_MOST;
	h->lines[0] = HALVES_DITS;
	h->lines[1] = HALVES_DAHS;
	h->line = 0;
	h->unit_ms = HALVES_MS_A_WPM / wpm;
	h->was_up = 0;
	h->was_down = 0;
	h->down_ms = 0;
	h->let_go_ms = 0;
	halves_abort(h);
}

// the chosen moved by one element
static void move(struct halves *h, int is_dit) {
	if (h->at < 0) {
		h->line = is_dit ? 0 : 1;
		h->at = HALVES_ACROSS / 2;
		h->step = FIRST_STEP;
	} else if (h->is_figures) {
		h->at += is_dit ? -1 : 1;
		if (h->at < 0)
			h->at = 0;
		if (h->at >= HALVES_FIGURES_ACROSS)
			h->at = HALVES_FIGURES_ACROSS - 1;
	} else if (h->step > 0) {
		h->at += is_dit ? -h->step : h->step;
		h->step /= 2;
	} else if (!is_dit && h->line == 1 && h->at == HALVES_ACROSS - 1) {
		h->is_figures = 1;
		h->at = HALVES_FIGURES_FIRST;
	}
}

// the keys as they are now, every turn; 1 with a letter taken
int halves_keys(struct halves *h, int is_up, int is_down, long now, char *letter) {
	if (is_up && !h->was_up)
		move(h, 1);
	if (is_down && !h->was_down) {
		move(h, 0);
		h->down_ms = now;
	}
	if (is_down && !h->is_figures && now - h->down_ms >= HALVES_FIGURES_HELD_MS) {
		h->is_figures = 1;
		h->at = HALVES_FIGURES_FIRST;
	}
	if ((h->was_up && !is_up) || (h->was_down && !is_down))
		h->let_go_ms = now;
	h->was_up = is_up;
	h->was_down = is_down;
	if (is_up || is_down || h->at < 0 || now - h->let_go_ms < HALVES_LETTER_UNITS * h->unit_ms)
		return 0;
	*letter = h->is_figures ? HALVES_FIGURES[h->at] : h->lines[h->line][h->at];
	halves_abort(h);
	return 1;
}

// 1 if the character at on line is one that the next element may choose;
// line is 0 for the figures
int halves_offered(const struct halves *h, int line, int at) {
	if (h->at < 0)
		return !h->is_figures && at == HALVES_ACROSS / 2;
	if (h->is_figures)
		return at == h->at - 1 || at == h->at + 1;
	if (h->step == 0 || line != h->line)
		return 0;
	return at == h->at - h->step || at == h->at + h->step;
}
