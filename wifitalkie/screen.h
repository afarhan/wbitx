// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// screen.h — what the display shows and what the keys do. See screen.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void screen_open(void);
void screen_poll(long now);
void screen_describe(char *text, int room);
void screen_time_heard(long long unix_s, long now);
void screen_text_heard(const char *from, const char *to, int wpm, const char *word, long now);
void screen_text_sent(int count, long now);
void screen_text_refused(const char *why, long now);
void screen_update_shown(void);

#ifdef __cplusplus
}
#endif
