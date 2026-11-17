// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// text.c — text a word at a time, as the author designed it on 14
// November 2026: kept, timed, and played as Morse.
//
// Words go to a callsign or a room, and come from them, one at a time, as
// CW does on the air (my.sbitx.net passes them on, PROTOCOL.md "Text, a
// word at a time"). They are kept in RAM only, in TEXT_RUNS runs of words
// from one party to another, each up to TEXT_RUN_ROOM; a word from the
// same to the same as the newest run goes on the end of it, and any other
// begins a run, the oldest let go. A screen shows a party's runs, those
// from it or to it (text_runs_of()), each under a header from>to.
//
// The talkie has no clock. my.sbitx.net says the time as it connects,
// TIME <seconds since 1970>, and text_clock_set() keeps how far that is from
// millis(); text_clock_now() adds it back, UTC. Nothing is stamped per
// word: each party keeps only the time it last spoke or was spoken to, by
// voice or text (text_touch(), text_last()), for the menu.
//
// Words heard are played as Morse, at the speed they were sent, through
// struct text_play: text_play_add() puts a word in its ring, and
// text_play_next(), each turn, says how long a tone to sound when an
// element is due, and 0 the rest. Between elements a unit's space, between
// letters three, between words seven, as morse.c keys them. While
// may_sound is 0, the talkie hearing a voice, no element is begun.
//
// It knows nothing of the ESP32, so tests/test_units.c tries it.

#include <string.h>
#include <stdio.h>

#include "morse.h"
#include "text.h"

void text_begin(struct text *t) {
	memset(t, 0, sizeof(*t));
}

// the i'th run, oldest first, of t->count
const struct text_run *text_run(const struct text *t, int i) {
	return &t->runs[(t->newest - t->count + 1 + i + TEXT_RUNS) % TEXT_RUNS];
}

static struct text_run *new_run(struct text *t, const char *from, const char *to) {
	struct text_run *run;

	t->newest = (t->newest + 1) % TEXT_RUNS;
	if (t->count < TEXT_RUNS)
		t->count++;
	run = &t->runs[t->newest];
	snprintf(run->from, sizeof(run->from), "%s", from);
	snprintf(run->to, sizeof(run->to), "%s", to);
	run->words[0] = 0;
	run->length = 0;
	return run;
}

// a word from one to another, on the end of the newest run if it is theirs
// and has room, or in a run of its own
void text_add(struct text *t, const char *from, const char *to, const char *word) {
	struct text_run *run = t->count > 0 ? &t->runs[t->newest] : NULL;
	int length = (int)strlen(word);

	if (length >= TEXT_RUN_ROOM - 1)
		length = TEXT_RUN_ROOM - 2;
	if (run == NULL || strcmp(run->from, from) || strcmp(run->to, to)
			|| run->length + 1 + length >= TEXT_RUN_ROOM)
		run = new_run(t, from, to);
	if (run->length > 0)
		run->words[run->length++] = ' ';
	memcpy(run->words + run->length, word, (size_t)length);
	run->length += length;
	run->words[run->length] = 0;
}

// the runs from party or to it, oldest first, their places in found, up to
// most; how many
int text_runs_of(const struct text *t, const char *party, int *found, int most) {
	int n = 0;
	int i;

	for (i = 0; i < t->count && n < most; i++) {
		const struct text_run *run = text_run(t, i);

		if (!strcmp(run->from, party) || !strcmp(run->to, party))
			found[n++] = i;
	}
	return n;
}

void text_clock_set(struct text *t, long long unix_s, long now_ms) {
	t->offset_ms = unix_s * 1000 - now_ms;
	t->is_timed = 1;
}

// the time, in seconds since 1970, UTC; -1 before it is told
long long text_clock_now(const struct text *t, long now_ms) {
	if (!t->is_timed)
		return -1;
	return (t->offset_ms + now_ms) / 1000;
}

