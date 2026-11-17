// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// text.h — text a word at a time: kept, timed, and played as Morse. See
// text.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define TEXT_PARTY_ROOM 11
#define TEXT_WORD_ROOM 41
#define TEXT_RUNS 32
#define TEXT_RUN_ROOM 120
#define TEXT_PARTIES 24
#define TEXT_PLAY_ROOM 256

// words from one to another, in the order they came
struct text_run {
	char from[TEXT_PARTY_ROOM];
	char to[TEXT_PARTY_ROOM];
	char words[TEXT_RUN_ROOM];
	int length;
};

// a callsign or a room, and when it last spoke or was spoken to
struct text_party {
	char name[TEXT_PARTY_ROOM];
	long long at;
};

struct text {
	struct text_run runs[TEXT_RUNS];
	int count;
	int newest;
	int is_timed;
	long long offset_ms;
	struct text_party parties[TEXT_PARTIES];
	int party_count;
};

// what is being played: a ring of words, each a byte of its speed and its
// letters, and where in it the player is
struct text_play {
	char ring[TEXT_PLAY_ROOM];
	int in;
	int out;
	int wpm;
	const char *code;
	long until_ms;
};

void text_begin(struct text *t);
void text_add(struct text *t, const char *from, const char *to, const char *word);
int text_runs_of(const struct text *t, const char *party, int *found, int most);
const struct text_run *text_run(const struct text *t, int i);
void text_clock_set(struct text *t, long long unix_s, long now_ms);
long long text_clock_now(const struct text *t, long now_ms);
void text_touch(struct text *t, const char *party, long now_ms);
long long text_last(const struct text *t, const char *party);

void text_play_begin(struct text_play *p);
int text_play_add(struct text_play *p, const char *word, int wpm);
int text_play_next(struct text_play *p, long now, int may_sound);
int text_play_is_busy(const struct text_play *p);

#ifdef __cplusplus
}
#endif
