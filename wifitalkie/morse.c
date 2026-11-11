// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// morse.c — an iambic keyer on two keys, and the characters it keys.
//
// The screen gives UP as the dit paddle and DOWN as the dah. Held, a
// paddle keys its element again and again, each followed by a space of one
// unit; both held, squeezed, key dits and dahs in turn, starting with the
// one pressed first. A paddle pressed while the other's element is keyed,
// or down in the space after it, is kept and its element keyed next:
// iambic A, with element memory, so a squeeze let go of keys no element
// more than the one keying. The
// unit is MORSE_MS_A_WPM over the words a minute, a dit MORSE_DIT_UNITS
// of it and a dah MORSE_DAH_UNITS.
//
// The elements keyed are kept in code, up to MORSE_MOST, and those past it
// are let go. MORSE_LETTER_UNITS after the last element ended with no
// paddle down, the gap between letters, ends the character: morse_keys()
// gives MORSE_LETTER and the character, or MORSE_UNKNOWN for a code that
// is none, and code is empty again. Each element is given too, MORSE_DIT
// or MORSE_DAH, as it begins, for the sidetone. morse_abort() empties code,
// nothing taken.
//
// It knows nothing of the ESP32, so tests/test_units.c tries it.

#include <string.h>

#include "morse.h"

struct sign {
	char letter;
	const char *code;
};

static const struct sign signs[] = {
	{'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."}, {'E', "."},
	{'F', "..-."}, {'G', "--."}, {'H', "...."}, {'I', ".."}, {'J', ".---"},
	{'K', "-.-"}, {'L', ".-.."}, {'M', "--"}, {'N', "-."}, {'O', "---"},
	{'P', ".--."}, {'Q', "--.-"}, {'R', ".-."}, {'S', "..."}, {'T', "-"},
	{'U', "..-"}, {'V', "...-"}, {'W', ".--"}, {'X', "-..-"}, {'Y', "-.--"},
	{'Z', "--.."},
	{'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
	{'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."},
	{'8', "---.."}, {'9', "----."},
	{'/', "-..-."}, {'?', "..--.."}, {'.', ".-.-.-"}, {',', "--..--"},
	{'-', "-....-"}, {'=', "-...-"}, {'+', ".-.-."}, {'@', ".--.-."},
	{':', "---..."}, {'\'', ".----."}, {'(', "-.--."}, {')', "-.--.-"},
	{'!', "-.-.--"}, {'"', ".-..-."}, {'_', "..--.-"}, {';', "-.-.-."},
};

#define SIGNS ((int)(sizeof(signs) / sizeof(signs[0])))

void morse_abort(struct morse *m) {
	m->code[0] = 0;
	m->length = 0;
}

void morse_begin(struct morse *m, int wpm) {
	if (wpm < MORSE_WPM_LEAST)
		wpm = MORSE_WPM_LEAST;
	if (wpm > MORSE_WPM_MOST)
		wpm = MORSE_WPM_MOST;
	m->unit_ms = MORSE_MS_A_WPM / wpm;
	m->state = MORSE_IDLE;
	m->is_last_dit = 0;
	m->is_dit_kept = 0;
	m->is_dah_kept = 0;
	m->was_dit = 0;
	m->was_dah = 0;
	m->until_ms = 0;
	m->ended_ms = 0;
	morse_abort(m);
}

// the character of code, 0 if it is none
char morse_letter_of(const char *code) {
	int i;

	for (i = 0; i < SIGNS; i++)
		if (!strcmp(signs[i].code, code))
			return signs[i].letter;
	return 0;
}

// the code of letter, a capital, NULL if it has none
const char *morse_code_of(char letter) {
	int i;

	for (i = 0; i < SIGNS; i++)
		if (signs[i].letter == letter)
			return signs[i].code;
	return NULL;
}

// an element begun at now: MORSE_DIT or MORSE_DAH
static int element(struct morse *m, int is_dit, long now) {
	if (m->length < MORSE_MOST) {
		m->code[m->length++] = is_dit ? '.' : '-';
		m->code[m->length] = 0;
	}
	m->state = MORSE_SENDING;
	m->is_last_dit = is_dit;
	// what was kept is keyed now, or passed over for the other
	m->is_dit_kept = 0;
	m->is_dah_kept = 0;
	m->until_ms = now + (is_dit ? MORSE_DIT_UNITS : MORSE_DAH_UNITS) * m->unit_ms;
	return is_dit ? MORSE_DIT : MORSE_DAH;
}

// the paddles as they are now, every turn: MORSE_DIT or MORSE_DAH as an
// element begins, MORSE_LETTER with the character in *letter at the gap
// after one, MORSE_UNKNOWN for a code that is none, and MORSE_NOTHING the
// rest
int morse_keys(struct morse *m, int is_dit, int is_dah, long now, char *letter) {
	// a paddle pressed while the other's element is keyed is kept, and one
	// down in the space after an element
	if (m->state == MORSE_SENDING && !m->is_last_dit && is_dit && !m->was_dit)
		m->is_dit_kept = 1;
	if (m->state == MORSE_SENDING && m->is_last_dit && is_dah && !m->was_dah)
		m->is_dah_kept = 1;
	if (m->state == MORSE_SPACING) {
		m->is_dit_kept |= is_dit;
		m->is_dah_kept |= is_dah;
	}
	m->was_dit = is_dit;
	m->was_dah = is_dah;
	if (m->state == MORSE_SENDING && now >= m->until_ms) {
		m->state = MORSE_SPACING;
		m->ended_ms = m->until_ms;
		m->until_ms += m->unit_ms;
	}
	if (m->state == MORSE_SPACING && now >= m->until_ms) {
		m->state = MORSE_IDLE;
		// the other one first, for a squeeze; then the same again
		if (m->is_last_dit && m->is_dah_kept)
			return element(m, 0, now);
		if (!m->is_last_dit && m->is_dit_kept)
			return element(m, 1, now);
		if (m->is_dit_kept)
			return element(m, 1, now);
		if (m->is_dah_kept)
			return element(m, 0, now);
	}
	if (m->state != MORSE_IDLE)
		return MORSE_NOTHING;
	if (is_dit)
		return element(m, 1, now);
	if (is_dah)
		return element(m, 0, now);
	if (m->length == 0 || now - m->ended_ms < MORSE_LETTER_UNITS * m->unit_ms)
		return MORSE_NOTHING;
	*letter = morse_letter_of(m->code);
	morse_abort(m);
	return *letter ? MORSE_LETTER : MORSE_UNKNOWN;
}