// party spoke or was spoken to now; the one longest quiet let go for a new
// one when there is no room
void text_touch(struct text *t, const char *party, long now_ms) {
	long long at = text_clock_now(t, now_ms);
	int oldest = 0;
	int i;

	if (at < 0)
		return;
	for (i = 0; i < t->party_count; i++) {
		if (!strcmp(t->parties[i].name, party)) {
			t->parties[i].at = at;
			return;
		}
		if (t->parties[i].at < t->parties[oldest].at)
			oldest = i;
	}
	if (t->party_count < TEXT_PARTIES)
		oldest = t->party_count++;
	snprintf(t->parties[oldest].name, sizeof(t->parties[oldest].name), "%s", party);
	t->parties[oldest].at = at;
}

// when party last spoke or was spoken to, 0 if not since the talkie began
long long text_last(const struct text *t, const char *party) {
	int i;

	for (i = 0; i < t->party_count; i++)
		if (!strcmp(t->parties[i].name, party))
			return t->parties[i].at;
	return 0;
}

void text_play_begin(struct text_play *p) {
	memset(p, 0, sizeof(*p));
}

static int ring_used(const struct text_play *p) {
	return (p->in - p->out + TEXT_PLAY_ROOM) % TEXT_PLAY_ROOM;
}

// a word to be played, at wpm; 0 if there is no room for it
int text_play_add(struct text_play *p, const char *word, int wpm) {
	int length = (int)strlen(word);
	int i;

	if (wpm < MORSE_WPM_LEAST)
		wpm = MORSE_WPM_LEAST;
	if (wpm > MORSE_WPM_MOST)
		wpm = MORSE_WPM_MOST;
	if (length == 0 || ring_used(p) + length + 2 >= TEXT_PLAY_ROOM)
		return 0;
	p->ring[p->in] = (char)wpm;
	p->in = (p->in + 1) % TEXT_PLAY_ROOM;
	for (i = 0; i < length; i++) {
		p->ring[p->in] = word[i];
		p->in = (p->in + 1) % TEXT_PLAY_ROOM;
	}
	p->ring[p->in] = 0;
	p->in = (p->in + 1) % TEXT_PLAY_ROOM;
	return 1;
}

// 1 while a word is being played or waits to be
int text_play_is_busy(const struct text_play *p) {
	return p->code != NULL || ring_used(p) > 0;
}

static char ring_take(struct text_play *p) {
	char c = p->ring[p->out];

	p->out = (p->out + 1) % TEXT_PLAY_ROOM;
	return c;
}

// each turn: the length of a tone to sound now, in ms, or 0. A letter's
// elements each sound and are followed by a unit's space; after its last,
// two units more make the space between letters three, or six more make
// the space after a word seven
int text_play_next(struct text_play *p, long now, int may_sound) {
	long unit;
	int ms;
	char c;

	if (now < p->until_ms || !may_sound)
		return 0;
	if (p->code != NULL && *p->code == 0) {
		unit = MORSE_MS_A_WPM / p->wpm;
		p->code = NULL;
		if (ring_used(p) > 0 && p->ring[p->out] == 0) {
			ring_take(p);
			p->wpm = 0;
			p->until_ms = now + 6 * unit;
		} else {
			p->until_ms = now + 2 * unit;
		}
		return 0;
	}
	while (p->code == NULL) {
		if (ring_used(p) == 0)
			return 0;
		if (p->wpm == 0) {
			p->wpm = (unsigned char)ring_take(p);
			continue;
		}
		c = ring_take(p);
		if (c == 0) {
			p->wpm = 0;
			continue;
		}
		if (c >= 'a' && c <= 'z')
			c = (char)(c - 'a' + 'A');
		p->code = morse_code_of(c);
	}
	unit = MORSE_MS_A_WPM / p->wpm;
	ms = (int)((*p->code == '.' ? MORSE_DIT_UNITS : MORSE_DAH_UNITS) * unit);
	p->code++;
	p->until_ms = now + ms + unit;
	return ms;
}
